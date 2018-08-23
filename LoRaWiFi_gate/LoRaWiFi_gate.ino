
#include <SPI.h>
#include <LoRa.h>
#include <RingBuf.h>
#include <SSD1306.h> // alias for `#include "SSD1306Wire.h"`
#define DEBUG
#define xSDEBUG
#include <Arduino.h>
#include "esp_system.h"

#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>
WiFiMulti wifiMulti;

#include <Base64.h>
#define SYNCPERIOD 15000
long tsync=1;
unsigned long lastsync=0;
unsigned long lastLoRaPacket=0;
//unsigned long GetLoRaAddress=1;
//unsigned long SetLoRaAddress=0;
unsigned long gasend=-1;
#define WIFI 0
#define LORA 1
#define SERIAL 2

#define WORK 2
#define GA 1
#define START 0
byte LoRaAdressMode=START;
//unsigned long lastLoRa;
//#define MINLORADELAY 300
#define LORAAdressLen 6
#define WIFITRY 2
byte LoRaAddr[LORAAdressLen];

 uint8_t WiFimac[6];
int LastLoRapacketLength=0;
byte LoRaLevel=250;


byte LoRaCurrentAdress=1;
byte sendmode=WIFI;
const char* ssid     = "TPw2K";
//const char* password = "+7(921)9636379";

//const char* ssid     = "eld_kzj_2_3";
//const char* ssid     = "RS71D";
const char* password = "as.df.gh12";

const char* host = "www.northernwind.spb.ru";
const int httpPort = 80;
int cmd=0;
#define EVENTDATALEN 20
struct Event 
{ 
  unsigned char resend[LORAAdressLen]; 
  unsigned char len; 
  unsigned char dat[EVENTDATALEN];
};
struct LoRapack 
{ 
  byte len; 
  byte dat[250];
  unsigned long gottime;
}; 
struct GAData 
{ 
  byte LoRaAdress[6];
  byte WiFimac;
}; 
#define BLACKLISTSIZE 4
byte blacklist[BLACKLISTSIZE][LORAAdressLen];
byte blacklistlevel[BLACKLISTSIZE];
#define MAXGAERROR 3
int gaerrors=0;



 
 // Declare as volatile, since modofied in ISR 
 volatile unsigned int indexx = 0; 
 
 
// Create a RinBuf object designed to hold a 200 Event structs 
RingBuf *buf = RingBuf_new(sizeof(struct Event), 200); 
RingBuf *bufLoRa = RingBuf_new(sizeof(struct LoRapack), 200); 
RingBuf *gaData = RingBuf_new(sizeof(struct GAData), 5); 
SSD1306  display(0x3c, 4, 15);
int setbandwidth=0;
int setsfactor=0;
byte setreboot=false;
byte GLoRaGot=false;
int GRssi=0;
float GSnr=0;

// GPIO5  -- SX1278's SCK
// GPIO19 -- SX1278's MISO
// GPIO27 -- SX1278's MOSI
// GPIO18 -- SX1278's CS
// GPIO14 -- SX1278's RESET
// GPIO26 -- SX1278's IRQ(Interrupt Request)

#define SS      18
#define RST     14
#define DI0     26
#define BAND    433E6
WiFiClient client;
//-----------------------------------------------------------------------
unsigned long getSyncTime()
{
  return(millis()+tsync);
}
//-----------------------------------------------------------------------
void cleardisplay(int y)
{
   display.setColor(BLACK);
   display.fillRect(0, y, 300, 10);
   display.display();
   display.setColor(WHITE);
}
//----------------------------------------------------------------------
#define WDTTIMEOUT 120000000 //in microseconds!!!
const int loopTimeCtl = 0;
hw_timer_t *timer = NULL;
void IRAM_ATTR resetModule(){
    ets_printf("reboot\n");
    esp_restart_noos();
}

