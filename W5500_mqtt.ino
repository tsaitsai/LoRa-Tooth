/*
Author:  Eric Tsai, 2017
License:  CC-BY-SA, https://creativecommons.org/licenses/by-sa/2.0/

Purpose:  Gateway for JSON serial data to MQTT via hardwired W5500 ethernet module

Input:  Expected JSON format via uart
{"mac":"c2f154bb0af9","rssi":-55,"volt":3.05,"sensor":1, "key":value}
{"mac":"cow2","rssi":-109,"volt":3.61,"lat":45.055761, "long":-92.980961}


Output:
Iterates through the key:value pairs and publishes in this form:
/BLE/<loc>/<mac>/<key>/ <value>
/ble/livingroom/c2f154bb0af9/volt 3.05
/ble/livingroom/c2f154bb0af9/rssi/ -55
/LORA/farmhouse/cow2/lat 45.055761
/LORA/farmhouse/cow2/long -92.980961


Hardware:
Tested on Uno and Nano
Uart RX & TX on default Serial pins 0 & 1
W5500 ethernet module wire to standard SPI pins 10, 11, 12, 13 (Uno)
Increase serial buffer size from 64 to 128 bytes in 
  C:\Program Files (x86)\Arduino\hardware\arduino\avr\cores\arduino\HardwareSerial.h

Changes:  ctrl-F "**CHANGE**"
1)  IP address of MQTT broker
2)  <loc> designation, persumably unique per gateway
3)  IP address of W5500
3)  Remember to compile with larger serial buffer board definition

Notes:  To test by subscribing to all topics
mosquitto_sub -h localhost -v -t '#'

*/

#include <SPI.h>
#include <Ethernet2.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

//"**CHANGE**" W5500 MAC address (i.e. used for static DHCP on router)
//Must be unique per gateway.  You'll see MQTT connection issues if not.
//byte mac[] = {0x00, 0xAA, 0xBB, 0xCC, 0xDE, 0x03};  //nano2 test
byte mac[] = {0x00, 0xAA, 0xBB, 0xCC, 0xDE, 0x01};  //nano1 livingroom
//byte mac[] = {0x00, 0xAA, 0xBB, 0xCC, 0xDE, 0x02};  //uno1 basement


/*
//"**CHANGE**" mqtt_name[] used as part of topic name
/ble/MQTT_NAME/mac/rssi
/ble/MQTT_NAME/mac/volt
*/
//const char mqtt_name[]="nano_02";
const char mqtt_name[]="nano_01";
//const char mqtt_name[]="uno_01";
//const char mqtt_name[]="livingroom";
//const char mqtt_name[]="basement";

const int pinHandShake = 6; //handshake, serial-to-mqtt flow control

//"**CHANGE**" MQTT server IP address
IPAddress server(192, 168, 2, 24);  //mqtt server server 2.3

unsigned long heartbeat_millis=0;
int heartbeat_cnt=0;

//"**CHANGE**" debug output
#define MyDebug 0  //0=no debug, 1=deeebug

//"**CHANGE**" BLE, LORA, etc...
#define RF_TYPE BLE
//#define RF_TYPE LORA

//buffer for char[] used in mqtt publishes, size at least as big as topic + msg length
char mqtt_buf[60];   // e.g "/ble/esp1/d76dcd1b3720/volt" is 30 chars long

bool conn_status;
// Callback function header
void callback(char* topic, byte* payload, unsigned int length);


// Initialize the Ethernet client library with the IP address and port of MQTT server
EthernetClient ethclient;
PubSubClient mqttclient(server, 1883, callback, ethclient);


/*
Increase serial buffer size so we can capture at least the entire JSON string in serial buffer
Needed for Arduino Uno, Nano, etc that only have 64byte serial buffer

Increase serial buffer size in Arduino core to 128
C:\Program Files (x86)\Arduino\hardware\arduino\avr\cores\arduino\HardwareSerial.h
#define SERIAL_RX_BUFFER_SIZE 64

C:\Program Files (x86)\Arduino\hardware\arduino\avr\cores\arduino\USBAPI.h
#define SERIAL_BUFFER_SIZE 64 
 *
 */
const int serialBufSize = 100;      //size for at least the size of incoming JSON string
static char serialbuffer[serialBufSize];  // {"mac":"c2f154bb0af9","rssi":-71,"volt":2.93,"tmr":230,"xmit_cnt":1,"mag/p":1}
static char mytopic[120];  //MQTT topic string size, must be less than size declared


