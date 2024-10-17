#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
/*
 * This program detects knock patterns
 *
 * Hardware connections:
 * - GPIO36 (ADC1_CH0): Piezo sensor (connected to ground with a 1MΩ pulldown resistor)
 * - GPIO2: Programming switch to enter a new code (short this pin to enter programming mode)
 * - GPIO16: Red LED indicator
 * - GPIO17: Green LED indicator
 */

// Pin definitions
const int knockSensorPin = 36;  // Piezo sensor connected to GPIO36 (ADC1_CH0)
const int programSwitchPin = 21; // Programming mode switch connected to GPIO2
const int redLedPin = 16;       // Red LED connected to GPIO16
const int greenLedPin = 17;     // Green LED connected to GPIO17

// Configuration constants
const int knockThreshold = 25;          // Minimum signal from the piezo to register as a knock
const int individualRejectMargin = 25; // Acceptable deviation percentage for individual knocks
const int averageRejectMargin = 15;    // Acceptable average deviation percentage for the knock sequence
const int knockFadeTime = 150;         // Milliseconds to allow a knock to fade
const int maxKnocks = 20;              // Maximum number of knocks to listen for
const int knockTimeout = 1200;         // Maximum time to wait for a knock sequence

// Variables
int secretCode[maxKnocks] = {50, 25, 25, 50, 100, 50}; // Initial knock pattern
int knockTimes[maxKnocks];                             // Array to store knock intervals
int sensorValue = 0;                                   // Last reading of the knock sensor
bool isProgrammingMode = false;                        // Flag for programming mode

// WiFi credentials
const char* ssid = "Your_SSID";
const char* password = "Your_Password";

// MQTT Broker IP and Port
IPAddress mqttBroker(192, 168, 0, 118); // Replace with your broker's IP
const int mqttPort = 1900;

// MQTT Client
WiFiClient espClient;
PubSubClient client(espClient);

// Sensor ID
const char* sensorID = "knock_sensor_1";

// Function Prototypes
void setupWiFi();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void reconnectMQTT();
void publishKnockSequence(int* knocks, int count);
void publishValidationResult(bool success);
void listenToKnocks();
bool validateKnock();
void triggerUnlock();

void setup()
{
    pinMode(redLedPin, OUTPUT);
    pinMode(greenLedPin, OUTPUT);
    pinMode(programSwitchPin, INPUT_PULLUP); // Use internal pull-up resistor

    Serial.begin(115200);
    Serial.println("Program started.");

    digitalWrite(greenLedPin, HIGH); // Green LED on, system is ready

    // Initialize WiFi
    setupWiFi();

    // Initialize MQTT
    client.setServer(mqttBroker, mqttPort);
    client.setCallback(mqttCallback);
}

void loop()
{
    if (!client.connected()) {
        reconnectMQTT();
    }
    client.loop();


    // Read the knock sensor value
    sensorValue = analogRead(knockSensorPin);

    // Check if we are in programming mode
    if (digitalRead(programSwitchPin) == LOW)
    { // Active LOW due to pull-up resistor
        isProgrammingMode = true;
        digitalWrite(redLedPin, HIGH); // Turn on the red LED
    }
    else
    {
        isProgrammingMode = false;
        digitalWrite(redLedPin, LOW);
    }

    // If the sensor value exceeds the threshold, start listening for the knock pattern
    if (sensorValue >= knockThreshold)
    {
        listenToKnocks();
    }
}
///////////// wifi logic //////////////////////

// Function to connect to WiFi
void setupWiFi() {
    delay(10);
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);

    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
}

// MQTT callback (not used but required)
void mqttCallback(char* topic, byte* payload, unsigned int length) {
    // Handle messages received (if any)
}

void reconnectMQTT() {
    // Loop until we're reconnected
    while (!client.connected()) {
        Serial.print("Attempting MQTT connection...");
        // Attempt to connect
        if (client.connect(sensorID)) {
            Serial.println("connected");
        } else {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.println(" try again in 5 seconds");
            delay(5000);
        }
    }
}

///////////////// wifi logic ////////////////////////////

// Function to record and process knock sequences
void listenToKnocks()
{
    Serial.println("Listening for knocks...");

    // Reset the knock times array
    memset(knockTimes, 0, sizeof(knockTimes));

    int knockCount = 0;                 // Number of knocks recorded
    unsigned long startTime = millis(); // Reference for when this knock started
    unsigned long currentTime = millis();

    // Blink the LEDs as a visual indicator of the knock
    digitalWrite(greenLedPin, LOW);
    if (isProgrammingMode)
    {
        digitalWrite(redLedPin, LOW);
    }
    delay(knockFadeTime);
    digitalWrite(greenLedPin, HIGH);
    if (isProgrammingMode)
    {
        digitalWrite(redLedPin, HIGH);
    }
    // loop to register a new knock 
    do
    {
        // Listen for the next knock or wait for it to timeout
        sensorValue = analogRead(knockSensorPin);
        if (sensorValue >= knockThreshold)
        { // Another knock detected
            // Record the delay time
            Serial.println("Knock detected.");
            Serial.println(sensorValue);
            currentTime = millis();
            knockTimes[knockCount] = currentTime - startTime;
            knockCount++; // Increment the counter
            startTime = currentTime;

            // Reset our timer for the next knock
            digitalWrite(greenLedPin, LOW);
            if (isProgrammingMode)
            {
                digitalWrite(redLedPin, LOW);
            }
            delay(knockFadeTime); // A little delay to let the knock decay
            digitalWrite(greenLedPin, HIGH);
            if (isProgrammingMode)
            {
                digitalWrite(redLedPin, HIGH);
            }
        }
        currentTime = millis();
        // Check if we timed out or ran out of knocks
    } while ((currentTime - startTime < knockTimeout) && (knockCount < maxKnocks));

    // We've got our knock recorded; let's see if it's valid
    if (!isProgrammingMode)
    { // If we're not in programming mode
        if (validateKnock())
        {
            publishValidationResult(true);
            triggerUnlock();
        }
        else
        {
            publishValidationResult(false);
            Serial.println("Secret knock failed.");
            digitalWrite(greenLedPin, LOW); // We didn't unlock, blink the red LED as visual feedback
            for (int i = 0; i < 4; i++)
            {
                digitalWrite(redLedPin, HIGH);
                delay(100);
                digitalWrite(redLedPin, LOW);
                delay(100);
            }
            digitalWrite(greenLedPin, HIGH);
        }
    }
    else
    { // If we're in programming mode, we still validate the knock but don't unlock
        validateKnock();
        // Blink the green and red LEDs alternately to show that programming is complete
        Serial.println("New lock stored.");
        digitalWrite(redLedPin, LOW);
        digitalWrite(greenLedPin, HIGH);
        for (int i = 0; i < 3; i++)
        {
            delay(100);
            digitalWrite(redLedPin, HIGH);
            digitalWrite(greenLedPin, LOW);
            delay(100);
            digitalWrite(redLedPin, LOW);
            digitalWrite(greenLedPin, HIGH);
        }
    }

// After recording the knock sequence
publishKnockSequence(knockTimes, knockCount);

}

