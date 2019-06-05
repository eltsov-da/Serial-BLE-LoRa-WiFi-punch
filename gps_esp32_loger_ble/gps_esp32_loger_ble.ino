#define DEBUG
#define TRACKFILE "/track.log"
#define CONFIGFILE "/config.log"
#define BLINK_LED 22
#define BUTTON_PIN_BITMASK 0x200000000 // это 2 в 33 степени
//GPIO_NUM_33 
#include "esp_system.h"
#include "SPIFFS.h" 
#include "BLEDevice.h"
#include "TinyGPS++.h"
TinyGPSPlus gps;
#define LONGPRESSTIME 500
#define DOUBLEPRESSTIME 2000
#define SINGLEPRESS 1
#define DOUBLEPRESS 2
#define LONGPRESS 4
byte buttonstatus=0;
#define GPSDELTA 1000
#define GPSSEEK 0
#define GPSREADY 1
#define GETGPSTREK 2
#define COMMANDMODE 128
#define DELTAMODE 5000
static byte systemMode=0;
static byte systemPreMode=0;
static unsigned long modestarttime=0;
unsigned long savedbytes=0;
union ulb
{ 
  unsigned long ul;
  byte b[4];
};

union dlb
{ 
  double dl;
  byte b[8];
};
//-----------------------------------------------------------------------------

const int wdtTimeout = 8000;  //time in ms to trigger the watchdog
hw_timer_t *timer = NULL;


void IRAM_ATTR resetModule() {
  ets_printf("reboot\n");
  esp_restart();
}
//------------------------------------------------------------------------------
unsigned long lastGPSpoint=0; //внутренее время когда взяли последнюю GPS точку
void GPS_getpoint()
{
  lastGPSpoint=millis();
  while (Serial1.available() > 0)
   {
    gps.encode(Serial1.read());
   }
  
//  if (gps.time.isUpdated())
  {
  
  File file = SPIFFS.open(TRACKFILE,FILE_APPEND);
  if(file)
   {
   if(file.size()>0)
   {
   file.seek(file.size());
   }
   unsigned long gpsTime=gps.time.value(); 
   double gpsLat=gps.location.lat();
   double gpsLng=gps.location.lng();
    unsigned long gpsSat=gps.satellites.value(); 
     unsigned long gpsHdop=gps.hdop.value();
   file.write((uint8_t*)(&gpsTime),sizeof(unsigned long));
   file.write((uint8_t*)(&gpsLat),sizeof(double));
   file.write((uint8_t*)(&gpsLng),sizeof(double));
   file.write((uint8_t*)(&gpsSat),sizeof(unsigned long));
   file.write((uint8_t*)(&gpsHdop),sizeof(unsigned long));
#ifdef DEBUG
 Serial.print("File size:");
 Serial.println(file.size());
 Serial.println(sizeof(double));
#endif

   file.close();
#ifdef DEBUG
 Serial.print("Point Time:");
 Serial.println(gpsTime);
 Serial.print("Point lat:");
 Serial.println(gpsLat,6);
 Serial.print("Point Lng:");
 Serial.println(gpsLng,6);
 Serial.print("Point Sat:");
 Serial.println(gpsSat,6);

#endif
   }
#ifdef DEBUG
else
 {
  Serial.println("File Error");
 }
#endif
   
  }
}
byte UpDownFlag=LOW;  //Флаг нажатия кнопки на 33 пине
//----------------------------------------------------------------
byte button_press()
{

unsigned long timeUpDownFlagChange=0;

  //byte buttonStatus=digitalRead(GPIO_NUM_33);
  if(digitalRead(GPIO_NUM_33)==HIGH && UpDownFlag==LOW) //Если кнопка нажата - блокируем все и ждем, что будет
   {
   UpDownFlag=HIGH; 
   timeUpDownFlagChange=millis();
   while((digitalRead(GPIO_NUM_33)==HIGH) && (timeUpDownFlagChange+LONGPRESSTIME>millis()))
    {
#ifdef DEBUG
       Serial.println("Long wait");
#endif       
    delay(100);  
    digitalWrite(BLINK_LED,HIGH);  //ждем сколько держат кнопку 
    }
   if(digitalRead(GPIO_NUM_33)==HIGH)
    {
      return(LONGPRESS);  //Если после выхода из цикла кнопка все еще нажата это LONGPRESS
    }
   else
    {
    timeUpDownFlagChange=millis();
    while((digitalRead(GPIO_NUM_33)==LOW) && (timeUpDownFlagChange+DOUBLEPRESSTIME>millis()))
     {;
      delay(100);
#ifdef DEBUG
  Serial.println("double wait");
#endif
      digitalWrite(BLINK_LED,LOW);  // Если кнопка отпущена выключаем диод
     }
    if(digitalRead(GPIO_NUM_33)==HIGH)  // Убеждаемся, что это не дребезг при длительном нажатии
     {
     while((digitalRead(GPIO_NUM_33)==HIGH) && (timeUpDownFlagChange+LONGPRESSTIME>millis()))
      {
    #ifdef DEBUG
       Serial.println("Long wait");
    #endif       
    delay(100);  
    digitalWrite(BLINK_LED,HIGH);  //ждем сколько держат кнопку 
      }
      if(digitalRead(GPIO_NUM_33)==HIGH)
      {
      return(LONGPRESS);  //Если после выхода из цикла кнопка все еще нажата это LONGPRESS
      }
     else
      {  
      return(DOUBLEPRESS);
      }
     }
    else
     {
      return(SINGLEPRESS);
     }

    }
   } //  if(buttonStatus==HIGH)
/*if(UpDownFlag!=digitalRead(GPIO_NUM_33))
 {
 Serial.print("Pin State changed");
 }*/
UpDownFlag=digitalRead(GPIO_NUM_33);
delay(100);
return(0);
}
//-------------------------------------------------------------------------------------

