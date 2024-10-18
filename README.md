# ESP32-Knock-Sensor
## Demo

## Description
Knock pattern detection offers an intuitive and interactive way to control devices or authenticate users. This project utilizes an ESP32 microcontroller connected to a piezoelectric sensor to detect specific knock sequences. Users can program custom knock patterns, which can trigger actions like unlocking a door or activating devices.  

The detected knock sequences are sent via MQTT to a broker (Mosquitto). An aggregator, written in Python using the Paho MQTT client and Flask, collects this data and exposes it through a RESTful API. This allows for seamless integration with other systems and applications that can consume the knock data and validation results.  

## Features
* Customizable Knock Patterns: Program your own unique knock sequences for personalized interaction.  
* Real-time Data Transmission: Sends knock data and validation results over MQTT for immediate processing.  
* RESTful API Access: Aggregated data is accessible via a REST API for integration with other applications.  
* Visual Feedback: LEDs provide on-site visual indicators of system status and knock detection.  
* Multiple Sensor Support: Deploy multiple knock sensors within the network, each with unique IDs.  
* Wi-Fi Connectivity: ESP32 connects to a Wi-Fi network for wireless communication.  

## Technical Section
### Bill of Materials
* ESP32 Development Board (e.g., AZDelivery ESP32 Dev Kit V4)  
* Piezoelectric Sensor  
* 1MΩ Resistor (pulldown resistor for the piezo sensor)  
* Push-button or Switch (for entering programming mode)  
* Red LED  
* Green LED  
* Resistors (e.g., 220Ω) for current limiting with LEDs  
* Breadboard and Jumper Wires  
* USB Cable (for programming the ESP32)  
* Computer (with PlatformIO or Arduino IDE)  
* Wi-Fi Network  
## Hardware Setup & Wiring (fritzing sketch coming soon) 
### Piezo Sensor Setup
Connect one lead of the piezo sensor to GPIO36 (ADC1_CH0) on the ESP32.
Connect the other lead to GND.
Place a 1MΩ pulldown resistor between GPIO36 and GND to stabilize the sensor readings.
### Programming Mode Switch
* Connect one terminal of the push-button or switch to GPIO21.
* Connect the other terminal to GND.
* The switch uses the internal pull-up resistor, so when pressed (LOW), it enters programming mode.
### LED Indicators
#### Red LED:
* Connect the anode (longer leg) to GPIO16 via a current-limiting resistor (e.g., 220Ω).
* Connect the cathode (shorter leg) to GND.
#### Green LED:
* Connect the anode to GPIO17 via a current-limiting resistor.
* Connect the cathode to GND.
## Software Setup - Sensor
### Prerequisites
PlatformIO installed in Visual Studio Code, or Arduino IDE with ESP32 support.
ESP32 Board Definitions installed in your development environment.
### Dependencies
PubSubClient library (for MQTT communication)
ArduinoJson library (for JSON serialization)
### Configuration
1.Open the knock.cpp file in your development environment.  
2.Update the Wi-Fi credentials:  
```cpp
const char* ssid = "Your_SSID";
const char* password = "Your_Password";
Set the MQTT broker IP address and port:
```
cpp
Copy code
IPAddress mqttBroker(192, 168, 0, 118); // Replace with your broker's IP
const int mqttPort = 1900;
Optionally, set a unique sensor ID:

cpp
Copy code
const char* sensorID = "knock_sensor_1";
Compiling and Uploading
Ensure the platformio.ini file is correctly configured:

ini
Copy code
[env:az-delivery-esp32-v4]
platform = espressif32
board = az-delivery-devkit-v4
framework = arduino
monitor_speed = 115200
lib_deps =
    knolleary/PubSubClient@^2.8
    bblanchon/ArduinoJson@^6.20.0
