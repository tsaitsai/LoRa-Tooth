/*
Author:  Eric Tsai, 2018
License:  CC-BY-SA, https://creativecommons.org/licenses/by-sa/2.0/
File: RFM95_Receive.ino
Purpose:  Parse BLE data via uart, re-transmit as LoRa

Credit:  Original LoRa code from Adafruit LoRa example.
*/

uint8_t Gateway_ID = 100;

#include <SoftwareSerial.h>
#include <ArduinoJson.h>  //change:  use version 5.10 for higher precision float support

#define MyDebug 1   //0=no debug, 1=deeebug



//RFM65 LoRa stuff stuff for 32U4 arduino.
#include <SPI.h>
#include <RH_RF95.h>
#define RFM95_CS 8
#define RFM95_RST 4
#define RFM95_INT 7
// Change to 434.0 or other frequency, must match RX's freq!
#define RF95_FREQ 915.0

// Singleton instance of the radio driver
RH_RF95 rf95(RFM95_CS, RFM95_INT);

const int serialBufSize = 100;      //size for at least the size of incoming JSON string
static char serialbuffer[serialBufSize];  // {"mac":c2f154bb0af9,"rssi":-55,"volt":3.05,"mag":1}

uint32_t timer;
uint8_t xmit_cnt2 = 1;  //only 1 byte for transmit counter;
char mac[6] = {0x11, 0x11, 0x11, 0x11, 0x11, 0x12}; //GPS Sensor "0001" [my own lora mac]



char volt[5] = "3.11"; //4 bytes long
//char json_data = "1234567891012141618202224262830"; //30 byes
uint8_t xmit_cnt = 1;  //only 1 byte for transmit counter;
int index;  //radiopacket index value;


//stealing this code from
//https://hackingmajenkoblog.wordpress.com/2016/02/01/reading-serial-on-the-arduino/
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
          buffer[pos++] = readch;   //first buffer[pos]=readch; then pos++;
          buffer[pos] = 0;
        }
    }
  }
  // No end of line has been found, so return -1.
  return -1;
}



inline byte toHex(char z) {
  return z <= '9' ? z - '0' :  z <= 'F' ? z - 'A' + 10 : z - 'a' + 10;
}