// Callback function for mqtt subscribes
void callback(char* topic, byte* payload, unsigned int length) {

}


void setup() {
  // Open serial communications and wait for port to open:
  Serial.begin(9600);
  // this check is only needed on the Leonardo:
  while (!Serial) {
    ; // wait for serial port to connect. Needed for Leonardo only
  }

  #if MyDebug
  Serial.println("startup...");
  #endif
  
  pinMode(pinHandShake, OUTPUT);
  
  
  // start the Ethernet connection:
  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
    // no point in carrying on, so do nothing forevermore:
    for (;;)
      ;
  }
  // print your local IP address:
  Serial.print("My IP address: ");
  for (byte thisByte = 0; thisByte < 4; thisByte++) {
    // print the value of each byte of the IP address:
    Serial.print(Ethernet.localIP()[thisByte], DEC);
    Serial.print(".");
  }
  Serial.println();


  //MQTT
  if (mqttclient.connect(mqtt_name)) {
    mqttclient.publish("hello",mqtt_name);
    //mqttclient.subscribe("inTopic");
  }
  else
  {

    Serial.print("could not connect to MQTT client");
  }
}//end setup()


//stealing this code from
//https://hackingmajenkoblog.wordpress.com/2016/02/01/reading-serial-on-the-arduino/
//non-blocking serial readline routine, very nice.  Allows mqtt loop to run.
int readline(int readch, char *buffer, int len)
{
  static int pos = 0;
  int rpos;

  if (readch > 0) {
    switch (readch) {
      case '\n': // Ignore new-lines
        break;
      case '\r': // Return on CR
        rpos = pos;
        pos = 0;  // Reset position index ready for next time
        return rpos;
      default:
        if (pos < len-1) {
          buffer[pos++] = readch;  //first buffer[pos]=readch; then pos++;
          buffer[pos] = (char)0;
        }
    }
  }
  // No end of line has been found, so return -1.
  return -1;
}//end readline()



