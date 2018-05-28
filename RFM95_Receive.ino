/*
Author:  Eric Tsai, 2018
License:  CC-BY-SA, https://creativecommons.org/licenses/by-sa/2.0/
File: RFM95_Receive.ino
Purpose:  LoRa to Serial gateway, converts LoRa packet to serial in JSON format

Credit:  Original LoRa code from Adafruit LoRa example.
*/

#include <SPI.h>
#include <RH_RF95.h>
#include <ArduinoJson.h>  //change:  use version 5.10 for higher precision float support

//pin out Arduino uno
/*
RFM95:DIO0 (int, yellow) --> Uno:pin2
RFM95:reset (rst, yellow) --> Uno:pin9
RFM95:nss (select, white) --> Uno:pin10
RFM95:sck (clock, blue) --> Uno:pin13
RFM95:mosi (mosi, green) --> Uno:pin11
RFM95:DIO0 (miso, white) --> Uno:pin12


*/



//Using 32U4 Adafruit-or clone version of RFM95 module, these are SPI pins.
#define RFM95_CS 8
#define RFM95_RST 4
#define RFM95_INT 7


//For Arduino Uno or Nano, this is interface for SPI
//#define RFM95_CS 10
//#define RFM95_RST 9
//#define RFM95_INT 2

//Pin# 10, 11, 12, 13 = CS, MOSI, MISO, NSS
//Pin #9 - reset
//Pin #2 - DIO-0 interrupt

uint8_t GATEWAY_ID = 1;


//const int pinHandShake = 4; //handshake, serial-to-mqtt flow control for Uno/Nano
const int pinHandShake = 6; //handshake, serial-to-mqtt flow control for 32U4


// Change to 434.0 or other frequency, must match RX's freq!
#define RF95_FREQ 915.0

// Singleton instance of the radio driver
RH_RF95 rf95(RFM95_CS, RFM95_INT);

// Blinky on receipt
#define LED 13

//using softwareserial for uart to BLE module
#include <SoftwareSerial.h>
//SoftwareSerial BLE_Serial(5, 6); // RX(yellow), TX(blue)


void setup() 
{
  //delay(10000);
  
  pinMode(pinHandShake, INPUT_PULLUP);  //make sure ETH gateway GND's pin.
  pinMode(RFM95_RST, OUTPUT);
  digitalWrite(RFM95_RST, HIGH);

  while (!Serial);
  Serial.begin(9600);
  delay(100);

  //Serial.println("after 10 seconds, ready");
  
  // manual reset
  digitalWrite(RFM95_RST, LOW);
  delay(10);
  digitalWrite(RFM95_RST, HIGH);
  delay(10);

  while (!rf95.init()) {
    Serial.println("LoRa radio init failed");
    while (1);
  }
  Serial.println("LoRa radio init OK!");

  // Defaults after init are 434.0MHz, modulation GFSK_Rb250Fd250, +13dbM
  if (!rf95.setFrequency(RF95_FREQ)) {
    Serial.println("setFrequency failed");
    while (1);
  }
  Serial.print("Set Freq to: "); Serial.println(RF95_FREQ);

  // Defaults after init are 434.0MHz, 13dBm, Bw = 125 kHz, Cr = 4/5, Sf = 128chips/symbol, CRC on


  //change modulation scheme here; faster than 125kHz BW is unstable
  //Bw125Cr45Sf128, Bw = 125 kHz, Cr = 4/5, Sf = 128chips/symbol, CRC on. Default medium range.
  //Bw500Cr45Sf128, Bw = 500 kHz, Cr = 4/5, Sf = 128chips/symbol, CRC on. Fast+short range.
  //Bw31_25Cr48Sf512, Bw = 31.25 kHz, Cr = 4/8, Sf = 512chips/symbol, CRC on. Slow+long range.
  //Bw125Cr48Sf4096, Bw = 125 kHz, Cr = 4/8, Sf = 4096chips/symbol, CRC on. Slow+long range.

  /*
  if (!rf95.setModemConfig(RH_RF95::Bw31_25Cr48Sf512)) {
    Serial.println("setModemConfig failed");
    while (1);
  }
  Serial.println("Set setModemConfig to: something");
  */

  // The default transmitter power is 13dBm, using PA_BOOST.
  // If you are using RFM95/96/97/98 modules which uses the PA_BOOST transmitter pin, then 
  // you can set transmitter powers from 5 to 23 dBm:
  rf95.setTxPower(23, false);


  //BLE serial port
  //BLE_Serial.begin(9600);
  
}