//---------------------------------------------------------------------
void DrawRSSI()
 {
  int i;
// GLoRaGot=true;
// GRssi=LoRa.packetRssi();
#ifdef DEBUG
  timerWrite(timer, 0);
#endif
 char tmpstr[40];
  char Srssi[20];
  byte bw = (LoRa.readRegister(0x1d) >> 4);  //read bandwidth
  int sf=(LoRa.readRegister(0x1e) >> 4); //read Spreading Factor
int llp=(int)(millis()-lastLoRaPacket)/1000;
  cleardisplay(0);
 sprintf(tmpstr,"ID:%d sf:%d BW:%d llp:%d",(int)WiFimac[5],sf,bw,llp);
  display.drawString(0, 0, tmpstr); 
 cleardisplay(10);

  sprintf(Srssi,"RSSI: %d SNR:%5.2f ",GRssi,GSnr);
  display.drawString(0, 10, Srssi);
 if(GLoRaGot)
  {
    timerWrite(timer, 0);

  char modes[20];
 
  char BufL[20];
  char Loraaddrstr[40];
 

 
 
  cleardisplay(30);
  cleardisplay(40);
  cleardisplay(50);
  switch (sendmode)
   {
   case LORA: {
              sprintf(modes,"LoRa %d",LoRaAdressMode);
              break;
              }
   case WIFI: {
              sprintf(modes,"WiFi %d",LoRaAdressMode);
             break;
              }
   case SERIAL: {
              sprintf(modes,"SERIAL %d",LoRaAdressMode);
               break;
              }
      }
   sprintf(Loraaddrstr,"LORa: %d",LoRaAddr[0]);
  for(i=1;i<LORAAdressLen;i++)
   {
   sprintf(Loraaddrstr,"%s %d",Loraaddrstr,LoRaAddr[i]);
   }
  
    sprintf(Loraaddrstr,"%s ca %d",Loraaddrstr,LoRaCurrentAdress);
   display.drawString(0, 40, Loraaddrstr);
   sprintf(BufL,"Buf L: %d",buf->numElements(buf));
   display.drawString(0, 50, BufL);
   
 
  } 
  display.display();
  GLoRaGot=false;
 }