//json iterator that parses char* and publish each key-value pair as mqtt data
void localParseJson(char *mybuf)
{
  //JSON parsing
  //JSON object exists in this scope, routine should execute atomatically no interrupts
  //If software serial is used, and parsing is interrupted, json object still be valid?  not sure.
  StaticJsonBuffer<150> jsonBuffer;
  JsonObject& object = jsonBuffer.parseObject(mybuf);
  /*
    Incoming uart:
    mac:c2f154bb0af9,rssi:-55,volt:3.05,mag:1
    
    Iterate and publish as
    /ble/livingroom/c2f154bb0af9/rssi -55
    /ble/livingroom/c2f154bb0af9/volt 3
    /ble/livingroom/c2f154bb0af9/mag 1

  1)  count # fields in after MAC
  2)  concat "ble"/"MAC"/+iterate field
  3)  capture data to publish
   */
  if (object.success())
  {

    //todo:  it's too easy to forget mytopic size, not checking for size when string being assembled.
    //    add error checking and improve readability

    int topic_char_index=0;
    // mytopic[i] = '\0'; make sure null terminate mytopic
  
    //construct the first part of the topic name, which is just /BLE or /LORA
    mytopic[0]='/'; topic_char_index++;
    mytopic[1]='y'; topic_char_index++;
    mytopic[2]='y'; topic_char_index++;
    mytopic[3]='y'; topic_char_index++;
    mytopic[4]='/'; topic_char_index++;  //topic_char_index is 5;

    //add gateway location name, /ble/LIVINGROOM
    int mqtt_name_len = sizeof(mqtt_name);
    for (int i=0; i<(mqtt_name_len-1); i++)
    {
      mytopic[topic_char_index] = mqtt_name[i];
      topic_char_index++;
    }
    mytopic[topic_char_index] = '/';  topic_char_index++;

    //set string pointer to JSON Value containing the key "mac"
    // so it's pointing to "mac":c2f154bb0af9
    const char* mac_name = object["mac"];

    #if MyDebug
    Serial.println("mac name is");
    Serial.println(mac_name);
    #endif
    
    int sizeofff = strlen(mac_name);

    #if MyDebug
    Serial.print("size of macname: ");
    Serial.println(sizeofff);
    #endif

    // /ble/livingroom/c2f154bb0af9/
    //fill mac address into topic name string
    for (int i=0; i<sizeofff; i++)
    {
      mytopic[topic_char_index]= mac_name[i];
      topic_char_index++;
    }
    mytopic[topic_char_index] = '/';  topic_char_index++;
    

    #if MyDebug
    Serial.println("mytopic");
    Serial.println(mytopic);
    #endif
  

    /*
     iterate through sensor topics portions
        /ble/livingroom/c2f154bb0af9/????
        /ble/livingroom/c2f154bb0af9/rssi
        /ble/livingroom/c2f154bb0af9/volt
        /ble/livingroom/c2f154bb0af9/sensor
    */

    //as we iterate through the keys in json, need a marker for char[] index
    //up to the MAC address:  "/ble/livingroom/c2f154bb0af9/"  <---- here
    int topic_char_index_marker = topic_char_index; //place holder for last / after mac

    for (JsonObject::iterator i=object.begin(); i!=object.end(); ++i)
    {
      topic_char_index = topic_char_index_marker; //reset index to just after MAC location
      /*
       /ble/livingroom/c2f154bb0af9/  <---- here
      */

      #if MyDebug
      Serial.print("iterator::");
      Serial.print(i->key);
      Serial.print(",");
      Serial.println(i->value.asString());
      #endif
      //mqttclient.publish(i->key,i->value.asString());

      //copy string for key into the prepared topic string
      const char* key_name = i->key;
      int keysize = strlen(key_name);   //returns size of keyname, not 1+size like sizeof()

      #if MyDebug
      Serial.print("key size:  ");
      Serial.print(keysize);
      Serial.print(",        topic_char_index:  ");
      Serial.println(topic_char_index);
      #endif
      
      for (int j=0; j<keysize; j++)
      {
        mytopic[topic_char_index] = key_name[j];  topic_char_index++;
        
      }

      
      /* note that topic_char_index is sitting at position after rss"i"
       *  /ble/c2f154bb0af9/rssi   <----- rssi, volt, etc...
       */

      mytopic[topic_char_index] = '\0'; //terminate string w/ null

      #if MyDebug
      Serial.print("topic_char_index of null char");
      Serial.println(topic_char_index);
      Serial.println("printing entire mytopic:");
      Serial.print(mytopic);
      #endif

      
      //mqtt publish relies a null terminated string for topic names
      mqttclient.publish(mytopic,i->value.asString());
    }//end iterator

  }//end if json parsed successful
  else
  {
    mqttclient.publish(mqtt_name, "bad json format");
    mqttclient.publish(mqtt_name, mybuf);
    Serial.println("--->bad json:");
    Serial.println(mybuf);
  } //failed json parse
} //end routine localParseJson


//*****************************
// main loop
//
//*****************************
void loop() {
  mqttclient.loop();

  //buffer for reading serial, must be writable
  if (Serial.available() > 0)
  {
    //received serial line of json
    if (readline(Serial.read(), serialbuffer, serialBufSize) > 0)
    {
      digitalWrite(pinHandShake, HIGH);
      
      #if MyDebug
      Serial.print("You entered: >");
      Serial.print(serialbuffer);
      Serial.println("<");
      #endif
      
      localParseJson(serialbuffer); //call JSON iterator on this line from serial
      
      //delete this, not needed.
      for (int i=0; i<serialBufSize; i++)
      {
        serialbuffer[i] = (char)0;
      }
      digitalWrite(pinHandShake, LOW);
    }
  }//end seria.available

  //every X seconds, publish mqtt heartbeat
  if ((millis() - heartbeat_millis > 20000) || ((millis() - heartbeat_millis) < 0))
  {
    heartbeat_millis = millis();
    heartbeat_cnt++;
    conn_status = mqttclient.connected();
    Serial.print("20 second heartbeat:  ");
    if (conn_status == 0)
    {
      Serial.print("mqtt dead, attempting to connect...");
      mqttclient.disconnect();
      delay(2000);
      if (mqttclient.connect(mqtt_name))
      {
        Serial.println("failed to reconnect MQTT"); 
      }
      else
        Serial.println("reconnected succesful");
    }
    else
    {
      Serial.println("mqtt alive");
    } 
    if (conn_status == 0)
    {
      mqttclient.publish(mqtt_name, "had to reconnect");
    }
    mqttclient.publish(mqtt_name,itoa(heartbeat_cnt, mqtt_buf, 10));
  } //end heartbeat
} //end loop

