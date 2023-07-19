#include "wificonfig.h"
#include "config.h"
#include "wiegandOutput2.h"
#include <ESP8266httpUpdate.h>
#include <Redis.h>
#include <ArduinoJson.h>


#define MAX_BACKOFF 300000 // 5 minutes

WiegandOut wiegandOut1(D0,D1);
WiegandOut wiegandOut2(D2,D4);
WiegandOut wiegandOut3(D5,D6);
WiegandOut wiegandOut4(D7,D8);
StaticJsonDocument<5000> doc;


void opengate(WiegandOut bus,unsigned long long numberL,int code,int bits){

  if (bits==26) 
  {
    bus.send(numberL,26,code); // Send 26 bits with facility code
    delay(100);
  }
  else if (bits==32) 
  {
    bus.send(numberL,32,code); // Send 32 bits with facility code
    delay(1000);
  }
  else  if (bits==37) 
  {
    bus.send(numberL,37,code); // Send 37 bits with facility code
    delay(1000);
  }
}


//format info data to call open gate action 
void managetargetinfo(String card_data, String gate){

unsigned long long numberL;
int module=0,gaten=0;

String bits =card_data.substring(0,card_data.indexOf("_"));
String code=card_data.substring(card_data.indexOf("_")+1); //all second part as code_number
String number =code.substring(code.indexOf("_")+1) ;  //aqui es donde debemos extraer el dato de number
code=code.substring(0,code.indexOf("_")); //Now we get only de code


//comprobar si el gate a activar pertenece a mi módulo

for (int i = 0; i < 3; i++) {
    char c = REDIS_MODULE[i];
    Serial.println(c);
   if (c < '0' || c > '9' || c=='\r') break;
   module *= 10;
   module += (c - '0');   
  }
//gaten obtiene el número de puerto en base al módulo en el que estamos
gaten=gate.toInt()-(4*(module-1));   
//solo activasi es un puerto nuestro
if (gaten<5){

  //---------------- Create a long long from a String
numberL = 0; //Will be a long long number

for (int i = 0; i < number.length(); i++) {
    char c = number.charAt(i);
   if (c < '0' || c > '9') break;
   numberL *= 10;
   numberL += (c - '0');   
  }
//----------
  switch (gaten){

    case 1:
      opengate(wiegandOut1,numberL,code.toInt(),bits.toInt());
      break;
    case 2:
      opengate(wiegandOut2,numberL,code.toInt(),bits.toInt());
      break;
    case 3:
      opengate(wiegandOut3,numberL,code.toInt(),bits.toInt());
      break;
    case 4:
      opengate(wiegandOut4,numberL,code.toInt(),bits.toInt());
      break;  
  }
  


  
  
 
  delay(100);
}
}



void update_started() {
  Serial.println(F("CALLBACK:  HTTP update process started"));
}

void update_finished() {
  Serial.println(F("CALLBACK:  HTTP update process finished"));
}

void update_progress(int cur, int total) {
  Serial.printf("CALLBACK:  HTTP update process at %d of %d bytes...\n", cur, total);
}

void update_error(int err) {
  Serial.printf("CALLBACK:  HTTP update fatal error code %d\n", err);
}

