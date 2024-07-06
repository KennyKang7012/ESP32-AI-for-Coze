#include <WiFi.h>
#include <HTTPClient.h>
#include <PubSubClient.h>
#include <Tone32.h>

#define BUTTON_BOTPMT 5

//請修改以下參數
//-------------------------------------------------------------------------------------------
const char*   szSSID  = "Your_WiFi_SSID";
const char*   szPSWD  = "Your_WiFi_Password";
const String  szMQTTBrokerIP = "broker.mqttgo.io";
const int     nMQTTBorkerPort = 1883;
const String  szMQTTopic = "Your_Unique_MQTT_Topic";
//-------------------------------------------------------------------------------------------
String  szCozeTriggerKey="";
String  szCozeTriggerID="";
String  szParamaterName = "";
String  szCozeTriggerPrompt="";
String  szMQTTData="", szTextData="";
bool    bGenerateFlag=false;
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
        while (!mqtt_client.connect(szCozeTriggerKey.c_str())) {
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
    pinMode(BUTTON_BOTPMT, INPUT);
    pinMode(39, INPUT);

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
        while (!mqtt_client.connect(szCozeTriggerKey.c_str())) {
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
}

void loop()
{
    fnCheckMQTTConnect();
    if (!bGenerateFlag && digitalRead(BUTTON_BOTPMT) == HIGH) {
        if(analogRead(39)>2048) {
            digitalWrite(2, HIGH);
            tone(27,2048,100,0);
            szCozeTriggerKey="Your_Coze_BotPrompt_Trigger_Key";
            szCozeTriggerID="Your_Coze_BotPrompt_Trigger_ID";
            szParamaterName = "ImagePrompt";
            szCozeTriggerPrompt="高雄";
            if(fnStoryGeneratbyCozeTrigger(szCozeTriggerPrompt, szParamaterName, szCozeTriggerID, szCozeTriggerKey)){
                bGenerateFlag=true;
                Serial.println("Create Image Request...");
            }
            else{
                bGenerateFlag=false;
                Serial.println("Something is wrong...");
            }
            digitalWrite(2, LOW);
        }
        else {
            bGenerateFlag=true;
            digitalWrite(2, HIGH);
            tone(27,2048,100,0);
            if(szMQTTData.length()>0)
            {
                szCozeTriggerKey="Your_Coze_Plugin_Trigger_Key";
                szCozeTriggerID="Your_Coze_Plugin_Trigger_ID";
                szParamaterName = "ImageURL";
                szCozeTriggerPrompt=szMQTTData;
                if(fnStoryGeneratbyCozeTrigger(szCozeTriggerPrompt, szParamaterName, szCozeTriggerID, szCozeTriggerKey))
                    Serial.println("Send LINE Notify...");
                else
                    Serial.println("Something is wrong...");
            }
            else {
                Serial.println("Nothing to send...");
                delay(1000);
            }
            digitalWrite(2, LOW);
            bGenerateFlag=false;
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
        digitalWrite(2,HIGH);
        tone(27,1046,500,0);
        szMQTTData = szGetMQTTData;
        szTextData = szMQTTData;
        Serial.println(szTextData);
        digitalWrite(2,LOW);
    }
}