void localParseJson(char *mybuf)
{
    Serial.println("in localparsejson");
    char radiopacket[220]="heloo";
    char mac[6] = {0x22, 0x33, 0x44, 0x55, 0xab, 0xcd};

    
    /*
    
        Radio packet to send:
        MAC[6 bytes], volt [4 bytes], xmit count [1 byte], rssi[1 byte], gateway #[1 byte], sensor json
         [1-6]                  [7-10]             [11]                       [12]               [13]                      [14-x]        
        uart ble:
        {"mac":c56e806d7cfb,"rssi":-44,"volt":3.00,"tmr":132,"xmit_cnt":240,"mag/p":0}
        
        uart lora:
        {"mac":c56e806d7cfb,"volt":3.00,"xmit_cnt":240,"rssi":-44,"gw":1,"mySensorValue":xxx, "mySensorValue":yyy}
        
        
    
    */
   
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& object = jsonBuffer.parseObject(mybuf);
  if (object.success())
  {
      Serial.println("parse success");
      //1.  Grab mac address "mac", convert to hex
      //2.  Grab volts "volt", print as is
      //3.  Grab CNT "xmit_cnt", convert to int
      //4.  Grab rest of key-value pairs, print as is
      const char* mac_name = object["mac"];     //grab pointer to mac address[0], null terminated
      int size_of_mac = strlen(mac_name); 
      //create 6 byte "0x11,0x11, 0x11...0xAB"
      
      const char* volt_val = object["volt"];     //grab pointer to mac address[0], null terminated
      int size_of_volt_val = strlen(volt_val);  //no need to convert
      
      const char* cnt_val = object["xmit_cnt"];     //grab pointer to xmit_cnt
      int size_of_cnt_val = strlen(cnt_val);    //convert to number, ugh

      const char* rssi_val = object["rssi"];     //grab pointer to rssi value (should be 4 bytes)
      int size_of_rssi_val = strlen(rssi_val);
      
      
        int num_jsons = 0;
        //const char (*jdata)[3]; //max of 3 json sets, reasonable.
        const char* key01;
        const char* val01;
        const char* key02;
        const char* val02;
        const char* key03;
        const char* val03;


        
        int keysize[4];
        int valsize[4];
        
        //find JSON objects.
        for (JsonObject::iterator i=object.begin(); i!=object.end(); ++i)
        {
            const char* key=i->key;
            if ((strcmp(key,"mac")!=0) &&
                (strcmp(key,"volt")!=0) &&
                (strcmp(key,"rssi")!=0) &&
                (strcmp(key,"tmr")!=0) &&
                (strcmp(key,"xmit_cnt")!=0) )
            { // is json data
               num_jsons++;
               if (num_jsons == 1)
               {
                   key01 = i->key;
                   val01 = i->value;
                   Serial.print(key01); Serial.print(":::"); Serial.println(val01);
                   keysize[num_jsons] = strlen(key01);
                   valsize[num_jsons] = strlen(val01);
               }
               else if (num_jsons == 2)
               {
                   key02 = i->key;
                   val02 = i->value;
                   Serial.print(key02); Serial.print(":::"); Serial.println(val02);
                   keysize[num_jsons] = strlen(key02);
                   valsize[num_jsons] = strlen(val02);
               }
              
            }//is json data
        }//end json object iterator
        
        //work on creating "radiopacket"
        int radiopacket_index = 0;
        radiopacket[0]=0x77;

        // mac_name[] is null terminated, but should be 12 characters long 
        Serial.print("reading mac:");
        
        for (int i = 0; i < 6; i++) 
        {
            radiopacket[i+1] = (toHex(mac_name[i * 2]) << 4) | toHex(mac_name[i * 2 + 1]);
        }
        
 
        Serial.print("volt=");
        
        radiopacket_index = 7;  //start of voltage

        radiopacket[radiopacket_index] = volt_val[0];  //radiopacket[7]

        Serial.print(radiopacket[radiopacket_index], HEX);
        radiopacket_index++;  //radiopacket[7]

        radiopacket[radiopacket_index] = volt_val[1];  //radiopacket[8]
        Serial.print(radiopacket[index], HEX);
        radiopacket_index++;  //radiopacket[8]

        radiopacket[radiopacket_index] = volt_val[2];  //radiopacket[9]
        Serial.print(radiopacket[radiopacket_index], HEX);
        radiopacket_index++;  //radiopacket[9]

        radiopacket[radiopacket_index] = volt_val[3];  //radiopacket[10]
        Serial.print(radiopacket[radiopacket_index], HEX);
        radiopacket_index++;  //radiopacket[11]

        //[10] = xmit count, 
        // cnt_val, size_of_cnt_val:  convert 0-255 to byte
        //radiopacket[radiopacket_index] = 150;  // count //radiopacket[10]
        radiopacket[radiopacket_index] = atoi(cnt_val);
        radiopacket_index++;  //radiopacket[11]

        //[11] = rssi
        //radiopacket[radiopacket_index] = -33;  // count //radiopacket[10]
        radiopacket[radiopacket_index] = atoi(rssi_val);
        radiopacket_index++;  //radiopacket[12]
        
        //12 = gateway
        radiopacket[radiopacket_index] = Gateway_ID;  // count //radiopacket[10]
        radiopacket_index++;  //radiopacket[13]
        

        //  "lo":-43.3433,"la":93.3433;  START OF JSON
        if (num_jsons >= 1)
        {
            radiopacket[radiopacket_index] = 0x22;  //"
            radiopacket_index++;  //radiopacket[14]
            for (int i=0;i<keysize[1]; i++)
            {
                radiopacket[radiopacket_index] = key01[i];
                radiopacket_index++;
            }
            radiopacket[radiopacket_index] = 0x22;  //"
            radiopacket_index++;
            radiopacket[radiopacket_index] = 0x3A;  //:
            radiopacket_index++;
            for (int i=0;i<valsize[1]; i++)
            {
                radiopacket[radiopacket_index] = val01[i];
                radiopacket_index++;
            }
        }

        if (num_jsons >= 2)
        {
            
            radiopacket[radiopacket_index] = ',';  //
            radiopacket_index++;
            radiopacket[radiopacket_index] = 0x22;  //"
            radiopacket_index++;   
            
            for (int i=0;i<keysize[2]; i++)
            {
                radiopacket[radiopacket_index] = key02[i];
                radiopacket_index++;
            }
            radiopacket[radiopacket_index] = 0x22;  //"
            radiopacket_index++;
            radiopacket[radiopacket_index] = 0x3A;  //:
            radiopacket_index++;
            for (int i=0;i<valsize[2]; i++)
            {
                radiopacket[radiopacket_index] = val02[i];
                radiopacket_index++;
            }
        }
        //null terminated
        radiopacket[radiopacket_index] = '\0';  //"
        
        Serial.print("first 6:");
        Serial.print(radiopacket[0]);
        Serial.print(radiopacket[1]);
        Serial.print(radiopacket[2]);
        Serial.print(radiopacket[3]);
        Serial.print(radiopacket[4]);
        Serial.print(radiopacket[5]);
        Serial.print("final radiopacket");  Serial.println(radiopacket);
        
        
        char mac2[6] = {0x11, 0x11, 0x11, 0x11, 0x11, 0x12};
        //char json_data = "1234567891012141618202224262830"; //30 byes
        xmit_cnt2++;
        char radiopacket2[220]="heloo";    //make this long enough to capture json string.
        uint8_t index2 = 0;
        

    
        rf95.send((uint8_t *)radiopacket, radiopacket_index + 1);
    }//end json.object.success
} //end routine localParseJson