//--------------------------------------------------------------------------------------------
 void sendtoWiFi()  //Отправляет в WiFi все, что есть в буфере
{
      struct Event e;
//     byte *tmp;
     int bufNumEl;
    int j=0;
     int i=0;

 if((sendmode==WIFI)||(sendmode==SERIAL))
  {
  bufNumEl=buf->numElements(buf);
  if(bufNumEl>0)
    {
    byte tmp[bufNumEl*(sizeof(struct Event)+2)];
#ifdef DEBUG
    Serial.println(bufNumEl*(sizeof(struct Event)+2));
Serial.print("Beg data:");
#endif
 //    tmp=(char*)calloc(bufNumEl,sizeof(struct Event)+2);

  if((wifiMulti.run() == WL_CONNECTED)||(sendmode==SERIAL)) 
   {
    while(!buf->isEmpty(buf))
     {
     buf->pull(buf, &e);
    for(i=0;i<LORAAdressLen;i++)
     {
     tmp[j]=e.resend[i];
#ifdef DEBUG
Serial.print(tmp[j]);
Serial.print(":");
#endif
    
    j++;
    }
   tmp[j]=e.len;
   j++;
   for(i=0;i<e.len;i++)
    {
    tmp[j]=e.dat[i];
#ifdef DEBUG
Serial.print(tmp[j]);
Serial.print(":");
#else
Serial.write(tmp[j]);
#endif

    j++;
    }
    tmp[j]=0xFA;
#ifdef DEBUG
Serial.print(tmp[j]);
Serial.print(":");
#endif
    j++;
    tmp[j]=0xFB; 
#ifdef DEBUG
Serial.print(tmp[j]);
Serial.print(":");
#endif
    j++; 
   }
#ifdef DEBUG
Serial.println("END tmp");
#endif 
 if((j>0)&&(sendmode==WIFI))  //если собралась посылка
 {
 String encoded=base64::encode(tmp,j);
  HTTPClient http;
   http.begin("http://www.northernwind.spb.ru/alttiming/binary_post.ph5");
#ifdef DEBUG
   Serial.print("encoded");  
   Serial.println(encoded);
#endif
   int httpCode = http.POST(encoded);
   if(httpCode > 0) {
#ifdef DEBUG
            Serial.printf("[HTTP] GET... code: %d\n", httpCode);
#endif
            // file found at server
            if(httpCode == HTTP_CODE_OK) {
                String payload = http.getString();
#ifdef DEBUG
                Serial.println("Got replay from server:");
                Serial.println(payload);
#endif
            }
        } 
       else //если данные не ушли по HTTP скидываем их обратно в буффер
        {
#ifdef DEBUG
     Serial.println("HTTP Send Error");
#endif
       int k=0;
        while(k<j)
          {
          for(i=0;i<LORAAdressLen;i++)
            {
            e.resend[i]=tmp[k];
            k++;
            }
          e.len=tmp[k];
          k++;
          for(i=0;i<e.len;i++)
            {
            e.dat[i]=tmp[k];
            k++;
            }
          k+=2; //Пропускаем 0xFA 0xFB
          
          buf->add(buf, &e);   
          }
        }

      http.end();
      }
    }
   else  //если нет WiFi то ничего не готовилось к отправке пытаемся переподключится
    {
#ifdef DEBUG
     Serial.println("****************NotConnected*********************");
#endif
connectWiFi();
    }
 }//if BufNumElement>0

}

}
union ll
{
 unsigned long l;
 byte b[4];
};
//---------------------------------------------------------------------------------------------------
void sendLoRa()
{
  int i;
  int j=0;
  unsigned long rnd;
//  if((lastLoRa+MINLORADELAY)<millis())
delay(300);
  {
 
 if(!gaData->isEmpty(gaData))//      agadress=LoRaCurrentAdress; agmac=pack.dat[j];
 {
  struct GAData ga;
  gaData->pull(gaData, &ga);
#ifdef DEBUG
Serial.print("send ag...");
Serial.println(ga.WiFimac);
#endif

      LoRa.beginPacket();
      LoRa.print("ag");
      for(i=0;i<LORAAdressLen;i++)
       {
       LoRa.write(ga.LoRaAdress[i]);
       }
      LoRa.write(ga.WiFimac);
      LoRa.endPacket();
      delay(200);
      LoRa.receive(); 
  //    lastLoRa=millis();
      return;
   }
 
rnd=getSyncTime();

if(((long)(rnd-lastsync)>SYNCPERIOD)&&(LoRaAdressMode==WORK))
    { //отправляем синхронизацию

      ll ct;
      ct.l=getSyncTime();
#ifdef DEBUG
Serial.print("lastsync ");
Serial.print(lastsync);
Serial.print(" rnd ");
Serial.println(rnd);
Serial.print(" rnd-lastsync ");
Serial.println((long)(rnd-lastsync));
Serial.print("send ct");
Serial.print(ct.l);
Serial.print("addr ");
      for(i=0;i<LORAAdressLen;i++)
       {
       if(i==LoRaLevel)
        {
        Serial.print(LoRaCurrentAdress);  
        }
       else
        {
        Serial.print(LoRaAddr[i]);  
        }
       }
       Serial.println();
//Serial.println("");
#endif
      LoRa.beginPacket();
      LoRa.print("ct");
      
      for(i=0;i<4;i++)
       {
       LoRa.write(ct.b[i]);
       }  
      for(i=0;i<LORAAdressLen;i++)
       {
       if(i==LoRaLevel)
        {
        LoRa.write(LoRaCurrentAdress);  
        }
       else
        {
        LoRa.write(LoRaAddr[i]);  
        }
       } 
    LoRa.endPacket();

//    lastsync=ct.l;
   lastsync=lastsync+SYNCPERIOD;
    } //отправляем синхронизацию

if((sendmode==LORA)&&(LoRaAdressMode==GA)&&(millis()>gasend)) //Если режим LORA и режим адреса GA 
   {
#ifdef DEBUG
Serial.print("ga ");
    for(i=0;i<LORAAdressLen;i++)
      {
      Serial.print((int)LoRaAddr[i]);
      }
//Serial.println("");
#endif
//rnd=random((int)WiFimac[5]*50);
 //   delay(rnd);

      LoRa.beginPacket();
      LoRa.print("ga");
      for(i=0;i<LORAAdressLen;i++)
       {
       LoRa.write(LoRaAddr[i]);
       }
       LoRa.write(WiFimac[5]);
       LoRa.endPacket();
       delay(200);
       LoRa.receive(); 
 //       lastLoRa=millis();
       LoRaAdressMode=START;
       return;
    }
if((sendmode==LORA)&&(LoRaAdressMode==WORK)) //LORA режим, адрес получен и квитанция отправлена и получен ag с нашим последним байтом mac
 {
 struct Event e;
 int bufNumEl;
 j=0;
 i=0;
 if(!buf->isEmpty(buf))
     {
     buf->pull(buf, &e);
     j=2;
     LoRa.beginPacket();
     LoRa.print("dp");
      for(i=0;i<LORAAdressLen;i++)
        {
        LoRa.write(e.resend[i]);
        }
     LoRa.write(e.len);
     for(i=0;i<e.len;i++)
      {
      LoRa.write(e.dat[i]);
      }
     
      LoRa.endPacket();
      delay(200);
      LoRa.receive();
     return;
    }
 }
// byte setbandwidth=0;
//byte setsfactor=0;
if(setbandwidth>0)
 {
       LoRa.beginPacket();
      LoRa.print("bw");
      for(i=0;i<LORAAdressLen;i++)
       {
       LoRa.write(LoRaAddr[i]);
       }
       LoRa.write(setbandwidth);
       LoRa.write(WiFimac[5]);
       LoRa.endPacket();
    delay(200);
    LoRa.receive();
  LoRa.writeRegister(0x1d, (LoRa.readRegister(0x1d) & 0x0f) | (setbandwidth << 4));
  setbandwidth=0;
  return;
 }

if(setsfactor>0)
 {
       LoRa.beginPacket();
      LoRa.print("sf");
      for(i=0;i<LORAAdressLen;i++)
       {
       LoRa.write(LoRaAddr[i]);
       }
       LoRa.write(setsfactor);
       LoRa.write(WiFimac[5]);
       LoRa.endPacket();
    delay(200);
    LoRa.receive();
    if (setsfactor == 6) {
    LoRa.writeRegister(0x31, 0xc5);
    LoRa.writeRegister(0x37, 0x0c);
  } else {
    LoRa.writeRegister(0x31, 0xc3);
    LoRa.writeRegister(0x37, 0x0a);
  }

  LoRa.writeRegister(0x1e, (LoRa.readRegister(0x1e) & 0x0f) | ((setsfactor << 4) & 0xf0));
  setsfactor=0;
  return;
 }
 
 if(setreboot)
  {
      LoRa.beginPacket();
      LoRa.print("rd");
      for(i=0;i<LORAAdressLen;i++)
       {
       LoRa.write(LoRaAddr[i]);
       }
       LoRa.write(WiFimac[5]);
       LoRa.endPacket();
    delay(200);
    ESP.restart();
  }
//  lastLoRa=millis();
  delay(200);
  LoRa.receive();
  } 
}
//---------------------------------------------------------------------------------------------------

