

#define XDEBUG
#define DEBUG
#define REMOVECONFIG
#define REMOVELOG
#define PACKEGESIZE 504
#define PACKAGEINTRACK 200
#include "esp_system.h"
const int wdtTimeout = 8000;  //time in ms to trigger the watchdog
hw_timer_t *timer = NULL;


void IRAM_ATTR resetModule() {
  ets_printf("reboot\n");
  esp_restart();
}
//#include <RingBuf.h>
//RingBuf *inputbuffer = RingBuf_new(PACKEGESIZE, PACKAGEINTRACK);
unsigned long nextTransmit;
//#include "FS.h" 
#include "SPIFFS.h" 
unsigned long lastsendtobs=0;

byte cmdbuff[20];
#include <BLEDevice.h>
#include <BLEServer.h>
//#include <BLEUtils.h>
#include <BLE2902.h>

esp_power_level_t btxp=ESP_PWR_LVL_N12;

#define TRANSPONDERLOGFILE "/translog.log"
// Singleton instance of the radio driver

BLEServer *pGServer = NULL;
BLECharacteristic * pTxCharacteristic;
bool deviceConnected = false;
bool oldDeviceConnected = false;
uint8_t txValue = 0;

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/
#define SERVICE_UUID "4fafc333-1fb5-459e-8fcc-c5c9c331914b"
//#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E" // UART service UUID
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

#define GREEN_PIN 32
#define RED_PIN 33
long startGreen=0;
#define GREENTIME 3000
long startRed=0;
#define REDTIME 5000
#define SHOWGREEN 1
#define SHOWRED 2
#define CONNECT 1
#define DISCONNECT 2

struct transponder_info
{
  char transponder_id=255;
  byte getcontrols[10];
};
//Как проверяем последовательность 
// Должны быть все отметки в нужном порядке (могут быть лишние)
#define CHECKSEQ 0x01


class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {

      pGServer->getAdvertising()->stop();
      deviceConnected = true;
      
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
    }
};

union ulongbyte
{ 
  double dl;
  unsigned long ul;
  byte b[4];
} ulb;

union doublebyte
{ 
  double dl;
  byte b[8];
} ;
     

class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
  /*    struct transponder_info incometransponder;*/
      std::string rxValue;
      union ulongbyte tmpDt;
      union doublebyte xtmpDt;

    rxValue = pCharacteristic->getValue();
//    inputbuffer->add(inputbuffer, rxValue.c_str()); 
#ifdef XDEBUG
 int k=0;
/*
   file.write((uint8_t*)(&gpsTime),sizeof(unsigned long));
   file.write((uint8_t*)(&gpsLat),sizeof(double));
   file.write((uint8_t*)(&gpsLng),sizeof(double));
   file.write((uint8_t*)(&gpsSat),sizeof(double));
   file.write((uint8_t*)(&gpsHdop),sizeof(double));
*/
 timerWrite(timer, 0); //reset timer (feed watchdog)
 disableCore0WDT();
 if(rxValue.length()==14)
  {
#ifdef XDEBUG
      Serial.print("BLE MAC:");
      for(int i=0;i<6;i++)
       {
       Serial.print(rxValue[k],HEX);
       k++;
       }
      Serial.print(" GPS Date:");
      for(int i=0;i<4;i++)
       {
       tmpDt.b[i]=rxValue[k];
       k++;
       }
      Serial.print(tmpDt.ul);
      Serial.print(" Data size:");
      for(int i=0;i<4;i++)
       {
       tmpDt.b[i]=rxValue[k];
       k++;
       }
      Serial.print(tmpDt.ul);
#endif
#ifndef XDEBUG
    for(int i=0;i<rxValue.length();i++)
     {
     Serial.write(rxValue[i]);
     }

#endif       

/*    bm=(byte*)mydev.getNative();
    byte h=0;
    for(byte i=0;i<6;i++)
     {
     frow[h]=bm[i];
     h++;
     }
   union ulb tmpdat;
   tmpdat.ul=gps.date.value();
   for(byte i=0;i<4;i++)
    {
      frow[h]=tmpdat.b[i];
      h++;
    }
    union ulb fsize;

   fsize.ul=file.size();
   for(byte i=0;i<4;i++)
    {
      frow[h]=fsize.b[i];
      h++;
    }
*/


    
  }
 else
  { 
 while(k<rxValue.length()-1)
  {
      Serial.print("Time:");
      for(int i=0;i<4;i++)
       {
       tmpDt.b[i]=rxValue[k];
       k++;
       }
      Serial.print(tmpDt.ul);
      Serial.print(" Lat:");
      for(int i=0;i<8;i++)
       {
       xtmpDt.b[i]=rxValue[k];
       k++;
       }
      Serial.print(xtmpDt.dl,6);
      Serial.print(" Lng:");
      for(int i=0;i<8;i++)
       {
       xtmpDt.b[i]=rxValue[k];
       k++;
       }
      Serial.print(xtmpDt.dl,6);
      Serial.print(" Sat:");
      for(int i=0;i<4;i++)
       {
       tmpDt.b[i]=rxValue[k];
       k++;
       }
      Serial.print(tmpDt.ul);
      Serial.print(" Hdop:");
      for(int i=0;i<4;i++)
       {
       tmpDt.b[i]=rxValue[k];
       k++;
       }
      Serial.print(tmpDt.ul);
      Serial.println("");     
   }
      Serial.print(" CSum:");
      Serial.print((unsigned char)rxValue[k],HEX);
 Serial.println("");

#endif
#ifndef XDEBUG
    for(int i=0;i<rxValue.length();i++)
     {
     Serial.write(rxValue[i]);
     }
#endif
    }
   }   
};


