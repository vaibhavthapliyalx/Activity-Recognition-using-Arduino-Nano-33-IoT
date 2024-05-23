import json
import paho.mqtt.client as mqtt
import csv
from collections import defaultdict
import time

# Create a dictionary to store file writers for each activity
file_writers = defaultdict(lambda: {'train': None, 'test': None})
burst_counts = defaultdict(int)

# The callback for when the client receives a CONNACK response from the server.
def on_connect(client, userdata, flags, reason_code, properties):
    print(f"Connected with result code {reason_code}")
    # Subscribing in on_connect() means that if we lose the connection and
    # reconnect then subscriptions will be renewed.
    isSubscribed = client.subscribe("com669/vaibhav")
    if isSubscribed[0] == 0:
        print("Subscribed to topic")
    else:
        print("Error while subscribing to topic")
    print(userdata)

def on_subscribe(client, userdata, mid, granted_qos, properties):
    print(f"Subscribed to topic with QoS {granted_qos}")

# The callback for when a PUBLISH message is received from the server.
def on_message(client, userdata, msg):
    try:
        print(msg.topic+" "+str(msg.payload))
        # Parse JSON data
        data = json.loads(msg.payload)
        activity = data['activity']

        # Determine whether to write to train or test file
        file_type = 'train' if data['burst_count'] <= 3 else 'test'

        # Open the appropriate file and get the writer
        with open(f"{activity}_{file_type}.csv", 'a') as f:
            writer = csv.writer(f)
            # Write header if file is empty
            if f.tell() == 0:
                writer.writerow(['timestamp', 'ax', 'ay', 'az', 'gx', 'gy', 'gz'])

            # Write data
            writer.writerow([data['timestamp'], data['ax'], data['ay'], data['az'], data['gx'], data['gy'], data['gz']])
    except Exception as e:
        print(f"Error while writing to CSV: {e}")

mqttc = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
mqttc.on_connect = on_connect
mqttc.on_subscribe = on_subscribe
mqttc.on_message = on_message

mqttc.connect("192.168.1.54", 1883, 60)

# Blocking call that processes network traffic, dispatches callbacks and
# handles reconnecting.
# Other loop*() functions are available that give a threaded interface and a
# manual interface.
mqttc.loop_forever()