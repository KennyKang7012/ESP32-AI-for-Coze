#include <WiFi.h>
#include <HTTPClient.h>
#include <PubSubClient.h>
#include <Tone32.h>
#include <time.h>

#define BUTTON_CREATE 5

#define RTC_YEAR      0
#define RTC_MONTH     1
#define RTC_DAY       2
#define RTC_HOUR      3
#define RTC_MIN       4
#define RTC_SECOND    5
#define RTC_WEEKDAY   6
#define BATCH_NUMBER  2

//請修改以下參數
//-------------------------------------------------------------------------------------------
const char*   szSSID  = "Your_WiFi_SSID";
const char*   szPSWD  = "Your_WiFi_Password";
const String  szCozeTriggerKey[BATCH_NUMBER]={"Your_1st_Trigger_Key", "Your_2nd_Trigger_Key"};
const String  szCozeTriggerID[BATCH_NUMBER]={"Your_1st_Trigger_ID", "Your_2nd_Trigger_ID"};
const String  szParamaterName[BATCH_NUMBER]={"ImagePrompt", "ImageURL"};
const String  szTriggerParameter="一個年輕女孩，手捧著鮮花，微笑的看著天空，水彩畫風格。圖上還有大大的「Good Morning」文字。";
const String  szMQTTBrokerIP = "broker.mqttgo.io";
const int     nMQTTBorkerPort = 1883;
const String  szMQTTopic = "Your_Unique_MQTT_Topic";
int     nClockHour=13;
int     nClockMin =30;
//-------------------------------------------------------------------------------------------
String  szMQTTData="";
bool    bGenerateFlag=false;
int     nBatchNum=-1;
WiFiClient client;
PubSubClient mqtt_client(client);
WiFiClientSecure clientSecure;

bool fnStoryGeneratbyCozeTrigger(String szRequestBody, String szParamater, String szTriggerID, String szAPIKey) 
{ 
    bool  bResult=false;

    szRequestBody = "{\"" + szParamater + "\":\"" + szRequestBody + "\"}";

    clientSecure.setInsecure();
    if (clientSecure.connect("api.coze.com", 443)) {
        clientSecure.println(String("POST /api/trigger/v1/webhook/biz_id/bot_platform/hook/") + szTriggerID + String(" HTTP/1.1"));
        clientSecure.println("Host: api.coze.com");
        clientSecure.println("Authorization: Bearer " + szAPIKey);
        clientSecure.println("Content-Type: application/json; charset=utf-8");
        clientSecure.println("Content-Length: " + String(szRequestBody.length()));
        clientSecure.println();
        clientSecure.println(szRequestBody);
        
        String szResponse="",szFeedback="";
        int nTimeout = 5000;
        long lCurTime = millis();
        while ((lCurTime + nTimeout) > millis()) {
            Serial.print(".");
            delay(500);      
            while (clientSecure.available()) {
                szResponse=clientSecure.readStringUntil('\n');
                //Serial.println(szResponse);
                if(szResponse.indexOf("\"message\":\"Success\"")>=0){
                    Serial.println();
                    szFeedback=szResponse;
                    bResult=true;
                    break;
                }
                lCurTime = millis();
            }
            if (szFeedback.length()>0) 
                break;
        }
        clientSecure.flush();
        clientSecure.stop();

        return bResult;
    }
    return bResult;
}

void fnCheckMQTTConnect() 
{
    if (mqtt_client.connected())
        mqtt_client.loop();
    else {
        WiFi.disconnect();
        WiFi.reconnect();
        while (!mqtt_client.connect(szCozeTriggerKey[0].c_str())) {
            Serial.print(".");
            delay(1000);
        }
        mqtt_client.subscribe(szMQTTopic.c_str());
        Serial.println("MQTT Server connected!");
    }
}

