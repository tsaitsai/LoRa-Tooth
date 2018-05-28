/*
Author:  Eric Tsai, 2018
License:  CC-BY-SA, https://creativecommons.org/licenses/by-sa/2.0/
File: RFM95_Receive.ino
Purpose:  example to send GPS coordinates via LoRa.

Credit:  Original LoRa send code from Adafruit LoRa example.
*/
#include <Adafruit_GPS.h>
#include <SoftwareSerial.h>



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


// If using hardware serial, comment
// out the above two lines and enable these two lines instead:
Adafruit_GPS GPS(&Serial1); //Serial1 is the hardware uart (using for GPS coordinates)
// If using software serial, keep these lines enabled
// (you can change the pin numbers to match your wiring):
//SoftwareSerial Serial(6, 5);  // RX, TX, we only need TX

uint32_t timer;

char mac[6] = {0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};



char volt[5] = "3.14"; //4 bytes long
char json_data = "1234567891012141618202224262830"; //30 byes
uint8_t xmit_cnt = 1;  //only 1 byte for transmit counter;
int index;  //radiopacket index value;

void setup() {

  //Softserial used for test only
  Serial.begin(9600); //using for debug
  Serial.println("Hello, world? using softserial");
  
  //Serial1.begin(115200); // gps module baud
  // 9600 NMEA is the default baud rate for Adafruit MTK GPS's- some use 4800
  GPS.begin(9600);
  // uncomment this line to turn on RMC (recommended minimum) and GGA (fix data) including altitude
  GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCGGA);
  // uncomment this line to turn on only the "minimum recommended" data
  //GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCONLY);
  // For parsing data, we don't suggest using anything but either RMC only or RMC+GGA since
  // the parser doesn't care about other sentences at this time
  // Set the update rate
  GPS.sendCommand(PMTK_SET_NMEA_UPDATE_200_MILLIHERTZ);   // 5 second update
  // For the parsing code to work nicely and have time to sort thru the data, and
  // print it out we don't suggest using anything higher than 1 Hz
  // Request updates on antenna status, comment out to keep quiet
  GPS.sendCommand(PGCMD_ANTENNA);
  delay(1000);
  Serial.println(PMTK_Q_RELEASE);
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

