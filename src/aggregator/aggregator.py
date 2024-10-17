import paho.mqtt.client as mqtt
from flask import Flask, jsonify, request, make_response
import json
import threading

app = Flask(__name__)

mqttBroker = "192.168.0.118"  # Replace with your broker's IP
mqttPort = 1900

knockData = {}

# MQTT Callback functions
def on_connect(client, userdata, flags, rc):
    print("Connected to MQTT Broker with result code " + str(rc))
    client.subscribe("knock/sensor/#")

def on_message(client, userdata, msg):
    topic = msg.topic
    payload = msg.payload.decode()
    data = json.loads(payload)
    sensor_id = data.get('sensor_id')

    if 'knock_sequence' in data:
        knockData[sensor_id] = data  # Store the knock sequence
    elif 'validation' in data:
        if sensor_id in knockData:
            knockData[sensor_id]['validation'] = data['validation']
        else:
            knockData[sensor_id] = data

# MQTT Client Setup
client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message
client.connect(mqttBroker, mqttPort, 60)

def mqtt_loop():
    client.loop_forever()

mqtt_thread = threading.Thread(target=mqtt_loop)
mqtt_thread.start()

# REST API Endpoints
@app.route('/knocks', methods=['GET'])
def get_all_knocks():
    response = make_response(jsonify(knockData), 200)
    return response



# Run Flask app
if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5050)
