/* mbed Microcontroller Library
 * Copyright (c) 2006-2015 ARM Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 
 Eric Tsai
 
Scans for advertisements and captures the ones identified as mine
Takes UUID fields and injects the MAC and RSSI data.  Assumes UUID JSON data.

Sends this string
Given this ADV data that has properly JSON formatted key/values.
"volt":3.05,"mag":1

Sends via uart this string:
{"mac":"c2f154bb0af9","rssi":-55,"volt":3.05,"mag":1}

Tested:  works.

To Do:  Add hash function to avoid transmitting via uart repeated ADV's, especially since there's 3 channels.
Take care of comment uart.

 */

#include "mbed.h"
#include "ble/BLE.h"

//DigitalOut led1(LED1, 1);
Ticker     ticker;



Serial device(p9, p11);  //nRF51822 uart :  TX=p9.  RX=p11

void periodicCallback(void)
{
    //led1 = !led1; /* Do blinky on LED1 while we're waiting for BLE events */
}

void advertisementCallback(const Gap::AdvertisementCallbackParams_t *params) {


    //BLE MAC Address (6 bytes):
    //      params->peerAddr[5]
    //      params->peerAddr[4]
    //      ...
    //      params->peerAddr[0]
    //RSSI = params->rssi
    //Payload
    //  params->advertisingData[#<advertisingDataLen]
    

    
    //              [<---MY DATA-->]
    //0x02      0x01    0x06    0x06    0xff    D   E   C   ?  ?
    //0         1       2       3       4       5   6   7   8   9

    if ( (params->advertisingDataLen) >= 8)
    {
        //if one of "ours", these UUID fields will be this value.
        if ( (params->advertisingData[5] == 0x44) && (params->advertisingData[6] == 0x45) && (params->advertisingData[7]==0x43) )
        {
            /*
            BLE Received from MAC C2F154BB0AF9
            volt:3.11,mag:1
            
            uart-transmit:
            mac:C2F154BB0AF9,rssi:##,volt:3.11,mag:1
            
            /ble/C2F154BB0AF9/rssi
            /ble/C2F154BB0AF9/1st_token
            /ble/c2F153
            
            */
            /*
            device.printf("Adv peerAddr: [%02x%02x%02x%02x%02x%02x] rssi %d, ScanResp: %u, AdvType: %u\r\n",
              params->peerAddr[5], params->peerAddr[4], params->peerAddr[3], params->peerAddr[2], params->peerAddr[1], params->peerAddr[0],
              params->rssi, params->isScanResponse, params->type);
            */
            device.printf("{\"mac\":\"%02x%02x%02x%02x%02x%02x\",\"rssi\":%d,",
                params->peerAddr[5], 
                params->peerAddr[4], 
                params->peerAddr[3], 
                params->peerAddr[2], 
                params->peerAddr[1], 
                params->peerAddr[0],
                params->rssi
                );
            //mac:"c2f154bb0af9"
            for (unsigned index = 8; index < params->advertisingDataLen; index++)
            {
                //device.printf("%02x", params->advertisingData[index]);
                device.printf("%c", params->advertisingData[index]);
            }
            device.printf("}");
            device.printf("\r\n");
            

            
        }//end if it's our adv
    }//end if advertisingDataLen




}//end advertisementCallback

/**
 * This function is called when the ble initialization process has failed
 */
void onBleInitError(BLE &ble, ble_error_t error)
{
    /* Initialization error handling should go here */
    device.printf("periodic callback ");
    device.printf("\r\n");
}

/**
 * Callback triggered when the ble initialization process has finished
 */
void bleInitComplete(BLE::InitializationCompleteCallbackContext *params)
{
    BLE&        ble   = params->ble;
    ble_error_t error = params->error;

    if (error != BLE_ERROR_NONE) {
        /* In case of error, forward the error handling to onBleInitError */
        onBleInitError(ble, error);
        return;
    }

    /* Ensure that it is the default instance of BLE */
    if(ble.getInstanceID() != BLE::DEFAULT_INSTANCE) {
        return;
    }
 
    // in ms.  Duty cycle = (interval / window);  200ms/500ms = 40%
    ble.gap().setScanParams(500 /* scan interval */, 200 /* scan window */);
    ble.gap().startScan(advertisementCallback);
}

int main(void)
{
    device.baud(9600);
    device.printf("started main... ");
    device.printf("\r\n");
    ticker.attach(periodicCallback, 5);

    BLE &ble = BLE::Instance();
    ble.init(bleInitComplete);

    while (true) {
        ble.waitForEvent();
    }
}
