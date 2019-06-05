//#include <SPI.h>
//#include <RH_RF95.h>
//#include <RHRouter.h> 
//#include <RHMesh.h> 
#include <WiFi.h>
#include <WiFiUdp.h>
#include <RingBuf.h>
#define LISTENTIME 1000
#define GPSPACKETLENGTH 22
#define RH_MESH_MAX_MESSAGE_LEN 200
//Include required lib so Arduino can communicate with Yun Shield
//#include <FileIO.h>
//#include <Console.h>
#define GATELORAMESHID 1
#define SERIAL1SPEED 38400
const char *ssid = "eld_kzj_3_3";  // You will connect your phone to this Access Point
const char *pw = "as.df.gh12"; // and this is the password
const char * udpAddress = "192.168.11.38"; 
const int udpPort = 1256; 
WiFiUDP udp;
#define XDEBUG
//#define DEBUG

// Singleton instance of the radio driver
//RH_RF95 rf95(18,26);
//RHMesh *mesh;
//Для разных плат нужны разные reset_lora
int reset_lora = 14;
 uint8_t LoRaERROR=0;
 
 struct Event{
 char data_buf[RH_MESH_MAX_MESSAGE_LEN];
 uint8_t len;
 };
RingBuf *buf = RingBuf_new(sizeof(struct Event), 100); 

byte WiFimac[6]; 
byte meshNodeId; 
 

 
 union ufb {
      float f;;
      unsigned long ul;
      long l;
      byte b[4];
    } uimei,ulat,ulon,uspd,udt,utm;


const __FlashStringHelper* getErrorString(uint8_t error) { 
   switch(error) { 
     case 1: return F("invalid length"); 
     break; 
     case 2: return F("no route"); 
     break; 
     case 3: return F("timeout"); 
     break; 
     case 4: return F("no reply"); 
     break; 
     case 5: return F("unable to deliver"); 
     break; 
   } 
   return F("unknown"); 
 }
//---------------------------------------------------------------------------------
boolean connected=false;
void connectToWiFi(const char * ssid, const char * pwd){
  Serial.println("Connecting to WiFi network: " + String(ssid));

  // delete old config
  WiFi.disconnect(true);
  //register event handler
  WiFi.onEvent(WiFiEvent);
  
  //Initiate connection
  WiFi.begin(ssid, pwd);

  Serial.println("Waiting for WIFI connection...");
}

//wifi event handler
void WiFiEvent(WiFiEvent_t event){
    switch(event) {
      case SYSTEM_EVENT_STA_GOT_IP:
          //When connected set 
          Serial.print("WiFi connected! IP address: ");
          Serial.println(WiFi.localIP());  
          //initializes the UDP state
          //This initializes the transfer buffer
          udp.begin(WiFi.localIP(),udpPort);
          connected = true;
          break;
      case SYSTEM_EVENT_STA_DISCONNECTED:
          Serial.println("WiFi lost connection");
          connected = false;
          break;
    }
}
  
//----------------------------------------------------------------------------------
void readFromSerial()
{
  struct Event SERIAL_buf;
  uint8_t i=1;
  while(Serial.available()&&(i<RH_MESH_MAX_MESSAGE_LEN-2))
  {
  SERIAL_buf.data_buf[i]=(unsigned char)Serial.read();
#ifdef DEBUG
Serial.print(SERIAL_buf.data_buf[i],HEX);
Serial.print(":");

#endif  

  i++;
  }

  if(i>1)
   {
   SERIAL_buf.data_buf[0]='S';
   SERIAL_buf.data_buf[i]='E';
   SERIAL_buf.data_buf[i+1]='D';
   SERIAL_buf.len=i+2;
   if(buf->isFull(buf))
    {
    struct Event ex; 
    buf->pull(buf, &ex);
    }
    buf->add(buf, &SERIAL_buf);
   }
i=1;

  while(Serial1.available()&&(i<RH_MESH_MAX_MESSAGE_LEN-2))
  {
  SERIAL_buf.data_buf[i]=(unsigned char)Serial1.read();
#ifdef DEBUG
Serial.print(SERIAL_buf.data_buf[i],HEX);
Serial.print(":");
#endif  
#ifdef XDEBUG
Serial.write((byte)SERIAL_buf.data_buf[i]);
#endif  

  i++;
  }
  if(i>1)
   {
#ifdef DEBUG
Serial.println("");
#endif  

    
   SERIAL_buf.data_buf[0]='S';
   SERIAL_buf.data_buf[i]='E';
   SERIAL_buf.data_buf[i+1]='D';
   SERIAL_buf.len=i+2;
   if(buf->isFull(buf))
    {
    struct Event ex; 
    buf->pull(buf, &ex);
    }
    buf->add(buf, &SERIAL_buf);
   }

}