void handleOTA(String  host, bool firm_name)
{
  
   WiFiClient client; 
   String ESP_host=ESP_getChipId();
   ESP_host.replace(":","-");
   ESP_host.toLowerCase();
   Serial.println(ESP_host);
 
 // Add optional callback notifiers
    ESPhttpUpdate.onStart(update_started);
    ESPhttpUpdate.onEnd(update_finished);
    ESPhttpUpdate.onProgress(update_progress);
    ESPhttpUpdate.onError(update_error);
    if(!firm_name) ESP_host="all";
    
    t_httpUpdate_return ret = ESPhttpUpdate.update(client, "http://"+host+":"+String (OTA_port)+"/"+ESP_host+".bin");
    // Or:
   // t_httpUpdate_return ret = ESPhttpUpdate.update(client, OTA_host, 80, ESP_host+".bin");

    switch (ret) {
      case HTTP_UPDATE_FAILED:
        Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s\n", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
       
        break;

      case HTTP_UPDATE_NO_UPDATES:
        Serial.println("HTTP_UPDATE_NO_UPDATES");
        
        break;

      case HTTP_UPDATE_OK:
        Serial.println("HTTP_UPDATE_OK");
        
        break;
    }
    

}
void setup()
{
  
  //Change : on MAC by - to avoid http calls
  String ESP_host=ESP_getChipId();
  Serial.begin(115200);
  
 Serial.println(ESP_host);
  setupwifi();
  
  auto backoffCounter = -1;
    auto resetBackoffCounter = [&]() { backoffCounter = 0; };

    resetBackoffCounter();
    while (subscriberLoop(resetBackoffCounter))
    {
        auto curDelay = min((1000 * (int)pow(2, backoffCounter)), MAX_BACKOFF);

        if (curDelay != MAX_BACKOFF)
        {
            ++backoffCounter;
        }
        else{
          ESP.reset();
        }

        Serial.printf("Waiting %lds to reconnect...\n",5);
        delay(5);
    }
    Serial.printf("Done!\n");
    

}

// returning 'true' indicates the failure was retryable; false is fatal
bool subscriberLoop(std::function<void(void)> resetBackoffCounter)
{   
    check_status();
    WiFiClient redisConn;
    
    if (!redisConn.connect(REDIS_ADDR, REDIS_PORT))
    {
        Serial.println(F("Failed to connect to the Redis server!"));
        return true;
    }

    Redis redis(redisConn);
    auto connRet = redis.authenticate(REDIS_PASSWORD);
    if (connRet == RedisSuccess)
    {
        Serial.println(F("Connected to the Redis server!"));
    }
    else
    {
        Serial.printf("Failed to authenticate to the Redis server! Errno: %d\n", (int)connRet);
        return false;
    }
    
    redis.psubscribe("channel-faceidentifyanalytics*");
    redis.psubscribe("ctrl-*");

    Serial.println("Listening...");
    resetBackoffCounter();

    auto subRv = redis.startSubscribing(
        [=](Redis *redisInst, String channel, String msg) {
            Serial.printf("Message on channel '%s': \"%s\"\n", channel.c_str(), msg.c_str());

            //call to open gate action
           if (channel.indexOf("faceidentifyanalytics")>0){

             String gate = channel.substring(channel.indexOf(":")+1); 
             
             
            // Deserialize the JSON document first clean until scene_analitics text
            int indexscene=msg.indexOf("scene_analytics");
            //search key number for get target data
            msg=msg.substring(indexscene-1);
            msg.replace("\\","");
            msg.replace("\"{","{");
            msg.replace("}\"","}");
            msg.replace("\"\"","\"");
            msg='{'+msg;
            
            DeserializationError error = deserializeJson(doc, msg);
            
              // Test if parsing succeeds.
              if (error) {
                Serial.print(F("deserializeJson() failed: "));
                Serial.println(error.f_str());
                
              }
              else {
               JsonObject documentRoot = doc["scene_analytics"]["face_identify"].as<JsonObject>();
               //get key name for first objet on json
                for (JsonPair keyValue : documentRoot) {
                  
                
                 
                  
                  //call to manage target data and gate
                  managetargetinfo(keyValue.key().c_str(),gate);
                  }
                }
            
            }

            else if (channel == "ctrl-OTA")
            {
                
                Serial.printf("Required action '%s'\n", msg.c_str());
                handleOTA(msg,0);
            }
            else if (channel == "ctrl-OTAALL")
            {
                
                Serial.printf("Required action '%s'\n", msg.c_str());
                handleOTA(msg,1);
            }
                else if (channel == "ctrl-reconfig"){
                  resetwifi();
                }
            
           
           
        },
        [=](Redis *redisInst, RedisMessageError err) {
            Serial.printf("Subscription error! ");});

    redisConn.stop();
    Serial.printf("Connection closed! (%d)\n", subRv);
    ESP.reset();
    return subRv == RedisSubscribeServerDisconnected; // server disconnect is retryable, everything else is fatal
}

  
  
void loop()
{ 
 
}