//----------------------------------------------------------------------------------------------------
void setup() 
{
int readfromconf=0;
byte noconfile=true;
//read config
 Serial.begin(230400);

//init BLE
 // BLEDevice::setPower(BLETXPOWER);
BLEDevice::init("xxx srv");
  BLEDevice::setMTU(512);

  BLEDevice::setPower(btxp);
byte *xx;
 BLEServer *pGServer = BLEDevice::createServer(); 
 BLEService *pService = pGServer->createService(SERVICE_UUID); 
 pGServer->setCallbacks(new MyServerCallbacks());
  // Create a BLE Characteristic
  pTxCharacteristic = pService->createCharacteristic(
                    CHARACTERISTIC_UUID_TX,
                    BLECharacteristic::PROPERTY_NOTIFY
                  );
                      
  pTxCharacteristic->addDescriptor(new BLE2902());

  BLECharacteristic * pRxCharacteristic = pService->createCharacteristic(
                       CHARACTERISTIC_UUID_RX,
                       BLECharacteristic::PROPERTY_READ 
                    );

  pRxCharacteristic->setCallbacks(new MyCallbacks());
//pRxCharacteristic->setValue("Char1");
  // Start the service
  pService->start();

  // Start advertising
  pGServer->getAdvertising()->start();
  Serial.println("Waiting a client connection to notify...");
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);  // functions that help with iPhone connections issue
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();
  #ifdef DEBUG
  Serial.println("Characteristic defined! Now you can read it in your phone!");
  #endif
  //WDT
  timer = timerBegin(0, 80, true);                  //timer 0, div 80
  timerAttachInterrupt(timer, &resetModule, true);  //attach callback
  timerAlarmWrite(timer, wdtTimeout * 1000, false); //set time in us
  timerAlarmEnable(timer);                          //enable int

}
//-----------------------------------------------------------------------------------
void loop() {
  timerWrite(timer, 0); //reset timer (feed watchdog)
  // ble part ---------------------------------------------
enableCore0WDT();
if (deviceConnected) {
     /* char x[]={}
        pTxCharacteristic->setValue((byte*)x, 101);
        pTxCharacteristic->notify();
        txValue++;*/
      
         //pGServer->getAdvertising()->stop();
         BLEAdvertising* blead=BLEDevice::getAdvertising();
         blead->stop();
         blead->setScanResponse(false);
         
    delay(100); // bluetooth stack will go into congestion, if too many packets are sent
  }
  else
  {
     /* char x[]={}
        pTxCharacteristic->setValue((byte*)x, 101);
        pTxCharacteristic->notify();
        txValue++;*/
      
         //pGServer->getAdvertising()->stop();
         BLEAdvertising* blead=BLEDevice::getAdvertising();
         blead->start();
         blead->setScanResponse(true);
         
    delay(100); // bluetooth stack will go into congestion, if too many packets are sent
  }
  
  /*  if (!deviceConnected && oldDeviceConnected) {
        delay(500); // give the bluetooth stack the chance to get things ready
         Serial.println("start advertising");
        pServer->startAdvertising(); // restart advertising
        Serial.println("started advertising");
        oldDeviceConnected = deviceConnected;
    }
    if (deviceConnected && !oldDeviceConnected) {
    // do stuff here on connecting
        oldDeviceConnected = deviceConnected;
        pServer->getAdvertising()->stop();
#ifdef DEBUG
Serial.println("Stop advertisiing");
#endif

    }
*/
 
}
