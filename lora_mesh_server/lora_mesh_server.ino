#include <SPI.h>
#include <RH_RF95.h>
//#include <RHRouter.h> 
#include <RHMesh.h> 
#include <WiFi.h>
#include <RingBuf.h>
#define LISTENTIME 1000
#define GPSPACKETLENGTH 22
//Include required lib so Arduino can communicate with Yun Shield
//#include <FileIO.h>
//#include <Console.h>
#define GATELORAMESHID 1
#define SERIAL1SPEED 38400
#define DEBUG

// Singleton instance of the radio driver
RH_RF95 rf95(18,26);
RHMesh *mesh;
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
;

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
//----------------------------------------------------------------------------------
void readFromSerial()
{
  struct Event SERIAL_buf;
  uint8_t i=1;
  while(Serial.available()&&(i<RH_MESH_MAX_MESSAGE_LEN-2))
  {
  SERIAL_buf.data_buf[i]=(unsigned char)Serial.read();
#ifdef DEBUG
Serial.print(SERIAL_buf.data_buf[i]);
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
Serial.print(SERIAL_buf.data_buf[i]);
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

#ifdef DEBUG
Serial.print("In buffer:");
Serial.println(buf->numElements(buf));
#endif    
   }

}

//----------------------------------------------------------------------------------
void transmitToLoRa()
{
  uint8_t i=0;
  uint8_t j=0;
  uint8_t message_size=0;
  byte send_buf[RH_MESH_MAX_MESSAGE_LEN];
  struct Event ex; 
//То, что buf не пустой проверяем в loop
  buf->pull(buf, &ex);
  message_size=ex.len;
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
    Serial.begin(115200);
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


  if (!rf95.init())
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
}


void loop()
{
readFromSerial();   // читаем из Serial
if(!(buf->isEmpty(buf)))  //Если есть, что передавать
  {
  if(nextTransmit<millis()) // Если вышло время задержки
   {
   transmitToLoRa(); // отправляем данные 
   nextTransmit = millis() + random(2000, 5000); //выставляем задержку следующей передачи
   }
  }
 

}