void setup()
{
    Serial.begin(115200);
    pinMode(2, OUTPUT);
    pinMode(BUTTON_CREATE, INPUT);

    WiFi.mode(WIFI_STA);
    WiFi.begin(szSSID, szPSWD);
    long int nlCurTime=millis();
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.print(".");
        if (millis()-nlCurTime >= 10000) 
            break;
    }
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("WiFi Connection Successful!");
        mqtt_client.setServer(szMQTTBrokerIP.c_str(), nMQTTBorkerPort);
        while (!mqtt_client.connect(szCozeTriggerKey[0].c_str())) {
            Serial.print(".");
            delay(1000);
        }
        mqtt_client.subscribe(szMQTTopic.c_str());
        mqtt_client.setCallback(fnMQTTCallback);
        mqtt_client.setBufferSize(8192);
        Serial.println("MQTT Server connected!");

        for (int i=0; i<3; i++) {
            digitalWrite(2, HIGH);
            delay(500);
            digitalWrite(2, LOW);
            delay(500);
        }
    }
    else {
        Serial.println("WiFi Connection Failed!");
        delay(1000);
        ESP.restart();
    }
    configTime(8*3600, 0, "time.stdtime.gov.tw", "time.nist.gov");
}

void loop()
{
    fnCheckMQTTConnect();
    if (nBatchNum<0 && (digitalRead(BUTTON_CREATE) == HIGH || (get_RTC_time(RTC_HOUR) == nClockHour && get_RTC_time(RTC_MIN) == nClockMin))) {
        nBatchNum=0;
        tone(27,2048,100,0);
        digitalWrite(2, HIGH);
        szMQTTData="";
        if(fnStoryGeneratbyCozeTrigger(szTriggerParameter, szParamaterName[nBatchNum], szCozeTriggerID[nBatchNum], szCozeTriggerKey[nBatchNum])){
            bGenerateFlag=true;
            Serial.println("Trigger First Event Trigger...");
        }
        else{
            bGenerateFlag=false;
            Serial.println("Something is wrong...");
        }
        digitalWrite(2, LOW);
    }

    if (szMQTTData.length() > 0) {
        if(nBatchNum>=0 && (nBatchNum < BATCH_NUMBER-1)) {
            nBatchNum++;
            tone(27,1536,100,0);
            if(fnStoryGeneratbyCozeTrigger(szMQTTData, szParamaterName[nBatchNum], szCozeTriggerID[nBatchNum], szCozeTriggerKey[nBatchNum])){
                bGenerateFlag=true;
                Serial.println("Trigger Next Event Trigger...");
            }
            else{
                bGenerateFlag=false;
                Serial.println("Something is wrong...");
            }
        }
        else {
            while(get_RTC_time(RTC_HOUR) == nClockHour && get_RTC_time(RTC_MIN) == nClockMin)
            {
                digitalWrite(2, HIGH);
                delay(1000);
                digitalWrite(2, LOW);
                delay(1000);
            }
            nBatchNum=-1;
        }
    }
}

String getTopicData(String myTopic, String inTopic, byte* payload ,unsigned int length)
{
    String TopicData;
    if(inTopic == myTopic){
        for (int i = 0; i < length; i++) {
            TopicData += (char)payload[i];
        }
        return TopicData;
    }else{
        return "";
    }
}

void fnMQTTCallback(char* topic, byte* payload, unsigned int length)
{
    String szGetMQTTData=getTopicData(szMQTTopic, topic, payload, length);
    //Serial.println(szGetMQTTData);
    if (bGenerateFlag && szGetMQTTData.length()>0 && szMQTTData!=szGetMQTTData) 
    {
        bGenerateFlag=false;
        tone(27,1046,500,0);
        digitalWrite(2,HIGH);
        szMQTTData = szGetMQTTData;
        digitalWrite(2,LOW);
  }
}

int get_RTC_time(int dataType) 
{
    struct tm timeinfo;
    getLocalTime(&timeinfo);
    if(dataType==RTC_YEAR){
        return 1900 + timeinfo.tm_year;
    }else if(dataType==RTC_MONTH){
        return 1 + timeinfo.tm_mon;
    }else if(dataType==RTC_DAY){
        return timeinfo.tm_mday;
    }else if(dataType==RTC_HOUR){
        return timeinfo.tm_hour;
    }else if(dataType==RTC_MIN){
        return timeinfo.tm_min;
    }else if(dataType==RTC_SECOND){
        return timeinfo.tm_sec;
    }else if(dataType==RTC_WEEKDAY){
        return timeinfo.tm_wday;
    }
}