void setup() {


   
  //Softserial used for test only
  Serial.begin(9600); //using for debug
  Serial.println("Hello, world? using softserial");
  Serial1.begin(9600); //use for incoming BLE string
  delay(10000);
  Serial.println("7 seconds over");

  delay(1000);
  //Serial.println(PMTK_Q_RELEASE);
  timer = millis();

  //LoRa stuff
  pinMode(RFM95_RST, OUTPUT);
  digitalWrite(RFM95_RST, HIGH);
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
  
  
  
  //change modulation scheme here; faster than 125kHz BW is unstable
  //Bw125Cr45Sf128, Bw = 125 kHz, Cr = 4/5, Sf = 128chips/symbol, CRC on. Default medium range.
  //Bw500Cr45Sf128, Bw = 500 kHz, Cr = 4/5, Sf = 128chips/symbol, CRC on. Fast+short range.
  //Bw31_25Cr48Sf512, Bw = 31.25 kHz, Cr = 4/8, Sf = 512chips/symbol, CRC on. Slow+long range.
  //Bw125Cr48Sf4096, Bw = 125 kHz, Cr = 4/8, Sf = 4096chips/symbol, CRC on. Slow+long range.
  //xmtr and rcvr must have exact same setting to work.
  /*
  if (!rf95.setModemConfig(RH_RF95::Bw31_25Cr48Sf512)) {
    Serial.println("setModemConfig failed");
    while (1);
  }
  Serial.println("Set setModemConfig to: something");
  */
  
  // Defaults after init are 434.0MHz, 13dBm, Bw = 125 kHz, Cr = 4/5, Sf = 128chips/symbol, CRC on

  // The default transmitter power is 13dBm, using PA_BOOST.
  // If you are using RFM95/96/97/98 modules which uses the PA_BOOST transmitter pin, then 
  // you can set transmitter powers from 5 to 23 dBm:
  rf95.setTxPower(23, false);
}//end setup
  
//45°03'23.7"N 93°00'54.0"W
float lon = -93.014995; //longtitude (west)
float lat = 45.056584; //latitude (north)

int16_t packetnum = 0;  // packet counter, we increment per xmission

float longitude = -52.95245; //GPS.longitude;
float latitude = -90.25458; //GPS.latitude

void loop() 
{



  if (Serial1.available() > 0)
  {
    
    //received serial line of json
    if (readline(Serial1.read(), serialbuffer, serialBufSize) > 0)
    {
      //digitalWrite(pinHandShake, HIGH);
      //digitalWrite(LED, LOW);  //LED on

      
      Serial.print("You entered: >");
      Serial.print(serialbuffer);
      Serial.println("<");

      
      localParseJson(serialbuffer); //call JSON iterator on this line from serial

    }

  }//end seria.available

}