void gosleep(byte smode)
{
#ifdef DEBUG
 Serial.println("Go sleep");
#endif

  File    file = SPIFFS.open(CONFIGFILE,FILE_WRITE);
      if(file)
       {
       file.write(smode); 
       file.close();
       }
delay(5000);
esp_sleep_enable_ext0_wakeup(GPIO_NUM_33,1);  
esp_deep_sleep_start();
}

//---------------------------------------------------------------------------------
#define SERVICE_UUID "4fafc333-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

byte  doConnect=false;
byte xconnected=false;
BLEAddress condev= BLEAddress((uint8_t*)"\0\0\0\0\0\0");
BLEAddress mydev= BLEAddress((uint8_t*)"\0\0\0\0\0\0");
BLEClient*  pClient  = BLEDevice::createClient();
static BLERemoteCharacteristic* pRemoteCharacteristic;
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
 /**
   * Called for each advertising BLE server.
   */
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    byte i;
    std::string devname;
    // We have found a device, let us now see if it contains the service we are looking for.
    if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(BLEUUID(SERVICE_UUID))) {
   
   //   devID=advertisedDevice.getName().c_str()[0];
      if(advertisedDevice.getName().c_str()[0]==0)
       {
        ESP.restart();
       }
#ifdef DEBUG
    Serial.print("Server name  ");
    Serial.println(advertisedDevice.getName().c_str());
#endif
//      cRSSI=advertisedDevice.getRSSI();
      BLEDevice::getScan()->stop();
      condev=advertisedDevice.getAddress();
      doConnect = true;
    } // Found our server
  } // onResult
}; // MyAdvertisedDeviceCallbacks

class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) 
  {
    
  }

  void onDisconnect(BLEClient* pclient) {
#ifdef DEBUG
  Serial.print("on Disconnect");
#endif
  if(xconnected)
   {
    ESP.restart();
   }
  }
};

