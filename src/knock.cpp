#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

/*
 * This program detects knock patterns and communicates with a FastAPI server.
 *
 * Hardware connections:
 * - GPIO36 (ADC1_CH0): Piezo sensor (connected to ground with a 1MΩ pulldown resistor)
 * - GPIO21: Programming switch to enter a new code (short this pin to enter programming mode)
 * - GPIO16: Red LED indicator
 * - GPIO17: Green LED indicator
 */

// Pin definitions
const int knockSensorPin = 36;  // Piezo sensor connected to GPIO36 (ADC1_CH0)
const int programSwitchPin = 21; // Programming mode switch connected to GPIO21
const int redLedPin = 16;       // Red LED connected to GPIO16
const int greenLedPin = 17;     // Green LED connected to GPIO17

// Configuration constants
int knockThreshold = 100;          // Minimum signal from the piezo to register as a knock
int knockFadeTime = 150;         // Milliseconds to allow a knock to fade
const int maxKnocks = 20;              // Maximum number of knocks to listen for
int defaultKnockTimeout = 1200;  // Default time to wait for a knock sequence

// Variables
int knockTimes[maxKnocks];                             // Array to store knock intervals
int sensorValue = 0;                                   // Last reading of the knock sensor
bool isProgrammingMode = false;                        // Flag for programming mode

// WiFi credentials
const char* ssid = "Your SS-ID";         // Replace with your SSID
const char* password = "password"; // Replace with your Wi-Fi password

// Server details
const char* serverName = "http://192.168.0.105:7500"; // Replace with your server's IP and port

// Function Prototypes
void setupWiFi();
void listenToKnocks(int timeout);
void sendKnockSequence(int* knocks, int count, int timeout);
void updateConfigurations(JsonObject config);

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
}

void loop()
{
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
        listenToKnocks(defaultKnockTimeout);
    }
}

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

    Serial.println("\nWiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
}

// Function to record and process knock sequences
void listenToKnocks(int timeout)
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
    } while ((currentTime - startTime < timeout) && (knockCount < maxKnocks));

    // After recording the knock sequence, send it to the server
    sendKnockSequence(knockTimes, knockCount, timeout);
}

void sendKnockSequence(int* knocks, int count, int timeout)
{
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        String serverPath = String(serverName) + "/knock";

        // Build JSON payload
        StaticJsonDocument<256> doc;
        doc["timeout"] = timeout;

        JsonArray knockSequence = doc.createNestedArray("knock_sequence");
        for (int i = 0; i < count; i++) {
            knockSequence.add(knocks[i]);
        }

        String jsonPayload;
        serializeJson(doc, jsonPayload);

        http.begin(serverPath);
        http.addHeader("Content-Type", "application/json");

        int httpResponseCode = http.POST(jsonPayload);

        if (httpResponseCode > 0) {
            String response = http.getString();
            Serial.println("Response:");
            Serial.println(response);

            // Handle response
            StaticJsonDocument<512> responseDoc;
            DeserializationError error = deserializeJson(responseDoc, response);
            if (!error) {
                // Check if there are configuration updates
                JsonObject config = responseDoc["config"];
                if (!config.isNull()) {
                    updateConfigurations(config);
                }

            } else {
                Serial.println("Failed to parse response JSON.");
            }
        } else {
            Serial.print("Error on sending POST: ");
            Serial.println(httpResponseCode);
        }
        http.end();
    } else {
        Serial.println("WiFi not connected");
    }
}

void updateConfigurations(JsonObject config) {
    if (config.containsKey("knockThreshold")) {
        knockThreshold = config["knockThreshold"];
        Serial.printf("Updated knockThreshold to %d\n", knockThreshold);
    }
    if (config.containsKey("knockFadeTime")) {
        knockFadeTime = config["knockFadeTime"];
        Serial.printf("Updated knockFadeTime to %d\n", knockFadeTime);
    }
    if (config.containsKey("defaultKnockTimeout")) {
        defaultKnockTimeout = config["defaultKnockTimeout"];
        Serial.printf("Updated defaultKnockTimeout to %d\n", defaultKnockTimeout);
    }
    if (config.containsKey("isProgrammingMode")) {
        isProgrammingMode = config["isProgrammingMode"];
        Serial.printf("Updated isProgrammingMode to %s\n", isProgrammingMode ? "true" : "false");
    }
}