build_src_filter = +<*> -<venv/**> -<aggregator/venv/**>
Connect the ESP32 board to your computer via USB.

Build and upload the code to the ESP32 using PlatformIO or the Arduino IDE.

Software Setup - Aggregator
Prerequisites
Python 3.x installed on your system.
Pip package manager.
Dependencies
Install the required Python packages using the provided requirements.txt:

bash
Copy code
pip install -r requirements.txt
This installs:

paho-mqtt==1.6.1
Flask==2.3.3
jsonpickle==3.0.2
Configuration
Open the aggregator.py script located in the aggregator directory.

Update the MQTT broker IP address and port:

python
Copy code
mqttBroker = "192.168.0.103"  # Replace with your broker's IP address
mqttPort = 1900
Running the Aggregator
Use the provided startupAggregator.sh script to run the aggregator:

bash
Copy code
./startupAggregator.sh
This script starts the Flask application on port 5050, accessible from any host (0.0.0.0).

MQTT Broker Setup
Install Mosquitto MQTT Broker on your system if not already installed.

Use the provided mosquitto.conf configuration file:

conf
Copy code
listener 1900 0.0.0.0
allow_anonymous true
Start the MQTT broker using the startupBroker.sh script:

bash
Copy code
./startupBroker.sh
Run
Start the MQTT Broker

bash
Copy code
./startupBroker.sh
Run the Aggregator

bash
Copy code
./startupAggregator.sh
Deploy the ESP32 Sensor

Power on the ESP32.
It will connect to the Wi-Fi network and start detecting knock patterns.
Interact with the Sensor

Use the programming switch to enter programming mode and set a custom knock pattern.
Knock on the sensor to test detection and pattern validation.
Employed Technologies
ESP32 Microcontroller: Provides Wi-Fi connectivity and processing power for sensor data acquisition.
Piezoelectric Sensor: Detects vibrations from knocks, converting mechanical stress into electrical signals.
MQTT Protocol: Lightweight messaging protocol ideal for IoT applications.
Python with Flask and Paho MQTT: Aggregator uses Flask for the REST API and Paho MQTT client to subscribe to MQTT topics.
Arduino Framework: Used to program the ESP32 with familiar functions and libraries.
Wi-Fi Communication: Allows the sensor to communicate over the network wirelessly.
JSON Serialization: Data is serialized into JSON format for easy parsing and integration.
Endpoints
The aggregator exposes the following RESTful API endpoints:

GET /knocks
Description: Retrieves data of all knock sequences from all sensors.

Response: JSON object containing knock data indexed by sensor IDs.

Example Response:

json
Copy code
{
  "knock_sensor_1": {
    "sensor_id": "knock_sensor_1",
    "timestamp": 1625097600000,
    "knock_sequence": [50, 25, 25, 50, 100, 50],
    "validation": "success"
  },
  "knock_sensor_2": {
    "sensor_id": "knock_sensor_2",
    "timestamp": 1625097660000,
    "knock_sequence": [75, 50, 50, 75],
    "validation": "failure"
  }
}
[Additional endpoints can be added as needed.]

Usage
Programming a New Knock Pattern
Enter Programming Mode: Press and hold the programming switch connected to GPIO21.
Visual Indicator: The red LED will turn on, indicating programming mode.
Record Knock Pattern: Knock the desired pattern on the sensor.
Exit Programming Mode: Release the programming switch.
Confirmation: LEDs will blink alternately to confirm the new pattern is stored.
Validating a Knock Pattern
Ensure Normal Mode: Make sure the programming switch is not pressed.
Perform Knock Pattern: Knock the stored pattern on the sensor.
Feedback:
Success: Green LED blinks; validation result is sent over MQTT.
Failure: Red LED blinks; validation failure is sent over MQTT.
Trigger Actions: Upon successful validation, actions like unlocking a door can be triggered.
Testing
A test publisher script test_publisher.py is provided to simulate sensor data being sent to the MQTT broker. This can be used to test the aggregator and REST API without the physical hardware.

Conclusion
This project demonstrates how knock pattern detection can be implemented using an ESP32 microcontroller and a piezoelectric sensor, with data transmission over MQTT and aggregation via a RESTful API. It provides a foundation for further development, such as integrating with smart locks or other IoT devices.

Note: Ensure all IP addresses, ports, and configuration settings are updated to match your specific environment before deploying the system.