void savetrack()
{
  byte bl=LOW;
  BLEDevice::init("");
  BLEDevice::setMTU(512);
  
  BLEDevice::setPower(ESP_PWR_LVL_N12);
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setInterval(1349);
  pBLEScan->setWindow(449);
  pBLEScan->setActiveScan(true);
  Serial.print("Start BLE scan");
  pClient->setClientCallbacks(new MyClientCallback());
#ifdef DEBUG
Serial.print("Start BLE scan");
#endif
  pBLEScan->start(5, false);
  mydev=BLEDevice::getAddress();
#ifdef DEBUG
Serial.print("Pre Connect");
#endif
  if(doConnect)
   {
#ifdef DEBUG
Serial.print("Connect");
#endif

    //Если нашли наш сервер, то присоединяемся и сливаем трек.
   BLEClient*  pClient  = BLEDevice::createClient();
     BLERemoteService* pRemoteService;
    if(doConnect)
     {
     pClient->connect(condev);
     pRemoteService = pClient->getService(SERVICE_UUID);

     }
    
    if (pRemoteService == nullptr) 
      {
        pRemoteService = pClient->getService(SERVICE_UUID);
      pClient->disconnect();
       xconnected=false;
      }
     else
      {
    pRemoteCharacteristic = pRemoteService->getCharacteristic(CHARACTERISTIC_UUID_RX);
    if (pRemoteCharacteristic == nullptr) 
      {
      pClient->disconnect();
       xconnected= false;
      }
     else
      {
      xconnected = true;  
      }
     }
if(xconnected)  //соединились начинаем передачу
 {
    byte frow[505];
/*for(unsigned long kk=0;kk<1000;kk++)
 {
   pRemoteCharacteristic->writeValue((uint8_t *)(frow),505);
   timerWrite(timer, 0); //reset timer (feed watchdog)
   delay(100);
 }*/
  File    file = SPIFFS.open(TRACKFILE,FILE_READ); 

  byte* bm;
#ifdef DEBUG
Serial.println("Connected to xxxserver");
#endif

  if(file)
   {
  #ifdef DEBUG
Serial.print("File open");
#endif

   //Отправляем заголовок
#ifdef DEBUG
Serial.print("header ");
#endif

    bm=(byte*)mydev.getNative();
    byte h=0;
    for(byte i=0;i<6;i++)
     {
     frow[h]=bm[i];
     
#ifdef DEBUG
Serial.print(frow[h]);
#endif
     h++;
     }
#ifdef DEBUG
Serial.print(" ");
#endif

   union ulb tmpdat;
   tmpdat.ul=gps.date.value();
#ifdef DEBUG
Serial.print(tmpdat.ul);
#endif

   for(byte i=0;i<4;i++)
    {
      frow[h]=tmpdat.b[i];
      h++;
    }
    union ulb fsize;

   fsize.ul=file.size();

#ifdef DEBUG
Serial.print(" ");
Serial.print(fsize.ul);
#endif

   for(byte i=0;i<4;i++)
    {
      frow[h]=fsize.b[i];
      h++;
    }
   pRemoteCharacteristic->writeValue((uint8_t *)(frow),14);
#ifdef DEBUG
Serial.println("");
#endif

   unsigned long k=0;
   int n=0;
   while(file.available())
    {
    timerWrite(timer, 0); //reset timer (feed watchdog)
    digitalWrite(BLINK_LED,bl);
    bl=!bl;
    unsigned char cs=0;
    if((fsize.ul-k)>(unsigned long)504)
     {
    file.read(frow,504);
    for(int s=0;s<504;s++)
     {
      cs+=frow[s];
     }
     frow[504]=cs;
#ifdef DEBUG
Serial.print("CSum:");
Serial.println(cs);
#endif
    pRemoteCharacteristic->writeValue((uint8_t *)(frow),505);
    if(n>9)
     {
     delay(170);
     n=0;
     }
   else
     {
     delay(30);
     n++;
     }

    k=k+504;
     }
    else
     {

     file.read(frow,(int)(fsize.ul-k));
     tmpdat.ul=0;
    for(int s=0;s<504;s++)
     {
      if(s>fsize.ul-k)
       {
        frow[s]=0;
       }
      cs+=frow[s];
     }
      frow[504]=cs;

     delay(30);
     
     pRemoteCharacteristic->writeValue((uint8_t *)(frow),505);
     }
    }
   file.close();
   }
 }
     delay(170);  
    
   }  //if(doConnect)
// По окончанию работы с BLE перезагружаемся
xconnected=false;
pClient->disconnect();
  File    file = SPIFFS.open(CONFIGFILE,FILE_WRITE);
      if(file)
       {
       file.write(0); 
       file.close();
       }
  esp_restart(); 
    
}

esp_power_level_t btxp=ESP_PWR_LVL_N12;
unsigned long blTime;
byte bl_byte=LOW;


