/*
Eric Tsai

Purpose:  Gateway for JSON serial data to MQTT via hardwired W5500 ethernet module

Input:
Expected JSON format:  {"mac":"c2f154bb0af9","rssi":-55,"volt":3.05,"mag":1}

Output:
Iterates through the key:value pairs and publishes in this form:
/ble/<mac>/<key>/<loc>_<value>
/ble/c2f154bb0af9/volt/1 3.05
/ble/c2f154bb0af9/rssi/1 -55


Hardware:
Tested on Uno.
Uart RX & TX on default Serial pins 0 & 1
W5500 ethernet module wire to standard SPI pins 10, 11, 12, 13

Changes:
1)  IP address of MQTT broker

2)  <loc> designation, persumably unique per gateway

3)  heartbeat

Notes:
mosquitto_sub -h localhost -v -t '#'

Status:  tested, works.  Need to comment out debug

ToDo:  
save note:  may need to pre-parse for escape characters?  Not sure if ArduinoJSON reads / as reserved


 */

#include <SPI.h>
#include <Ethernet2.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// Enter a MAC address for your controller below.
// Newer Ethernet shields have a MAC address printed on a sticker on the shield
byte mac[] = {
  0x00, 0xAA, 0xBB, 0xCC, 0xDE, 0x02
};

IPAddress server(192, 168, 2, 53);  //mqtt server
unsigned long heartbeat_millis=0;
int heartbeat_cnt=0;

//MQTT names
const char mqtt_name[]="ble_gateway1";

//buffer for char[] used in mqtt publishes
char mqtt_buf[20];

// Callback function header
void callback(char* topic, byte* payload, unsigned int length);


// Initialize the Ethernet client library
// with the IP address and port of the server
// that you want to connect to (port 80 is default for HTTP):
EthernetClient ethclient;
PubSubClient mqttclient(server, 1883, callback, ethclient);
//String serialData;


static char serialbuffer[80];


// Callback function
void callback(char* topic, byte* payload, unsigned int length) {
  // In order to republish this payload, a copy must be made
  // as the orignal payload buffer will be overwritten whilst
  // constructing the PUBLISH packet.

  // Allocate the correct amount of memory for the payload copy
  byte* p = (byte*)malloc(length);
  // Copy the payload to the new buffer
  memcpy(p,payload,length);
  mqttclient.publish("outTopic", p, length);
  // Free the memory
  free(p);
}


void setup() {
  // Open serial communications and wait for port to open:
  Serial.begin(9600);
  // this check is only needed on the Leonardo:
  while (!Serial) {
    ; // wait for serial port to connect. Needed for Leonardo only
  }

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
}

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
          buffer[pos++] = readch;
          buffer[pos] = 0;
        }
    }
  }
  // No end of line has been found, so return -1.
  return -1;
}

