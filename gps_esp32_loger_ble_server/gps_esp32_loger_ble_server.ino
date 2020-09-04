

#define XDEBUG
#define DEBUG
#define REMOVECONFIG
#define REMOVELOG
#define PACKEGESIZE 504
#define PACKAGEINTRACK 200
#include "esp_system.h"
const int wdtTimeout = 8000;  //time in ms to trigger the watchdog
hw_timer_t *timer = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;

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

#define TRACKFILE "/track.log"
// Singleton instance of the radio driver

BLEServer *pGServer = NULL;
BLECharacteristic * pTxCharacteristic;

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
#ifdef DEBUG
Serial.println("got Disconnect");
#endif
    }
};

union ulongbyte
{ 
  double dl;
  unsigned long ul;
  byte b[4];
} ulb,trkdt,filesize;
char strTrkDt[21];
union doublebyte
{ 
  double dl;
  byte b[8];
} ;
byte gotNewTrack=0;    
  File Gfile;
byte blemac[6];
#ifdef XDEBUG
void savetoGPX()
{
 int k; 
 char rxValue[512];

Gfile= SPIFFS.open(TRACKFILE,FILE_READ);
if(Gfile)
 {
Gfile.read((uint8_t*)rxValue,14);

Serial.println("<?xml version=\"1.0\" encoding=\"UTF-8\"?><gpx version=\"1.1\" creator=\"Run.GPS Community Server\" ");
Serial.println(" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" ");
Serial.println(" schemaLocation=\"http://www.topografix.com/GPX/1/1/gpx.xsd");
Serial.println(" http://www.garmin.com/xmlschemas/TrackPointExtension/v1 ");
Serial.println("http://www.garmin.com/xmlschemas/TrackPointExtensionv1.xsd\" ");
Serial.println("xmlns=\"http://www.topografix.com/GPX/1/1\"");
Serial.println(" xmlns:gpxtpx=\"http://www.garmin.com/xmlschemas/TrackPointExtension/v1\">");
Serial.println(" <metadata>");
Serial.println(" <name></name>");
Serial.print(" <author>");      

for(int i=0;i<6;i++)
       {
       Serial.print(rxValue[k],HEX);
       k++;
       }
for(int i=0;i<4;i++)
       {
       trkdt.b[i]=rxValue[k];
       k++;
       }
for(int i=0;i<4;i++)
       {
       filesize.b[i]=rxValue[k];
       k++;
       }
Serial.println("</author>");
Serial.println("  </metadata><trk><name></name><desc></desc><trkseg>");

//      Serial.print(" GPS Date:");
       sprintf(&(strTrkDt[4]),"%06d",trkdt.ul);
       
       strTrkDt[0]='2';
       strTrkDt[1]='0';
      //  strTrkDt[2]='x';
      // strTrkDt[3]='x';
   strTrkDt[2]=strTrkDt[8];
       strTrkDt[3]=strTrkDt[9];
       strTrkDt[8]=strTrkDt[4];  
       strTrkDt[9]=strTrkDt[5];   
       strTrkDt[5]=strTrkDt[6];  
       strTrkDt[6]=strTrkDt[7];  
       strTrkDt[4]='-';
       strTrkDt[7]='-';
       strTrkDt[10]='T';
//      Serial.print(tmpDt.ul);
//      Serial.print(" Data size:");
 //     Serial.print(tmpDt.ul);

//----------------------------------------------------------
 union ulongbyte pointTime,pointSat,pointHDOP;
 union doublebyte pointLat,pointLng;

 while(Gfile.available())
  {
 Gfile.read((uint8_t*)rxValue,505);
 k=0;
 while(k<504)
  {
    
     
 //     Serial.print("Time:");
      for(int i=0;i<4;i++)
       {
       pointTime.b[i]=rxValue[k];
       k++;
       }
  //    Serial.print(tmpDt.ul);
  //    Serial.print(" Lat:");
      for(int i=0;i<8;i++)
       {
       pointLat.b[i]=rxValue[k];
       k++;
       }
   //   Serial.print(xtmpDt.dl,6);
   //   Serial.print(" Lng:");
      for(int i=0;i<8;i++)
       {
       pointLng.b[i]=rxValue[k];
       k++;
       }
  //    Serial.print(xtmpDt.dl,6);
 //     Serial.print(" Sat:");
      for(int i=0;i<4;i++)
       {
       pointSat.b[i]=rxValue[k];
       k++;
       }
//      Serial.print(tmpDt.ul);
/// Serial.print(" Hdop:");
      for(int i=0;i<4;i++)
       {
       pointHDOP.b[i]=rxValue[k];
       k++;
       }
//      Serial.print(tmpDt.ul);
 Serial.print("<trkpt  lon=\"");
 Serial.print(pointLng.dl,6);
 Serial.print("\" lat=\"");
 Serial.print(pointLat.dl,6);
  Serial.print("\"> <time>");
 sprintf(&strTrkDt[12],"%08d",pointTime.ul);
 
//13:27:54:00
        strTrkDt[18]= strTrkDt[17];  // 0   // 4         
         strTrkDt[17]=strTrkDt[16];  // 4   // 5 ;      
       strTrkDt[11]= strTrkDt[12];   //     // 1         
       strTrkDt[12]=  strTrkDt[13];  // 1      // 3     
       strTrkDt[13]=':';            // 3   //":"
   //    strTrkDt[14]=;     // 2
   //    strTrkDt[15]=;     //7  
       strTrkDt[16]=':';     // 5 
       strTrkDt[19]='Z';     // 0
       Serial.print(strTrkDt);

      Serial.print("</time>");
      Serial.print("<sat>"); 
      Serial.print(pointSat.ul);    
      Serial.print("</sat>");      
      Serial.print("<hdop>"); 
      Serial.print((double)pointHDOP.ul/100.0,2);    
      Serial.println("</hdop></trkpt>");      
    }
   //   Serial.print(" CSum:");
   //   Serial.print((unsigned char)rxValue[k],HEX);
  }
 Serial.println("</trkseg></trk></gpx>");
Gfile.close();
SPIFFS.remove(TRACKFILE);
 }
else
 {
  Serial.println("No TRACKFILE found.");
 }
}
#endif


  
class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
  /*    struct transponder_info incometransponder;*/
      std::string rxValue;
      union ulongbyte tmpDt;
      union doublebyte xtmpDt;

    rxValue = pCharacteristic->getValue();



 timerWrite(timer, 0); //reset timer (feed watchdog)
 disableCore0WDT();
 if(rxValue.length()==14)
  {

#ifdef XDEBUG 
gotNewTrack=1;    //В режиме XDEBUG (генерация XML) сперва все пишем в файл. После перезапуска если есть файл то выдаем его в Serial и удаляем
Gfile= SPIFFS.open(TRACKFILE,FILE_WRITE);
    Gfile.write((uint8_t*)&((rxValue.c_str())[0]),rxValue.length());
//   for(int i=0;i<rxValue.length();i++)
//     {
//     Gfile.write(rxValue[i]);
//     }
#endif
#ifndef XDEBUG
    for(int i=0;i<rxValue.length();i++)
     {
     Serial.write(rxValue[i]);
     }
#endif       
   
  }
 else
  { 
  if(rxValue.length()==5)
  {
#ifdef XDEBUG   
  Serial.println("Got Full Track rebooting");
   Serial.print("filesize send:");
   Serial.println(filesize.ul);
   Serial.print("filesize writen:");
   Serial.print(Gfile.size());
   
   Serial.println("");
   Gfile.close();
   delay(100);
#endif
  esp_restart();  
  }
 else
  {
#ifdef XDEBUG
  // for(int i=0;i<rxValue.length();i++)
     {
     Gfile.write((uint8_t*)&((rxValue.c_str())[0]),rxValue.length());
     }
#endif
#ifndef XDEBUG
    for(int i=0;i<rxValue.length();i++)
     {
     Serial.write(rxValue[i]);
     }
#endif
    }
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
#ifdef XDEBUG
if(!SPIFFS.begin(true)){
        Serial.println("ERROR: Card Mount Failed");
        byte c=LOW;;
        while(1)
        {
 //          digitalWrite(BLINK_LED,c);
 //          c=(!c);
           delay(100);
        }
}

savetoGPX();
#endif
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
  //Serial.println("Waiting a client connection to notify...");
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);  // functions that help with iPhone connections issue
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();
  #ifdef DEBUG
  Serial.println("Ready");
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
        oldDeviceConnected=true;
         //pGServer->getAdvertising()->stop();
         BLEAdvertising* blead=BLEDevice::getAdvertising();
         blead->stop();
         blead->setScanResponse(false);
         
    delay(100); // bluetooth stack will go into congestion, if too many packets are sent
  }
  else
  {        /* char x[]={}
        pTxCharacteristic->setValue((byte*)x, 101);
        pTxCharacteristic->notify();
        txValue++;*/
        if(oldDeviceConnected)
         {
         Serial.println("</trkseg></trk></gpx>");
         Serial.println("");
         oldDeviceConnected=false;
        
         //pGServer->getAdvertising()->stop();
         BLEAdvertising* blead=BLEDevice::getAdvertising();
         blead->start();
         blead->setScanResponse(true);
         
    delay(100); // bluetooth stack will go into congestion, if too many packets are sent
         }
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