void LoRaBufPars()
 {
  struct LoRapack pack;
  struct Event e;
  int i;
  int notinblacklist=0;
  int j=0;
  byte LoRatmpAddr[LORAAdressLen];

  ll ct;
  long tsynctmp;
  int LoRatmpLevel=250;
  int ismyadress=0;
  int ismyGAadress=0;
  int tmpLoRaCurrentAdress=255;
  tmpLoRaCurrentAdress=LoRaCurrentAdress;
  while(!bufLoRa->isEmpty(bufLoRa))
     {
 //    lastLoRa=millis();
     bufLoRa->pull(bufLoRa, &pack);
#ifdef DEBUG
Serial.println("LoRa Packet");
    Serial.print((char)pack.dat[0]);
    Serial.print((char)pack.dat[1]);
    Serial.print(" ");
    for(i=2;i<pack.len;i++)
     {
     Serial.print(pack.dat[i]);
     Serial.print(":");
     }
    Serial.print(" pack length:");
    Serial.println(pack.len);  
#endif 
//------
lastLoRaPacket=pack.gottime;

if((pack.dat[0]=='b')&&(pack.dat[1]=='w'))
 {
 setbandwidth=pack.dat[2+LORAAdressLen];
 }
 if((pack.dat[0]=='s')&&(pack.dat[1]=='f'))
 {
 setsfactor=pack.dat[2+LORAAdressLen];
 }


if((pack.dat[0]=='r')&&(pack.dat[1]=='d'))
 {
 setreboot=true;
 }
if((pack.dat[0]=='a')&&(pack.dat[1]=='g')&&(pack.dat[pack.len-1]==WiFimac[5])&&(sendmode==LORA))  //если пришло подтвержение получения адреса и оно пришло нам (pack.dat[pack.len]==WiFimac[5])
    {  //записываем его как основной адрес
    j=LORAAdressLen+1;
    for(i=LORAAdressLen-1;i>=0;i--)
     {
     LoRaAddr[i]=(byte)pack.dat[j];
     if(LoRaAddr[i]==0)
      {
      LoRaLevel=i;  
      }
     j--;
     }
    LoRaAdressMode=WORK;
    lastsync=getSyncTime()+random((int)WiFimac[5]*50);
    GLoRaGot=true;
    }

#ifdef nDEBUG
     Serial.print("sendmode ");
     Serial.println(sendmode);
     Serial.print("LoRaAdressMode ");
     Serial.println(LoRaAdressMode);
     Serial.print("gasend+SYNCPERIOD ");
     Serial.println(gasend+SYNCPERIOD);


#endif
if((sendmode==LORA)&&(LoRaAdressMode==START)&&(millis()>(gasend+1.5*SYNCPERIOD))) //Если ag не пришло за время синхронизации
   {
#ifdef DEBUG
     Serial.println("Dont get ag ");
#endif
   for(j=0;j<LORAAdressLen;j++)  //вносим текущий адрес в blacklist
    {
    LoRaAddr[j]=254;
    }
    LoRaLevel=250;
    LoRaAdressMode=START; //адрес в режим старт
    gaerrors++;
    GLoRaGot=true;
   }
if((pack.dat[0]=='c')&&(pack.dat[1]=='t'))  //Если пришла синхронизация времени и свободный адрес
 {
  LoRatmpLevel=250;
  j=2;
  cmd=10;
 
  if(sendmode==LORA) //Все это работает ТОЛЬКО если это LORA repeater. WiFi gate - сам раздает время
    {
    for(i=0;i<4;i++)
     {
     ct.b[i]=(byte)pack.dat[j];
     j++;
     }
    tsynctmp=ct.l-pack.gottime;
#ifdef DEBUG
     Serial.print("tsync ");
     Serial.print(tsynctmp);
     Serial.print("gottime ");
     Serial.println(pack.gottime);

#endif
    for(i=0;i<LORAAdressLen;i++)
     {
     LoRatmpAddr[i]=(byte)pack.dat[j];
     if(LoRatmpAddr[i]==LoRaAddr[i])  // параллельно проверяем не наш ли это адрес 
      {
      ismyadress++;  
      }
      j++;
#ifdef DEBUG
     Serial.print(LoRatmpAddr[i]);
     Serial.print(":");
#endif
     if((LoRatmpAddr[i]==0)&&(LoRatmpLevel==250))
      {
      LoRatmpLevel=i;  //вычисляем уровень полученного адреса
      }
     }
     
     if(LoRatmpLevel<=LoRaLevel) // Если уровень выше или равен делаем синхронизация времени !!! ??? + 
      {
      tsync=tsynctmp;
      
 #ifdef nDEBUG
 Serial.print("Apply tsync ");
 Serial.print(tsync);
 Serial.print(" time");
 Serial.println(getSyncTime());
  Serial.print("Current Level");
 Serial.println(LoRaLevel);
  Serial.print("New Level");
 Serial.println(LoRatmpLevel);
 #endif
      }
if(LoRatmpLevel<=LoRaLevel)  //если уровень меньше или равен текущего проверяем адрес на blacklist
     {
      notinblacklist=0;
      for(i=0;i<BLACKLISTSIZE;i++)
       {
       if(blacklistlevel[i]!=LoRatmpLevel)  //если уровни не равны точно не в blacklist
        {
        notinblacklist++;  
        }
       else
        {
         int isbl=0;
         byte tmpaddrpart=LoRatmpAddr[blacklistlevel[i]-1];
         LoRatmpAddr[blacklistlevel[i]-1]=0;
         for(j=0;j<blacklistlevel[i];j++)
          {

  //      byte blacklist[BLACKLISTSIZE][LORAAdressLen];  
          if(blacklist[i][j]==LoRatmpAddr[j])
           {
           isbl++;
           }
         }
        LoRatmpAddr[blacklistlevel[i]-1]=tmpaddrpart;
        if(isbl<blacklistlevel[i])
         {
         notinblacklist++;
         }
        }
       }
     }
#ifdef DEBUG
if(notinblacklist!=BLACKLISTSIZE)
 {
 Serial.println("ct adress in blacklist");
 }
#endif 
if(ismyadress==LORAAdressLen) //если это наш адрес
 {
 if(gaerrors<MAXGAERROR)  //если количество попыток отправки меньше разрешенного
  {
  gaerrors++;             //увеличиваем уровень ошибки
  LoRaAdressMode=GA;     //переводим адрес в режим подтверждения
  gasend=millis();  //если это наш адрес мгновенно высылаем GA 
  }
 else
  { // иначе вносим адрес в blacklist
  gaerrors=0;
  i=0;
  while((blacklist[i][0]<250)&&(i<BLACKLISTSIZE))
   {
   i++;
   }
   for(j=0;j<LORAAdressLen;j++)  //вносим текущий адрес в blacklist
    {
   blacklist[i][j]=LoRatmpAddr[j];
   LoRaAddr[j]=254;
    }
  blacklist[i][LoRaLevel-1]=0;
  blacklistlevel[i]=LoRaLevel;
  LoRaLevel=250;
  LoRaAdressMode=START; //адрес в режим старт
  GLoRaGot=true;
   }
  }
 else
  {
   if((LoRatmpLevel<LoRaLevel)&&(notinblacklist==BLACKLISTSIZE)) // если уровень ниже нашего и адрес не в blacklist (нам этот адрес подходит)
    {
    LoRaAdressMode=GA;  //адрес в режим ожидания подтверждения
    LoRaLevel=LoRatmpLevel;
    for(i=0;i<LORAAdressLen;i++)  //присваиваем текущий 
      {
      LoRaAddr[i]=LoRatmpAddr[i];
      }
      gasend=millis()+random((int)WiFimac[5]*50); // ставим отправку GA на случайную задержку
      GLoRaGot=true;
    }
   }
  }
 }
if((pack.dat[0]=='g')&& (pack.dat[1]=='a')&&(LoRaAdressMode==WORK))  //если пришел запрос на адрес
 {
    cmd=20;
   j=2;
  int LoRaAddresCheck=0;
  int xi=0;
        #ifdef DEBUG
       Serial.print("parse GA ");
       #endif
      for(xi=0;xi<LORAAdressLen;xi++) //проверяем нам ли это запрос
       {
       #ifdef DEBUG
       Serial.print(pack.dat[j]);
       #endif
       if(xi==LoRaLevel)
        {
        if(tmpLoRaCurrentAdress==pack.dat[j])
         {
         LoRaAddresCheck++;
         }
        }
       else
        {
        if(LoRaAddr[xi]==pack.dat[j])
         {
         LoRaAddresCheck++;
         }
        }
       j++;
       }
    
#ifdef SDEBUG
if(LoRaCurrentAdress!=2)
 {
#endif
     if(xi==LoRaAddresCheck) //если запрос на наш текущий адрес
      {
#ifdef DEBUG
       Serial.print("Ident as OUR ");
       Serial.println(tmpLoRaCurrentAdress);
#endif

      struct GAData ga;
      if(!gaData->isFull(gaData))
      {
      for(i=0;i<LORAAdressLen;i++)
       {
        if(i==LoRaLevel)
         {
         ga.LoRaAdress[i]=LoRaCurrentAdress;
         LoRaCurrentAdress++;
         }
        else
         {
         ga.LoRaAdress[i]=pack.dat[i+2]; 
         }
        ga.WiFimac=pack.dat[pack.len-1];
      
       }
       gaData->add(gaData, &ga); 
      }
     }

    
#ifdef SDEBUG
 }
#endif

 }

if((pack.dat[0]=='d')&&(pack.dat[1]=='p'))  //если пришел пакет с данными
 {
   cmd=30;
   j=2;
// for(int xj=0;xj<(pack.len-2)/sizeof(struct Event);xj++)
//  {
  for(i=0;i<LORAAdressLen;i++)
   {
   e.resend[i]=pack.dat[j];
     if((e.resend[i]==0)&&(LoRatmpLevel==250))
      {
      LoRatmpLevel=i;  //выяисляем уровень полученного адреса
      }
   j++;
   }
 if((LoRatmpLevel>LoRaLevel)||(LoRaLevel==250)) // Если уровень выше или равен делаем синхронизация времени !!! ???
     {
     e.len=pack.dat[j];
     j++;
     for(i=0;i<e.len;i++)
      {
      e.dat[i]=pack.dat[j];
      j++;
      }
     if(buf->isFull(buf))
      {
       struct Event ex;  
       buf->pull(buf, &ex);
      }
     buf->add(buf, &e);   
     }
//  }    
 }
//--------
     }

 }