//json iterator that parses char* and publish each key-value pair as mqtt data
//I think 
void localParseJson(char *mybuf)
{

  //JSON parsing
  //JSON object exists in this scope, routine should execute atomatically no interrupts
  //If software serial is used, and parsing is interrupted, json object still be valid?
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& object = jsonBuffer.parseObject(mybuf);
  /*
    mac:c2f154bb0af9,rssi:-55,volt:3.05,mag:1
    /ble/c2f154bb0af9/rssi -55
    /ble/c2f154bb0af9/volt 3
    /ble/c2f154bb0af9/mag 1

  1)  count # fields in after MAC
  2)  concat "ble"/"MAC"/+iterate field
  3)  capture data to publish
 
   */
  if (object.success())
  {
    // Parsing fail

    char mytopic[60];
    int topic_char_index=0;
    // mytopic[i] = '\0'; make sure null terminate mytopic
  
    //construct the first part of the topic name, which is just /ble/mac
    mytopic[0]='/'; topic_char_index++;
    mytopic[1]='b'; topic_char_index++;
    mytopic[2]='l'; topic_char_index++;
    mytopic[3]='e'; topic_char_index++;
    mytopic[4]='/'; topic_char_index++;
    //topic_char_index = 5;
    const char* mac_name = object["mac"];
    Serial.println("mac name is");
    
    Serial.println(mac_name);
    int sizeofff = strlen(mac_name);
    Serial.print("size of macname: ");
    Serial.println(sizeofff);
    /*
    0123456789       17, 18
    /ble/c2f154bb0af9/
     
    */
    //fill mac address into topic name string
    for (int i=0; i<sizeofff; i++)
    {
      mytopic[topic_char_index]= mac_name[i];
      topic_char_index++;
    }
    mytopic[topic_char_index] = '/';
    topic_char_index++;
    
    Serial.println("da da da");
    Serial.println(mytopic);
    Serial.println("da da da");
  
  
    //iterate through sensor topics portions
    /*
     /ble/c2f154bb0af9/????
     /ble/c2f154bb0af9/rssi
     /ble/c2f154bb0af9/volt
     /ble/c2f154bb0af9/mag
     */

    //as we iterate through the keys in json, need a marker for char[] index
    //up to the MAC address:  "/ble/c2f154bb0af9/"  <---- here
    int topic_char_index_marker = topic_char_index;
    
    for (JsonObject::iterator i=object.begin(); i!=object.end(); ++i)
    {
      topic_char_index = topic_char_index_marker; //reset index to just after MAC location
      /*
       /ble/c2f154bb0af9/  <---- here
      */
      //!!! need to reset /ble/c2f154bb0af9/xxxxxee
      Serial.print("iterator::");
      Serial.print(i->key);
      Serial.print(",");
      Serial.println(i->value.asString());
      //mqttclient.publish(i->key,i->value.asString());

      //copy string for key into the prepared topic string
      //int keysize = strlen(i->key);
      //int keysize = 14;
      //int keysize = strlen(mac_name)  this worked before becuase it's big

      char* key_name = i->key;
      int keysize = strlen(key_name);
      Serial.print("key size:  ");
      Serial.print(keysize);
      Serial.print(",        topic_char_index:  ");
      Serial.println(topic_char_index);
      for (int j=0; j<keysize; j++)
      {
        mytopic[topic_char_index] = key_name[j];  topic_char_index++;
        
      }
      //topic_char_index--; //always one more after for loop
      
      /* note that topic_char_index is sitting at position after rss"i"
       *  /ble/c2f154bb0af9/rssi   <----- rssi, volt, etc...
       */

      //add ble gateway location code
      
      mytopic[topic_char_index] = '/';  topic_char_index++;
      mytopic[topic_char_index] = '1';  topic_char_index++;   //change location code per BLE gateway
      mytopic[topic_char_index] = '\0';
      Serial.print("topic_char_index of null char");
      Serial.println(topic_char_index);
      Serial.println("printing entire mytopic:");
      Serial.print(mytopic);
      
      //!!! need to null terminate mytopic
      mqttclient.publish(mytopic,i->value.asString());
    }//end iterator

  }//end if json parsed successful
  else
  {
    mqttclient.publish("hello", "bad json format");
    Serial.println("--->bad json:");
    Serial.println(mybuf);
  } //failed json parse

} //end routine localParseJson


//*****************************
//
//
//*****************************
void loop() {
  mqttclient.loop();
  
  //buffer for reading serial, must be writable
  



  
  if (Serial.available() > 0)
  {
    //received serial line of json
    if (readline(Serial.read(), serialbuffer, 80) > 0)
    {
      Serial.print("You entered: >");
      Serial.print(serialbuffer);
      Serial.println("<");
      localParseJson(serialbuffer); //call JSON iterator on this line from serial
    }

    
    
    /*
    mac:c2f154bb0af9,rssi:-55,volt:3.05,mag:1
    
     */
    
  }//end seria.available

  //every X seconds, publish heartbeat
  if (millis() - heartbeat_millis > 20000)
  {
    heartbeat_millis = millis();
    heartbeat_cnt++;
    mqttclient.publish(mqtt_name,itoa(heartbeat_cnt, mqtt_buf, 10));
  }
} //end loop


