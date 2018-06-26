
https://www.hackster.io/erictsai/lora-tooth-small-ble-sensors-over-wifi-lora-gateways


#esp8266_mqtt.ino

For BLE and LoRa gateway using ESP8266 WiFi
Input:  uart serial data in JSON format
Output:  iterates through JSON and publishes wireless LoRa or BLE data as MQTT


#W5500_mqtt.ino

Same as the <esp8266_mqtt>, but using W5500 ethernet module instead of WiFi

#RFM95_Receive.ino

LoRa Receiver
Input:  LoRa wireless packet
Output:  serial data in JSON format


#BLE_LoRa_bridge.ino

BLE to LoRa bridge
Input:  serial data in JSON format
Output:  LoRa data