void loop() {

  char c = GPS.read();
  
  //Serial.write(c);
  
  
  // if a sentence is received, we can check the checksum, parse it...
  if (GPS.newNMEAreceived()) 
  {
    // a tricky thing here is if we print the NMEA sentence, or data
    // we end up not listening and catching other sentences! 
    // so be very wary if using OUTPUT_ALLDATA and trytng to print out data
    //Serial.println(GPS.lastNMEA());   // this also sets the newNMEAreceived() flag to false
    
    if (!GPS.parse(GPS.lastNMEA()))   // this also sets the newNMEAreceived() flag to false
      return;  // we can fail to parse a sentence in which case we should just wait for another
  }
  
  
  // if millis() or timer wraps around, we'll just reset it
  if (timer > millis())  timer = millis();
  
  if (millis() - timer > 8000) { 
    timer = millis(); // reset the timer
    xmit_cnt++;
    
    
    Serial.print("\nTime: ");
    Serial.print(GPS.hour, DEC); Serial.print(':');
    Serial.print(GPS.minute, DEC); Serial.print(':');
    Serial.print(GPS.seconds, DEC); Serial.print('.');
    Serial.println(GPS.milliseconds);
    Serial.print("Date: ");
    Serial.print(GPS.day, DEC); Serial.print('/');
    Serial.print(GPS.month, DEC); Serial.print("/20");
    Serial.println(GPS.year, DEC);
    Serial.print("Fix: "); Serial.print((int)GPS.fix);
    Serial.print(" quality: "); Serial.println((int)GPS.fixquality); 
    if (GPS.fix) {
      Serial.print("Location: ");
      Serial.print(GPS.latitude, 4); Serial.print(GPS.lat);
      Serial.print(", "); 
      Serial.print(GPS.longitude, 4); Serial.println(GPS.lon);
      
      Serial.print("Speed (knots): "); Serial.println(GPS.speed);
      Serial.print("Angle: "); Serial.println(GPS.angle);
      Serial.print("Altitude: "); Serial.println(GPS.altitude);
      Serial.print("Satellites: "); Serial.println((int)GPS.satellites);
    }


    //char radiopacket[20] = "Hello World #      ";
    //itoa(packetnum++, radiopacket+13, 10);
    //Serial.print("Sending "); Serial.println(radiopacket);
    //radiopacket[19] = 0;
    
    
    char char_long[14]=""; int char_long_length=sizeof(char_long);
    char char_lat[14]=""; int char_lat_length=sizeof(char_lat);
    for (int i=0; i<sizeof(char_long); i++) //don't need to null char this array.
    {
        char_long[i] = 0;   //also, wrong null char
        char_lat[i] = 0;
    }
    
    //float longitude = GPS.longitude;  //not this one, this gives incompatible coordaintes for google maps
    longitude = GPS.longitudeDegrees;   //use this one, want degrees for google maps
    //longitude = decimalDegrees(longitude);    //trash

    
    //float latitude = GPS.latitude;    //no, don't use this one.  doesn't work for google maps
    latitude = GPS.latitudeDegrees; //use this one for google maps
    
    //latitude = decimalDegrees(latitude);
    uint8_t gpsseconds = GPS.seconds;
    Serial.print("logitude: ");
    Serial.print(longitude, 5);
    Serial.print("       latitude: ");
    Serial.print(latitude, 5);
    Serial.print("       time: ");
    Serial.println(gpsseconds);
    //--------------------------------------------------------------------------------
    
    //convert float to char[]
    dtostrf(longitude, sizeof(char_long), 5, char_long); //float var, width=13 or whatever, precision=5, returns char*
    dtostrf(latitude, sizeof(char_lat), 5, char_lat); //float var, width=13 or whatever, precision=5, returns char*
    Serial.print("*****this is charlong");
    Serial.println(char_long);
    

    Serial.print("***** this is mac");
    Serial.print(mac[0], HEX);
    Serial.print(mac[1], HEX);
    Serial.print(mac[2], HEX);
    Serial.print(mac[3], HEX);
    Serial.print(mac[4], HEX);
    Serial.print(mac[5], HEX);
    /*  Notes
        char_long_length = length of conversion (integer, 8 or 9 or 10)
        char_long = lontitude in char[], including leading blank characters
        long_shift = number of blank space in char_long
    
    */
    
    

    Serial.print("this is the float:");
    Serial.print(longitude);
    for (int i=0; i<sizeof(char_long); i++)
    {
        //Serial.print(i);
        //Serial.print("=");
        //Serial.print(char_long[i]);
        //Serial.print(", ");
        if (char_long[i] == ' ')
        {
            char_long_length--;
        }

    }
    
    
    for (int i=0; i<sizeof(char_lat); i++)
    {
        if (char_lat[i] == ' ')
        {
            char_lat_length--;
        }
    }
    
    
    
    
    int char_long_size = sizeof(char_long); Serial.print("***size of char_long = "); Serial.println(char_long_size);  //14
    int long_shift = char_long_size - char_long_length;Serial.print("**size of long_shift = "); Serial.println(long_shift); //5
    int char_lat_size = sizeof(char_lat); Serial.print("***size of char_long = "); Serial.println(char_lat_size);  //14
    int lat_shift = char_lat_size - char_lat_length;Serial.print("**size of long_shift = "); Serial.println(long_shift); //5
    //long_shift = 5, char_long_length = 9;
    Serial.println("arraying...");
    
    
    
    /*
    Must left shift dtostrf results
    */
    
    for (int i=0; i<sizeof(char_long); i++)
    {
      if (i < char_long_length) //if less than size of float string
      {
        char_long[i] = char_long[i+long_shift]; //left shift chars by long_shift
        Serial.print(char_long[i]);
      }
      else
      {
        char_long[i] = 0x00;  //'nul'
        Serial.print(i);
      }
    }//end for longitude shift

    for (int i=0; i<sizeof(char_lat); i++)
    {
      if (i < char_lat_length) //if less than size of float string
      {
        char_lat[i] = char_lat[i+lat_shift]; //left shift chars by long_shift
        //Serial.print(char_long[i]);
      }
      else
      {
        char_lat[i] = 0x00;  //'nul'
        //Serial.print(i);
      }
    }//end for longitude shift
    
    


    
    /* 
    
    Radio packet to send:
    MAC[6 bytes], volt [4 bytes], xmit count [1 byte], rssi[1 byte], gateway #[1 byte], sensor json
    
    For a LoRa sensor:
    rssi=0
    gateway=0
    
    */
    char radiopacket[220]="hello";    //make this long enough to capture json string.
    index = 1;  //reset character counter to 0
    radiopacket[0]=77;  //arbitrary, byte 0 not used.
    //MAC[6 bytes],
    for (int i=0; i<6; i++)
    {
        radiopacket[index] = mac[i];
        Serial.print(radiopacket[index], HEX);
        index++;
    }//index @ 
    index = 7;
    
    
    //volt [4 bytes], just fill in for now.
    radiopacket[index]='1';  //radiopacket[6]
    Serial.print(radiopacket[index]);
    index++;    //
    radiopacket[index]='.';  //radiopacket[7]
    Serial.print(radiopacket[index]);
    index++;    
    radiopacket[index]='1';  //radiopacket[8]
    Serial.print(radiopacket[index]);
    index++;
    radiopacket[index]='1';  //radiopacket[9]
    Serial.print(radiopacket[index]);
    index++;  //radiopacket[10]

    //xmit count [1 byte]
    radiopacket[index] = xmit_cnt;  //radiopacket[10]
    Serial.print(radiopacket[index], DEC);
    index++;    //radiopacket[11]
    
    //rssi[1 byte]
    radiopacket[index] = 0;  //radiopacket[11]
    index++;    //radiopacket[12]
    
    //gateway [1 byte]
    radiopacket[index] = 0;  //radiopacket[12], 0 for LoRa Sensor
    index++;    //radiopacket[13]
    
    
    // json looks like this:
    //           "lo":xx.xxxx,"la":xxx.xxxxx
    radiopacket[index] = 0x22;  //"
    index++;
    radiopacket[index] = 'l';
    index++; 
    radiopacket[index] = 'o';
    index++; 
    radiopacket[index] = 0x22;  //"
    index++;
    radiopacket[index] = 0x3A;  //:
    index++;
    for (int i=0; i<char_long_length; i++)
    {
        radiopacket[index] = char_long[i];
        Serial.print(radiopacket[index]);
        index++;
    }//index @ 
    
    radiopacket[index] = ',';
    
    index++;
    radiopacket[index] = 0x22;  //"
    index++;
    radiopacket[index] = 'l';
    index++; 
    radiopacket[index] = 'a';
    index++; 
    radiopacket[index] = 0x22;  //"
    index++;
    radiopacket[index] = 0x3A;  //:
    index++;
    for (int i=0; i<char_long_length; i++)
    {
        radiopacket[index] = char_lat[i];
        Serial.print(radiopacket[index]);
        index++;
    }//index @ 
    //radiopacket[index] = '}';
    //index++;
    radiopacket[index] = '\0';   //null terminate json string
    
    rf95.send((uint8_t *)radiopacket, index + 1);
    rf95.waitPacketSent();
    // Now wait for a reply
    uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
    uint8_t len = sizeof(buf);
  

   
    
  }

}