//----------------------------------------------------------------------------------
void transmitToWiFi()
{
  uint8_t i=0;
  uint8_t j=0;
  uint8_t message_size=0;
  byte send_buf[RH_MESH_MAX_MESSAGE_LEN];
  struct Event ex; 
//То, что buf не пустой проверяем в loop
  if(!buf->isEmpty(buf))
   {
   buf->pull(buf, &ex);
   message_size=ex.len;
    i=0;
    for(j=1;j<ex.len-1;j++)
     {
     if(ex.data_buf[j]=='E'&&ex.data_buf[j+1]=='D')
      {
       // Если следующие два байта ED отправляем в порт
       if(!connected){
        connectToWiFi(ssid, pw);       
        }
    //Send a packet
    udp.beginPacket(udpAddress,udpPort);
    udp.write(send_buf,i);
    udp.endPacket();
       if(j+3<ex.len)
        {
        j+=2; 
        i=0;
        }
      }
     else
      {
     send_buf[i]=ex.data_buf[j];
#ifdef DEBUG
Serial.print(send_buf[i]);
#endif  
     i++;
      }
     }
    
   }
  
  
/*  
  while(message_size<RH_MESH_MAX_MESSAGE_LEN)
   {
    for(j=0;j<ex.len;j++)
     {
     send_buf[i]=ex.data_buf[j];
#ifdef DEBUG
Serial.print(send_buf[i]);
#endif  

     i++;
     }
     if(!buf->isEmpty(buf))
      {
      buf->pull(buf, &ex);
      message_size=message_size+ex.len;  
      }
     else
      {
      break;  
      }
   }
  if(i<message_size)
   {
    message_size=message_size-ex.len;    
    buf->add(buf,&ex);  // Если последнее сообщение не влезло возвращаем его обратно в буфер
   }
   uint8_t error = mesh->sendtoWait((uint8_t *)send_buf, message_size, GATELORAMESHID); // все отправляем на 1 узел
    if (error != RH_ROUTER_ERROR_NONE) { 
       
       Serial.println(); 
       Serial.print(F(" ! ")); 
       Serial.println(getErrorString(error)); 
       for(i=0;i<message_size;i++)
        {
         ex.data_buf[i]=send_buf[i];
        }
        ex.len=message_size;
        buf->add(buf,&ex);
     } else { 
       Serial.println(F(" OK")); 
     }
     */
 }
//----------------------------------------------------------------------------------


unsigned long nextTransmit;

void setup() 
{
/*   pinMode(reset_lora, OUTPUT);     
   digitalWrite(reset_lora, LOW);   
   delay(1000);
   digitalWrite(reset_lora, HIGH); 
*/
#ifdef XDEBUG
    Serial.begin(38400);
#endif
#ifndef XDEBUG
    Serial.begin(115200);
#endif
    Serial.println("start server x");
    Serial.println("");
    Serial1.begin(SERIAL1SPEED, SERIAL_8N1, 23, 27);   

   WiFi.begin();
   WiFi.macAddress(WiFimac);
   for(int i=0;i<6;i++)
    {
      Serial.print(WiFimac[i]);
      Serial.print(":");
    }
    Serial.println("");
 // WiFi.config(ip, remote, netmask, remote, remote);
  connectToWiFi(ssid, pw);
 

 /* if (!rf95.init())
   Serial.println("init failed");  
   int k;
   for(int k=5;WiFimac[k]!=0;k--)   meshNodeId=WiFimac[k];
   
 mesh = new RHMesh(rf95, meshNodeId);  
 if (!mesh->init()) { 
    Serial.println(F("Mesh init failed")); 
    } 
   else 
   {
    Serial.println("Server mesh done look for GATE"); 
   } 
nextTransmit = millis() + random(2000, 5000); //выбираем случайное время передачи
uint8_t error;
uint8_t send_buf[]="HELO";
while(1)
 {
 error = mesh->sendtoWait((uint8_t *)send_buf, 4, GATELORAMESHID); // все отправляем на 1 узел
    if (error != RH_ROUTER_ERROR_NONE) { 
       Serial.println(); 
       Serial.print(F(" ! ")); 
       Serial.println(getErrorString(error)); 
     } else { 
       Serial.println(F(" OK")); 
       break;
     }
 }
 */
}


void loop()
{
readFromSerial();   // читаем из Serial
if(!(buf->isEmpty(buf)))  //Если есть, что передавать
  {
    
   transmitToWiFi(); // отправляем данные 
  }
 

}