//---------------------------------------------------------------------------------------------------
#define CM_PSZ 10
#define GA_PSZ 6
void onReceive(int packetSize)  //Срабатывает при получении данных с LoRa
{
struct LoRapack pack;
int i=0;
cmd=5;

if(packetSize>0 && packetSize<sizeof(struct LoRapack))
 {
 for(i=0;i<packetSize;i++)
  {
   pack.dat[i]=LoRa.read();
   }
  pack.len=packetSize;
  pack.gottime=millis();
  bufLoRa->add(bufLoRa, &pack);
 }
 GRssi=LoRa.packetRssi();
 GSnr=LoRa.packetSnr();
 GLoRaGot=true;
}
//------------------------------------------------------------------------------------


void connectWiFi()
{
#ifdef DEBUG
 Serial.print("WiFI start");
#endif
   // start WiFi
   wifiMulti.addAP(ssid, password);  
   int k;   
    for(k=0;(k<WIFITRY)&&(wifiMulti.run() != WL_CONNECTED);k++) {
#ifdef DEBUG
 Serial.print("WiFI try");
 Serial.println(k);
#endif
        delay(500);
     }
   WiFi.macAddress(WiFimac);

 #ifdef DEBUG
 Serial.print("ssid");
 Serial.println(ssid);
 Serial.print("password");
 Serial.println(password);
 Serial.print("mac:");
//  Serial.println(xx);
 //Serial.println(WiFi.macAddress());
 for(int l=0;l<6;l++)
  {
  Serial.printf("%02X:",WiFimac[l]);

  }
     Serial.println("");
 #endif 

char tstr[30];
char ipstr[20];
cleardisplay(20);
cleardisplay(30);

if(k<WIFITRY)
 {
    WiFi.localIP().toString().toCharArray(ipstr,16);
    display.drawString(0, 10, "WiFi connected");
    sprintf(tstr,"IP: %s",ipstr);
    display.drawString(0, 20, tstr);
    display.display();  
    sendmode=WIFI;
    LoRaLevel=0;
    LoRaAdressMode=WORK;
    for(int k=0;k<LORAAdressLen;k++)
     {
     LoRaAddr[k]=0;
     }

 }
else
 {
 display.drawString(0, 10, "WiFi not found"); 

 display.drawString(0, 30, "LoRa repeater"); 
    display.display();
 sendmode=LORA;
 }

 
}
//------------------------------------------------------------------------------------
void readFromSerial()  //Читает данные и UART помещает в буфер 
{
ll ct; 
struct Event e;
unsigned char i=0;
 
while(Serial2.available()&&(i<EVENTDATALEN))
  {
   
   e.dat[i]=(unsigned char)Serial2.read();
#ifdef DEBUG
Serial.print("e.dat[]");
Serial.println(e.dat[i]);
#endif
   i++;
  }
 if(i>0)
  {
  e.len=i;
#ifdef DEBUG
Serial.print("e.len:");
Serial.println(e.len);
#endif
for(int k=0;k<LORAAdressLen;k++)
  {
  e.resend[k]=LoRaAddr[k];
  }
   if(buf->isFull(buf))
    {
    struct Event ex;  
    buf->pull(buf, &ex);
    }
  buf->add(buf, &e);   
  }
// читаем из USB
while(Serial.available()&&(i<EVENTDATALEN))
  {
   
   e.dat[i]=(unsigned char)Serial.read();
#ifdef DEBUG
Serial.print("e.dat[]");
Serial.println(e.dat[i]);
#endif
   i++;
  }
 if(i>0)
  {
  e.len=i;
 if((e.dat[0]=='x')&&(e.dat[1]=='x'))
  {
   switch (e.dat[2])
    {
    case 's':
     {
     
     sendmode=SERIAL;
     LoRaLevel=0;
     LoRaAdressMode=WORK;
     
     for(int k=0;k<LORAAdressLen;k++)
      {
      LoRaAddr[k]=0;
      }
     LoRaCurrentAdress=1;
     lastsync=getSyncTime();
     GLoRaGot=true;
 //    tsync=0;
     break;   
     }
    case 'r':
     {
     setreboot=true;
     break;   
     }
    case 'b':
    {
      setbandwidth=e.dat[3]-48;
      break;
    }
      case 'f':
    {
      setsfactor=e.dat[3]-48;
      break;
    }  
  //setreboot
    }
  }
 else
  {
#ifdef DEBUG
Serial.print("e.len:");
Serial.println(e.len);
#endif
for(int k=0;k<LORAAdressLen;k++)
  {
  e.resend[k]=LoRaAddr[k];
  }
   if(buf->isFull(buf))
    {
    struct Event ex;  
    buf->pull(buf, &ex);
    }
  buf->add(buf, &e);   
  }
  }


}