void setup()
{
 Serial1.begin(9600, SERIAL_8N1, 17, 16);   //GPS Serial
  Serial.begin(115200);
  pinMode(GPIO_NUM_33, INPUT);   
  pinMode(BLINK_LED, OUTPUT);     
 digitalWrite(BLINK_LED,HIGH);
if(!SPIFFS.begin(true)){
        Serial.println("Card Mount Failed");
        byte c=LOW;;
        while(1)
        {
           digitalWrite(BLINK_LED,c);
           c=(!c);
           delay(100);
        }
}
File    file = SPIFFS.open(CONFIGFILE,FILE_READ);
      if(file)
       {
       systemMode=file.read(); 
       if(systemMode==COMMANDMODE)
        {
         systemMode=GPSSEEK; 
        }
       file.close();
       }
      else
       {
       systemMode=GPSSEEK;
       }
 
 delay(500);          
#ifdef DEBUG
  Serial.println("start ");
#endif
   digitalWrite(BLINK_LED,LOW);
   blTime=millis(); 
   bl_byte=LOW;

//WDT
  timer = timerBegin(0, 80, true);                  //timer 0, div 80
  timerAttachInterrupt(timer, &resetModule, true);  //attach callback
  timerAlarmWrite(timer, wdtTimeout * 1000, false); //set time in us
  timerAlarmEnable(timer);                          //enable int

}
void sysModDepActions(unsigned long tm)
{
  unsigned long blDelta=1000;
  switch(systemMode)
 {
  case GPSSEEK:
  {
    blDelta=100;
    break;
  }
  case GPSREADY:
  {
    blDelta=500;
    break;
  }
  case GETGPSTREK:
  {
    if(bl_byte)
     {
      blDelta=2000;
     }
    else
     {
     blDelta=150;
     }
    break;
  }
  case COMMANDMODE:
  {
    blDelta=100;
    break;
  }
 }
    if(tm>blTime+blDelta)
     {
      digitalWrite(BLINK_LED,bl_byte);
      bl_byte=!bl_byte;
      blTime=tm;
     }
  
}
//-------------------------------------------------------
void loop()
{ 
timerWrite(timer, 0); //reset timer (feed watchdog)
unsigned long loopmillis=millis();
sysModDepActions(loopmillis);

if(systemMode<GPSREADY)
 {
    while (Serial1.available() > 0)
   {
    char g;
    g=Serial1.read();
#ifdef DEBUG
Serial.print(g);
#endif
    gps.encode(g);
   }
#ifdef DEBUG
Serial.println("");
#endif
  if (gps.location.isUpdated()) 
   {
    systemMode=GPSREADY;
    #ifdef DEBUG
Serial.println("Mode GPSREADY");
#endif

   }
 }
buttonstatus=button_press();
switch(buttonstatus)
 {
  case SINGLEPRESS :
   {
#ifdef DEBUG
 Serial.println("SINGLEPRESS");
#endif

    systemPreMode=systemMode;
    systemMode=COMMANDMODE;
    modestarttime=millis();
#ifdef DEBUG
 Serial.println("ENTER COMMAND MODE");
#endif
    
    break;
   }
    case DOUBLEPRESS :
   {
#ifdef DEBUG
 Serial.println("DOUBLEPRESS");
#endif
    if(systemMode!=GETGPSTREK)
     {
     systemMode=GETGPSTREK;
     SPIFFS.remove(TRACKFILE);
     }
    break;
   }
    case LONGPRESS :
   {
#ifdef DEBUG
 Serial.println("LONGPRESS");
#endif

 /*   if(systemMode==COMMANDMODE)
     {
     gosleep(systemPreMode); 
     }
    else*/
     {
     savetrack();
     }
    break;
   }


 }
if(modestarttime+DELTAMODE<millis() )
{
 if(systemMode==COMMANDMODE)
  {
    #ifdef DEBUG
 Serial.println("EXIT COMMAND MODE");
#endif
    
    systemMode=systemPreMode;
  }
}
#ifdef DEBUG
 byte r=0;
while(Serial.available())
 {
  r=Serial.read();
 }
switch(r)
{
 case 'l':
  {
   savetrack();
   break; 
  }
 case 'd':
  {
   if(systemMode!=GETGPSTREK)
    {
    systemMode=GETGPSTREK;
    SPIFFS.remove(TRACKFILE);
    }
    break;
  }
}
if(buttonstatus>0)
 {
 Serial.print("Button status: ");
 Serial.println(buttonstatus);
 }
#endif
if(systemMode==GETGPSTREK && lastGPSpoint+GPSDELTA<millis())  // Если в режиме получения трека 
 {
  GPS_getpoint();
 }

}
