#define MESHNODEID 1
#include <SPI.h>
#include <RH_RF95.h>
//#include <RHRouter.h> 
#include <RHMesh.h> 

#define LISTENTIME 1000
#define GPSPACKETLENGTH 22
//Include required lib so Arduino can communicate with Yun Shield
//#include <FileIO.h>
//#include <Console.h>

// Singleton instance of the radio driver
RH_RF95 rf95(18,26);
RHMesh *mesh;

//int led = 4;

char LORA_buf[RH_MESH_MAX_MESSAGE_LEN];

int reset_lora = 23;
 uint8_t LoRaERROR=0;
 union ufb {
      float f;
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

//----------------------------------------------------------------------------------
// Разбор сообщения прешедшего из LoRa
void parsLoRaMessage(uint8_t from)
{
  uint8_t len = sizeof(LORA_buf);
  uint8_t i=0;
  while (i<len)
   {
    // в начало цикла попадаем только вначале нового сообщения (сообщения вся посылка = buf, пакет часть сообщения, 1 байт задает тип заканчивается двумя байтами ED
     switch((char)LORA_buf[i])
      {
        case 'G': // Сообщения типа GPS 
         {      // пока просто выводим в serial считаем, что оно 20 байта. +2 байта 'ED' 
          i++;
                    
          if((char)LORA_buf[i+GPSPACKETLENGTH-2]=='E' && (char)LORA_buf[i+GPSPACKETLENGTH-2]=='D')
          {
          ulat.b[0]=LORA_buf[i];
          ulat.b[1]=LORA_buf[i+1];
          ulat.b[2]=LORA_buf[i+2];
          ulat.b[3]=LORA_buf[i+3];
          ulon.b[0]=LORA_buf[i+4];
          ulon.b[1]=LORA_buf[i+5];
          ulon.b[2]=LORA_buf[i+6];
          ulon.b[3]=LORA_buf[i+7];
          uspd.b[0]=LORA_buf[i+8];
          uspd.b[1]=LORA_buf[i+9];
          uspd.b[2]=LORA_buf[i+10];
          uspd.b[3]=LORA_buf[i+11];
          udt.b[0]=LORA_buf[i+12];
          udt.b[1]=LORA_buf[i+13];
          udt.b[2]=LORA_buf[i+14];
          udt.b[3]=LORA_buf[i+15];
          utm.b[0]=LORA_buf[i+16];
          utm.b[1]=LORA_buf[i+17];
          utm.b[2]=LORA_buf[i+18];
          utm.b[3]=LORA_buf[i+19];
      Serial.print("GPS;");
      Serial.print(ulat.l);
      Serial.print(";"); 
      Serial.print(ulon.l);
      Serial.print(";"); 
      Serial.print(udt.l);
      Serial.print(";"); 
      Serial.print(utm.l);
      Serial.print(";from:");
      Serial.print(from, HEX);
      Serial.print(";RSSI:");
      Serial.print(rf95.lastRssi(), DEC);
      Serial.println("");
      i=i+22;
         }
        else
        {
          Serial.print("Error wrong GPS packet length");
          LoRaERROR=11; 
        }
       break;
       } // case 'G':
      case 'I':
       {
        int j;
       for(j=i;j<len;j++)
         {
          if((char)LORA_buf[j-1]=='E' && (char)LORA_buf[j]=='D') //Ищем конец пакета или конец сообщения
           {
            break;
           }
         }
        if(j<len)
        {
        for(i=i+1;i<j-1;i++)
         {
         Serial.write(LORA_buf[i]); //выводим пакет в Serial
         }
        i=j; // переставляем указатель на конец пакета 
        }
       else
        {
        Serial.print("SI packet error");
        LoRaERROR=21;  
        }
       }
      }
     if( LoRaERROR>0)  // если битый пакет или его часть ищем следующий или конец
     {
       while(i<len)
        {
         i++;
         if((char)LORA_buf[i-2]=='E' && (char)LORA_buf[i-1]=='D') //Ищем конец пакета или конец сообщения
           {
            LoRaERROR=0;
            break;
           }
        }
     }
   } // while (i<len)
  
}
//----------------------------------------------------------------------------------





void setup() 
{
   pinMode(reset_lora, OUTPUT);     
   digitalWrite(reset_lora, LOW);   
   delay(1000);
   digitalWrite(reset_lora, HIGH); 

    Serial.begin(115200);
    Serial.println("start server x");
    Serial.println("");


  if (!rf95.init())
   Serial.println("init failed");  
 mesh = new RHMesh(rf95, MESHNODEID);  
 if (!mesh->init()) { 
    Serial.println(F("Mesh init failed")); 
    } 
   else 
   {
    Serial.println("mesh done"); 
   } 
}

void loop()
{
       uint8_t len = sizeof(LORA_buf); 
       uint8_t from; 
       if (mesh->recvfromAckTimeout((uint8_t *)LORA_buf, &len, LISTENTIME, &from)) 
   //    if (mesh->recvfromAck((uint8_t *)buf, &len,  &from)) 
       { 
        parsLoRaMessage(from);
       }  
    /*  GPS простой
  dataString="";
  if (rf95.available())
  {
    byte buf[24];
    byte len = sizeof(buf);
    if (rf95.recv(buf, &len))
    {
    ulat.b[0]=buf[0];
    ulat.b[1]=buf[1];
    ulat.b[2]=buf[2];
    ulat.b[3]=buf[3];

    ulon.b[0]=buf[4];
    ulon.b[1]=buf[5];
    ulon.b[2]=buf[6];
    ulon.b[3]=buf[7];

    uspd.b[0]=buf[8];
    uspd.b[1]=buf[9];
    uspd.b[2]=buf[10];
    uspd.b[3]=buf[11];

    udt.b[0]=buf[12];
    udt.b[1]=buf[13];
    udt.b[2]=buf[14];
    udt.b[3]=buf[15];

   utm.b[0]=buf[16];
   utm.b[1]=buf[17];
   utm.b[2]=buf[18];
   utm.b[3]=buf[19];

   uimei.b[0]=buf[20];
   uimei.b[1]=buf[21];
   uimei.b[2]=buf[22];
   uimei.b[3]=buf[23];

      Serial.print(ulat.l);
     Serial.print(";"); 
      Serial.print(ulon.l);
     Serial.print(";XXRSSI: ");
      Serial.print(rf95.lastRssi(), DEC);
        Serial.println("");
     
    }
    else
    {
        Serial.println("recv failed");   
    }
  }
  */
}





