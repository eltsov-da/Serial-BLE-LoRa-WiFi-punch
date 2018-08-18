
#include <SPI.h>
#include <LoRa.h>
#include <RingBuf.h>
#include <SSD1306.h> // alias for `#include "SSD1306Wire.h"`
#define DEBUG
#include <Arduino.h>

#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>
WiFiMulti wifiMulti;

#include <Base64.h>
#define SYNCPERIOD 15000
long tsync=1;
unsigned long lastsync=0;
unsigned long GetLoRaAddress=1;
#define WIFI 0
#define LORA 1
#define LORAAdressLen 6
#define WIFITRY 2
byte LoRaAddr[LORAAdressLen];

int LastLoRapacketLength=0;
byte LoRaLevel=250;
byte LoRaCurrentAdress=1;
byte sendmode=WIFI;
//const char* ssid     = "TPw2K";
//const char* password = "+7(921)9636379";
const char* ssid     = "eld_kzj_3_3";
//const char* password = "";
//const char* ssid     = "RS71D";
//const char* ssid     = "444";

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
 
 // Declare as volatile, since modofied in ISR 
 volatile unsigned int indexx = 0; 
 
 
// Create a RinBuf object designed to hold a 200 Event structs 
RingBuf *buf = RingBuf_new(sizeof(struct Event), 200); 
RingBuf *bufLoRa = RingBuf_new(sizeof(struct LoRapack), 200); 
SSD1306  display(0x3c, 4, 15);

byte GLoRaGot=false;
int GRssi=0;


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
void DrawRSSI()
 {
  int i;
// GLoRaGot=true;
// GRssi=LoRa.packetRssi();
 if(GLoRaGot)
  {
  cleardisplay(10);
  char Srssi[20];
  char BufL[20];
  char Loraaddrstr[40];
  sprintf(Srssi,"RSSI: %d",GRssi);
  display.drawString(0, 10, Srssi);
  cleardisplay(40);
  cleardisplay(50);
  sprintf(Loraaddrstr,"LORa: %d",LoRaAddr[0]);
  for(i=1;i<LORAAdressLen;i++)
   {
   sprintf(Loraaddrstr,"%s %d",Loraaddrstr,LoRaAddr[i]);
   }
   display.drawString(0, 40, Loraaddrstr);
   sprintf(BufL,"Buf L: %d",buf->numElements(buf));
   display.drawString(0, 50, BufL);
   display.display();
  } 
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

 if(sendmode==WIFI)
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

  if((wifiMulti.run() == WL_CONNECTED)) 
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
 if(j>0)  //если собралась посылка
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
  
  if(LoRaLevel<250)
   {
   rnd=getSyncTime();
   if(rnd-lastsync>SYNCPERIOD)  
    { //отправляем синхронизацию
      ll ct;
      ct.l=getSyncTime();
 #ifdef DEBUG
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

    lastsync=ct.l;
    }
   }
if((sendmode==LORA)&&((millis()>GetLoRaAddress)&&(GetLoRaAddress>100)))
   {

#ifdef DEBUG
Serial.print("ga");
    for(i=0;i<LORAAdressLen;i++)
      {
      Serial.print((int)LoRaAddr[i]);
      }
//Serial.println("");
#endif
rnd=random(1000);
    delay(rnd);

      LoRa.beginPacket();
      LoRa.print("ga");
      for(i=0;i<LORAAdressLen;i++)
       {
       LoRa.write(LoRaAddr[i]);
       }
       LoRa.endPacket();
       GetLoRaAddress=2;
    }
if((sendmode==LORA)&&(GetLoRaAddress==2)) //LORA режим, адрес получен и квитанция отправлена
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
    
    }
 }

delay(200);
LoRa.receive(); 
}
//---------------------------------------------------------------------------------------------------

void LoRaBufPars()
 {
  struct LoRapack pack;
  struct Event e;
  int i;
  int j=0;
  byte LoRatmpAddr[LORAAdressLen];
  ll ct;
  long tsynctmp;
  int LoRatmpLevel=250;
  int ismyadress=0;
  while(!bufLoRa->isEmpty(bufLoRa))
     {
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
if((pack.dat[0]=='c')&&(pack.dat[1]=='t'))  //Если пришла синхронизация времени и свободный адрес от вышестоящего узла
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
     if(LoRatmpLevel<=LoRaLevel) // Если уровень выше или равен делаем синхронизация времени !!! ???
      {
      tsync=tsynctmp;
 #ifdef DEBUG
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
    if((LoRatmpLevel<LoRaLevel)||(ismyadress==LORAAdressLen)||(GetLoRaAddress>100)) //если уровень выше текущего уровня данной станции или повторно пришел наш адрес или пришел новый адрес, до того, как отправили подтверждение
     {
     for(i=0;i<LORAAdressLen;i++)
      {
      LoRaAddr[i]=LoRatmpAddr[i];
      }
     LoRaLevel=LoRatmpLevel;
     if(ismyadress==LORAAdressLen)
      {
      GetLoRaAddress=millis()+random(SYNCPERIOD/3); //ставим флаг, что получили новый адрес и нужно отправить подтверждение через случайное время !!! используем локальное время
      }
     else
      {
      GetLoRaAddress=millis(); //если пришел наш адрес отправляем ответ мгновенно
      } 
     lastsync=getSyncTime()+SYNCPERIOD+2000+random(SYNCPERIOD/2);  //останавливаем отправку ct на два периода + 2сек + случайное число 
     }
    }

 }
if((pack.dat[0]=='g')&& (pack.dat[1]=='a'))  //если пришла квитанция о полученнии адреса
 {
    cmd=20;
   j=2;
  int LoRaAddresCheck=0;
  int xi=0;
 
      for(xi=0;i<LORAAdressLen;xi++) //проверяем нам ли это квитанция
       {
       if(xi==LoRaLevel)
        {
        if(LoRaCurrentAdress==pack.dat[j])
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
     if(xi==LoRaAddresCheck) //если квитанция о нашем текущем адресе переходим к выдаче следущего
      {
      LoRaCurrentAdress++; 
      lastsync=0;
      } 
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
 while(LoRa.available())
  {
   pack.dat[i]=LoRa.read();
   i++;
  }
  pack.len=packetSize;
  pack.gottime=millis();
  bufLoRa->add(bufLoRa, &pack);
 }

 GRssi=LoRa.packetRssi();
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
 #ifdef DEBUG
 Serial.print("ssid");
 Serial.println(ssid);
 Serial.print("password");
 Serial.println(password);
 Serial.print("mac:");
 Serial.println(WiFi.macAddress());
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
display.clear();
 display.drawString(0, 0, "LoRa ok!!!");
 
  display.display(); 
 

    for(int k=0;k<LORAAdressLen;k++)
     {
     LoRaAddr[k]=0xFF;
     }
lastsync=getSyncTime();
connectWiFi();


  // register the receive callback
  LoRa.onReceive(onReceive);

  // put the radio into receive mode
  LoRa.receive();
    delay(5000);
 #ifdef DEBUG
Serial.print("rand");
Serial.println(rand());


#endif
}
#ifdef DEBUG
unsigned long snddeb=0;
#endif
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

