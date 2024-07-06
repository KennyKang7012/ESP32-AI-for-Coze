#include <WiFi.h>
#include <HTTPClient.h>
#include <PubSubClient.h>
#include <Tone32.h>
#include "SPIFFS.h"
#include "Audio.h"

#define BUTTON_CASE   5
#define I2S_LRC       25
#define I2S_BCLK      26
#define I2S_DIN       14
#define I2S_VOL       18

//請修改以下參數
//-------------------------------------------------------------------------------------------
const char*   szSSID  = "Your_WiFi_SSID";
const char*   szPSWD  = "Your_WiFi_Password";
const String  szCozeTriggerKey="Your_Coze_Workflow_Trigger_Key";
const String  szCozeTriggerID="Your_Coze_Workflow_Trigger_ID";
const String  szParameterName = "StoryParameter";
const String  szMQTTBrokerIP = "broker.mqttgo.io";
const int     nMQTTBorkerPort = 1883;
const String  szMQTTopic = "Your_Unique_MQTT_Topic";
//-------------------------------------------------------------------------------------------
String  szCozeTriggerPrompt="";
String  szMQTTData="", szTextData="";
bool    bGenerateFlag=false;
Audio   audio;
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
    pinMode(BUTTON_CASE, INPUT);
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

    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DIN);
    audio.setVolume(I2S_VOL);
    downloadTTStoSPIFFS("Coze童話故事播放器準備完成。","zh-TW","/TTS/tts.mp3");
    playFromSPIFFS("/TTS/tts.mp3");
    while (audio.isRunning()) {
        audio.loop();
        if (!audio.isRunning())
            audio.stopSong();
    }
}

void loop()
{
    fnCheckMQTTConnect();
    if (!bGenerateFlag && digitalRead(BUTTON_CASE) == HIGH) {
        if(analogRead(39)>2048)
            szCozeTriggerPrompt="倚天劍、屠龍刀"; //Your Workflow Trigger Parameter 1
        else
            szCozeTriggerPrompt="華山派、嵩山派、泰山派、衡山派、恆山派"; //Your Workflow Trigger Parameter 2
        tone(27,2048,100,0);
        digitalWrite(2, HIGH);
        if(fnStoryGeneratbyCozeTrigger(szCozeTriggerPrompt, szParameterName, szCozeTriggerID, szCozeTriggerKey)){
            bGenerateFlag=true;
            Serial.println("Send Story Parameter...");
        }
        else{
            bGenerateFlag=false;
            Serial.println("Something is wrong...");
        }
        digitalWrite(2, LOW);
    }
    
    if (szTextData.length() > 0) {
        //Serial.println(szTextData);
        szTextData.replace("\r","");
        szTextData.replace("\n","");
        fnPlayStory();
    }
}

void playFromSPIFFS(String myFileName)
{
    if(myFileName.indexOf("/")!=0)
        myFileName="/"+myFileName;
        
    if(!SPIFFS.begin()){
        if(SPIFFS.format())
            SPIFFS.begin();
    }
    audio.connecttoFS(SPIFFS,String(myFileName).c_str());
}

String URLEncode(const char* msg)
{
    const char *hex = "0123456789abcdef";
    String encodedMsg = "";
    
    while (*msg!='\0'){
        if(('a' <= *msg && *msg <= 'z')||('A' <= *msg && *msg <= 'Z')||('0' <= *msg && *msg <= '9')){
            encodedMsg += *msg;
        }else{
            encodedMsg += '%';
            encodedMsg += hex[*msg >> 4];
            encodedMsg += hex[*msg & 15];
        }
        msg++;
    }
    return encodedMsg;
}

void downloadTTStoSPIFFS(String url,String tl,String file_name)
{
    url=URLEncode(url.c_str());
    url="http://translate.google.com/translate_tts?ie=UTF-8&client=tw-ob&tl="+tl+"&q="+url;
    
    if(!SPIFFS.begin()){
        if(SPIFFS.format())
            SPIFFS.begin();
    }
    File f = SPIFFS.open(file_name, "w");
    if(f){
        HTTPClient http;
        http.begin(url);
        int httpCode = http.GET();
        if (httpCode > 0){
            if (httpCode == HTTP_CODE_OK)
                http.writeToStream(&f);
        }
        else
            Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());

        f.close();
        http.end();
    }
}

void fnPlayStory() 
{
    if (szTextData.length() > 0) {
        while (szTextData.length() > 0) {
            int nKeyWordPos=szTextData.indexOf("。");
            if (nKeyWordPos == -1)
                nKeyWordPos = szTextData.length();
            else
                nKeyWordPos += 3;
            
            //if(nKeyWordPos>=150)
                //nKeyWordPos=szStoryData.indexOf("，")+3;
            String szSubString=szTextData.substring(0, nKeyWordPos);
            Serial.println(szSubString);
            szTextData = szTextData.substring(nKeyWordPos);

            downloadTTStoSPIFFS(szSubString.c_str(),"zh-TW","/TTS/tts.mp3");
            playFromSPIFFS("/TTS/tts.mp3");
            while (audio.isRunning()) {
                audio.loop();
                if (!audio.isRunning())
                    audio.stopSong();
            }
        }
        Serial.println("Play story done!");
    } 
    else {
        downloadTTStoSPIFFS("沒有內容可以播放。","zh-TW","/TTS/tts.mp3");
        playFromSPIFFS("/TTS/tts.mp3");
        while (audio.isRunning()) {
            audio.loop();
            if (!audio.isRunning())
                audio.stopSong();
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
        szTextData = szMQTTData;
        digitalWrite(2,LOW);
    }
}