// Function to unlock the door: insert the code for the Nuki web API here.
void triggerUnlock()
{
    // for now we just light the LED to signify the door being unlocked
    Serial.println("Door unlocked!");
    
    digitalWrite(greenLedPin, HIGH);


    // Blink the green LED a few times for more visual feedback

    for (int i = 0; i < 5; i++)
    {
        digitalWrite(greenLedPin, LOW);
        delay(100);
        digitalWrite(greenLedPin, HIGH);
        delay(100);
    }
}

// Function to validate the recorded knock sequence
bool validateKnock()
{
    int currentKnockCount = 0;
    int secretKnockCount = 0;
    int maxInterval = 0; // We use this later to normalize the times

    // Count the number of knocks and find the maximum interval
    for (int i = 0; i < maxKnocks; i++)
    {
        if (knockTimes[i] > 0)
        {
            currentKnockCount++;
        }
        if (secretCode[i] > 0)
        {
            secretKnockCount++;
        }
        if (knockTimes[i] > maxInterval)
        { // Collect normalization data while looping
            maxInterval = knockTimes[i];
        }
    }

    // If we're recording a new knock, save the info and exit
    if (isProgrammingMode)
    {
        // normalize the times 
        // maxinterval (upper limit) represents 100 
        for (int i = 0; i < maxKnocks; i++)
        { // Normalize the times 
            secretCode[i] = map(knockTimes[i], 0, maxInterval, 0, 100);
        }
        // Flash the lights in the recorded pattern to indicate it's been programmed
        // visual feedback
        digitalWrite(greenLedPin, LOW);
        digitalWrite(redLedPin, LOW);
        delay(1000);
        digitalWrite(greenLedPin, HIGH);
        digitalWrite(redLedPin, HIGH);
        delay(50);
        for (int i = 0; i < maxKnocks; i++)
        {
            digitalWrite(greenLedPin, LOW);
            digitalWrite(redLedPin, LOW);
            // Only turn it on if there's a delay
            if (secretCode[i] > 0)
            {
                delay(map(secretCode[i], 0, 100, 0, maxInterval)); // Expand the time back out to what it was, roughly
                digitalWrite(greenLedPin, HIGH);
                digitalWrite(redLedPin, HIGH);
            }
            delay(50);
        }
        return false; // We don't unlock the door when we are recording a new knock
    }

    // Check if the number of knocks matches, otherwise already return false
    if (currentKnockCount != secretKnockCount)
    {
        return false;
    }

    // Compare the relative intervals of our knocks
    int totalTimeDifference = 0;
    int timeDifference = 0;
    for (int i = 0; i < maxKnocks; i++)
    { // Normalize the times
        knockTimes[i] = map(knockTimes[i], 0, maxInterval, 0, 100);
        timeDifference = abs(knockTimes[i] - secretCode[i]);
        if (timeDifference > individualRejectMargin)
        { // Individual value too far off
            return false;
        }
        totalTimeDifference += timeDifference;
    }
    // Fail if the whole sequence is too inaccurate
    if ((totalTimeDifference / secretKnockCount) > averageRejectMargin)
    {
        return false;
    }
    return true;
}

// builds a JSON object containing a knock sequence, sensor ID, and timestamp, then publishes it to an MQTT topic.
void publishKnockSequence(int* knocks, int count) {
    StaticJsonDocument<256> doc;
    doc["sensor_id"] = sensorID;
    doc["timestamp"] = millis();

    JsonArray knockSequence = doc.createNestedArray("knock_sequence");
    for (int i = 0; i < count; i++) {
        knockSequence.add(knocks[i]);
    }

    char payload[256];
    serializeJson(doc, payload);

    client.publish("knock/sensor/data", payload);
}
//creates a JSON-like string payload that contains a sensor ID, a timestamp, and a validation status
// (either "success" or "failure"). It then publishes this payload to the MQTT topic knock/sensor/status
void publishValidationResult(bool success) {
    StaticJsonDocument<128> doc;
    doc["sensor_id"] = sensorID;
    doc["timestamp"] = millis();
    doc["validation"] = success ? "success" : "failure";

    char payload[128];
    serializeJson(doc, payload);

    client.publish("knock/sensor/status", payload);
}