// HardwareSerial Serial2(2);

void setup() {
  pinMode(DI0,INPUT);
  pinMode(16,OUTPUT);
  digitalWrite(16, LOW);    // set GPIO16 low to reset OLED
  delay(50); 
   digitalWrite(16, HIGH); // while OLED is running, must set GPIO16 in high
    display.init();
    display.flipScreenVertically();
    display.setFont(ArialMT_Plain_10);
    display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.clear();
  display.drawString(0, 0, "Strat LoRa WiFi gateway");
  display.display(); 
  #ifdef DEBUG
  Serial.begin(115200,SERIAL_8N1,3,1);
  #else
    Serial.begin(38400,SERIAL_8N1,3,1);
  #endif
 

#define SERIAL2_RXPIN 23 

#define SERIAL2_TXPIN 22 

    Serial2.begin(38400, SERIAL_8N1, SERIAL2_RXPIN, SERIAL2_TXPIN); 

  
  while (!Serial); //if just the the basic function, must connect to a computer

  SPI.begin(5,19,27,18);
  LoRa.setPins(SS,RST,DI0);

  while (!LoRa.begin(BAND)) {
  display.clear();
  display.drawString(0, 0, "LoRa faile!!!");
  display.display(); 
  }
LoRa.setTxPower(20,1);
 display.clear();

    for(int k=0;k<LORAAdressLen;k++)
     {
     LoRaAddr[k]=0xFF;
     }
connectWiFi();
lastsync=getSyncTime()+random((int)WiFimac[5]*50);

 char tmpstr[16];
 sprintf(tmpstr,"LoRa ok ID:%d",WiFimac[5]);
 display.drawString(0, 0, tmpstr);
 
  display.display(); 
for(int i=0;i<BLACKLISTSIZE;i++)
 {
  blacklistlevel[i]=LORAAdressLen-1;
  for(int j=0;j<LORAAdressLen;j++)
   {
    blacklist[i][j]=252;
   }
 }

  // register the receive callback
  LoRa.onReceive(onReceive);

  // put the radio into receive mode
  LoRa.receive();
    delay(5000);
 #ifdef DEBUG
Serial.print("rand ");
Serial.println(random((int)WiFimac[5]*50));


#endif
lastLoRaPacket=millis();
    pinMode(loopTimeCtl, INPUT_PULLUP);
    delay(1000);
    Serial.println("running setup");
timer = timerBegin(0, 80, true); //timer 0, div 80
    timerAttachInterrupt(timer, &resetModule, true);
 
  
      timerAlarmWrite(timer, WDTTIMEOUT, false); //set time in us
      if(sendmode!=WIFI)
      {
      timerAlarmEnable(timer); //enable interrupt
      }

}
#ifdef DEBUG
unsigned long snddeb=0;
#endif
//---------------------------------------------------------
void loop() {

DrawRSSI();
LoRaBufPars();
readFromSerial(); 

sendtoWiFi();

sendLoRa();
LoRa.receive(); 
#ifdef DEBUG
if(cmd!=0)
 {
   Serial.print("got ");
 Serial.println(cmd);
 cmd=0;
 
 }
if(millis()-snddeb>30000)
 {
  snddeb=millis();
   Serial.print("time ");
 Serial.println(snddeb);  
 }

#endif
}

