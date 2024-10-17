import paho.mqtt.client as mqtt
import time
import json
import random

ipAddressMQTT = "192.168.0.118"
portMQTT = 1900

client = mqtt.Client()
client.connect(ipAddressMQTT, portMQTT, 60)

sensor_ids = ["knock_sensor_1", "knock_sensor_2", "knock_sensor_3"]

while True:
    sensor_id = random.choice(sensor_ids)
    knock_sequence = [random.randint(20, 100) for _ in range(random.randint(3, 6))]
    data = {
        "sensor_id": sensor_id,
        "timestamp": int(time.time() * 1000),
        "knock_sequence": knock_sequence
    }
    payload = json.dumps(data)
    client.publish("knock/sensor/data", payload)
    print(f"Published knock sequence from {sensor_id}")
    time.sleep(5)