void loop()
{
  if (rf95.available())
  {
    // Should be a message for us now   
    uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
    //char buf[RH_RF95_MAX_MESSAGE_LEN];
    uint8_t len = sizeof(buf);

    
    if (rf95.recv(buf, &len))
    {
      //digitalWrite(LED, HIGH);
      RH_RF95::printBuffer("Received: ", buf, len);
 
        
        /*

        Radio packet to send:
        MAC[6 bytes], volt [4 bytes], xmit count [1 byte], rssi[1 byte], gateway #[1 byte], sensor json
         [0-5]                  [6-9]             [10]                       [11]               [12]                      [13-x]
     
        MAC[6 bytes], volt [4 bytes], xmit count [1 byte], rssi[1 byte], gateway #[1 byte], sensor json
         [1-6]                  [7-10]             [11]                       [12]               [13]                      [14-x]        
        
        For a LoRa sensor:
        rssi=0
        gateway=0
        
        LoRa Repeater will have gateway != 0
        
        uart ble:
        {"mac":c56e806d7cfb,"rssi":-44,"volt":3.00,"tmr":132,"xmit_cnt":240,"mag/p":0}
        
        uart lora:
        {"mac":c56e806d7cfb,"volt":3.00,"xmit_cnt":240,"rssi":-44,"gw":1,"mySensorValue":xxx, "mySensorValue":yyy}
        
        MQTT:
        /xxx/Gateway/MAC/metric ____ value
        
        
        */
        
        
      if (len >= 9)
      {
          //capture RRSI and gateway for zeros to see if repeated packet or not
          uint8_t gateway = buf[13];
          int8_t rssi = buf[12];
          

          //Dont' use Serial.print(x, HEX), because it's capital, and misses single diggit hex<0x10.
          Serial.print("{\"mac\":");

          //six hex bytes to 12 ascii; 
          for(int i=1; i<7; i++)
          {
            char upper;
            char lower;
            upper = buf[i] >> 4 & 0x0f;
            lower = buf[i] & 0x0f;
            char upper2 = upper + 48;
            char lower2 = lower + 48;
            if (upper > 9)
              upper2 += 39;
            if (lower > 9)
              lower2 +=39;
            Serial.print(upper2);
            Serial.print(lower2);
          }


          //volt
          Serial.print(",\"volt\":");
          //  {"mac":c56e806d7cfb,"volt":
          Serial.print((char)buf[7]);
          Serial.print((char)buf[8]);
          Serial.print((char)buf[9]);
          Serial.print((char)buf[10]); 
          //  {"mac":c56e806d7cfb,"volt":3.00
          
          
          //xmit_cnt
          Serial.print(",\"xcnt\":");
          //  {"mac":c56e806d7cfb,"volt":3.00,"xmit_cnt":
          Serial.print(buf[11], DEC);
          
          //  {"mac":c56e806d7cfb,"volt":3.00,"xmit_cnt":240,
          //rssi (if repeat or direct)
          Serial.print(",\"rssi\":");
          //  {"mac":c56e806d7cfb,"volt":3.00,"xmit_cnt":240,"rssi":
          //if gateway>0, that means it's a repeat, so just replicate gateway
          if (gateway > 0)
          {
              Serial.print(rssi);
          }
          else
           //gateway=0, it's sensor data, so RSSI is valid
          {
              Serial.print(rf95.lastRssi(), DEC);
          }
          //  {"mac":c56e806d7cfb,"volt":3.00,"xmit_cnt":240,"rssi":-44

          
          //gateway (if repeat or direct)
          Serial.print(",\"gw\":");
          //  {"mac":c56e806d7cfb,"volt":3.00,"xmit_cnt":240,"rssi":-44,"gw":
          //gateway name: 
          //for ESP8266, if "gw" exists, use "gw" value.
          if (gateway > 0)
          {
              Serial.print(gateway);
          }
          else
           //gateway=0, it's sensor data, so RSSI is valid
          {
              Serial.print(GATEWAY_ID);
          }
          
          //  {"mac":c56e806d7cfb,"volt":3.00,"xmit_cnt":240,"rssi":-44,"gw":1

          
          //{"mac":c56e806d7cfb,"volt":3.00,"xmit_cnt":240,"rssi":-44,"gw":1,"mySensorValue":xxx, "mySensorValue":yyy}

          Serial.print(",");
          //json looks like this:
          //    "lo":xxxx,"la":xxxx
          //json
          for (int i=14; i<len-1; i++)
          {
              Serial.print(char(buf[i]));
          }
          Serial.print("}");
          Serial.print("\r\n");
      } //if buf len is big enough



    }
    else
    {
      Serial.println("Receive failed");
    }
    
    while (digitalRead(pinHandShake) == 0)
    {
      //loop until MQTT bridge ready to receive to avoid correupted JSON...optional
      //acts like uart flow control.  If pin is disconnected, works as normal, might get multiple uart lines
      //expect serial-MQTT bridge to ground the pin when receiving uart and posting MQTT.
    }
    
  }//received RFM



  //BLE send
  if (Serial.available()) {
    //BLE_Serial.write(Serial.read());
  }
}
