#include <Arduino.h>

/*
 * This program detects knock patterns and activates a motor to unlock a mechanism if the correct pattern is detected.
 *
 * Hardware connections:
 * - GPIO36 (ADC1_CH0): Piezo sensor (connected to ground with a 1MΩ pulldown resistor)
 * - GPIO2: Programming switch to enter a new code (short this pin to enter programming mode)
 * - GPIO4: DC gear reduction motor connected to the locking mechanism
 * - GPIO16: Red LED indicator
 * - GPIO17: Green LED indicator
 */

// Pin definitions
const int knockSensorPin = 36;  // Piezo sensor connected to GPIO36 (ADC1_CH0)
const int programSwitchPin = 21; // Programming mode switch connected to GPIO2
const int motorPin = 4;         // Motor control connected to GPIO4
const int redLedPin = 16;       // Red LED connected to GPIO16
const int greenLedPin = 17;     // Green LED connected to GPIO17

// Configuration constants
const int knockThreshold = 3;          // Minimum signal from the piezo to register as a knock
const int individualRejectMargin = 25; // Acceptable deviation percentage for individual knocks
const int averageRejectMargin = 15;    // Acceptable average deviation percentage for the knock sequence
const int knockFadeTime = 150;         // Milliseconds to allow a knock to fade
const int lockTurnDuration = 650;      // Milliseconds to run the motor for a half turn
const int maxKnocks = 20;              // Maximum number of knocks to listen for
const int knockTimeout = 1200;         // Maximum time to wait for a knock sequence

// Variables
int secretCode[maxKnocks] = {50, 25, 25, 50, 100, 50}; // Initial knock pattern
int knockTimes[maxKnocks];                             // Array to store knock intervals
int sensorValue = 0;                                   // Last reading of the knock sensor
bool isProgrammingMode = true;                        // Flag for programming mode

// **Function prototypes**
void listenToKnocks();
bool validateKnock();
void triggerUnlock();

void setup()
{
    pinMode(motorPin, OUTPUT);
    pinMode(redLedPin, OUTPUT);
    pinMode(greenLedPin, OUTPUT);
    pinMode(programSwitchPin, INPUT_PULLUP); // Use internal pull-up resistor

    Serial.begin(115200);
    Serial.println("Program started.");

    digitalWrite(greenLedPin, HIGH); // Green LED on, system is ready
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
        listenToKnocks();
    }
}

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

    do
    {
        // Listen for the next knock or wait for it to timeout
        sensorValue = analogRead(knockSensorPin);
        if (sensorValue >= knockThreshold)
        { // Another knock detected
            // Record the delay time
            Serial.println("Knock detected.");
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
            triggerUnlock();
        }
        else
        {
            Serial.println("Secret knock failed.");
            digitalWrite(greenLedPin, LOW); // We didn't unlock, so blink the red LED as visual feedback
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
}

// Function to unlock the door: insert the code for the Nuki web API here.
void triggerUnlock()
{
    Serial.println("Door unlocked!");

    // Insert Nuki API code here
    // Example: Send an HTTP request to the Nuki Bridge or API

    // For demonstration, we'll simulate the motor activation
    digitalWrite(motorPin, HIGH);
    digitalWrite(greenLedPin, HIGH);

    delay(lockTurnDuration); // Wait a bit

    digitalWrite(motorPin, LOW); // Turn the motor off

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
        for (int i = 0; i < maxKnocks; i++)
        { // Normalize the times
            secretCode[i] = map(knockTimes[i], 0, maxInterval, 0, 100);
        }
        // Flash the lights in the recorded pattern to indicate it's been programmed
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

    // Check if the number of knocks matches
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



