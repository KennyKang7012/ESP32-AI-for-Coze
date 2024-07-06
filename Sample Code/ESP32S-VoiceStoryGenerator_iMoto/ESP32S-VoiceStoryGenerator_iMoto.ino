#include <WiFi.h>
#include <HTTPClient.h>
#include <PubSubClient.h>
#include <Tone32.h>
#include "SPIFFS.h"
#include "Audio.h"

#define CHAPTER_NUM   4
#define BUTTON_CREATE 34
#define BUTTON_REPLAY 35
#define I2S_LRC       25
#define I2S_BCLK      26
#define I2S_DIN       12
#define I2S_VOL       21

//請修改以下參數
//-------------------------------------------------------------------------------------------
const char*   szSSID  = "Your_WiFi_SSID";
const char*   szPSWD  = "Your_WiFi_Password";
const String  szCozeTriggerKey="Your_Coze_Workflow_Trigger_Key";
const String  szCozeTriggerID="Your_Coze_Workflow_Trigger_ID";
const String  szParamaterName = "UserRequest";
const String  szCozeTriggerPrompt="金門、貢糖、菜刀";
const String  szMQTTBrokerIP = "broker.mqttgo.io";
const int     nMQTTBorkerPort = 1883;
const String  szMQTTopic = "Your_Unique_MQTT_Topic";
//-------------------------------------------------------------------------------------------
String  szMQTTData="", szMP3URLList="", szMP3URLArray[CHAPTER_NUM];
bool    bGenerateFlag=false;
Audio   audio;
WiFiClient client;
PubSubClient mqtt_client(client);
WiFiClientSecure clientSecure;

bool fnVoiceStoryGeneratbyEventTrigger(String szRequestBody, String szParamater, String szTriggerID, String szAPIKey) 
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
    pinMode(BUTTON_CREATE, INPUT);
    pinMode(BUTTON_REPLAY, INPUT);

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
    for(int i=0; i<CHAPTER_NUM; i++)
        szMP3URLArray[i]="";

    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DIN);
    audio.setVolume(I2S_VOL);
    downloadTTStoSPIFFS("Coze有聲童話產生器準備完成。","zh-TW","/TTS/tts.mp3");
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
    if (!bGenerateFlag && digitalRead(BUTTON_CREATE) == HIGH) {
        tone(27,2048,100,0);
        digitalWrite(2, HIGH);
        if(fnVoiceStoryGeneratbyEventTrigger(szCozeTriggerPrompt, szParamaterName, szCozeTriggerID, szCozeTriggerKey)){
            bGenerateFlag=true;
            Serial.println("Start Generate New Story...");
        }
        else{
            bGenerateFlag=false;
            Serial.println("Something is wrong...");
        }
        digitalWrite(2, LOW);
    }
    else if (!bGenerateFlag && digitalRead(BUTTON_REPLAY) == HIGH) {
        tone(27,2048,100,0);
        digitalWrite(2, HIGH);
        if(szMQTTData.length()>0)
        {
            szMP3URLList = szMQTTData;
            fnSplitMP3URL(szMP3URLList);
            for(int i=0; i<CHAPTER_NUM; i++) {
                fnPlayURLMP3(szMP3URLArray[i]);
                szMP3URLArray[i]="";
            }
        }
        else {
            downloadTTStoSPIFFS("沒有故事內容可以播放。","zh-TW","/TTS/tts.mp3");
            playFromSPIFFS("/TTS/tts.mp3");
            while (audio.isRunning()) {
                audio.loop();
                if (!audio.isRunning())
                    audio.stopSong();
            }
        }
        digitalWrite(2, LOW);
    }

    if (szMP3URLArray[0].length() > 0) {
        for(int i=0; i<CHAPTER_NUM; i++) {
            fnPlayURLMP3(szMP3URLArray[i]);
            szMP3URLArray[i]="";
        }
    }
}

void downloadMP3toSPIFFS(String szURL, String szFileName)
{
    if(!SPIFFS.begin()){
        if(SPIFFS.format())
            SPIFFS.begin();
    }
    File f = SPIFFS.open(szFileName, "w");
    if(f){
        HTTPClient http;
        http.begin(szURL);
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

void fnPlayURLMP3(String szMP3URL) 
{
    if (szMP3URL.length()>0 && szMP3URL.indexOf("mp3")>=0) 
    {
        Serial.println(szMP3URL);
        downloadMP3toSPIFFS(szMP3URL, "/TTS/tts.mp3");
        playFromSPIFFS("/TTS/tts.mp3");
        while (audio.isRunning()) {
            audio.loop();
            if (!audio.isRunning())
                audio.stopSong();
        }
        Serial.println("Play story done!");
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

void fnSplitMP3URL(String szOrgMP3URLList)
{
    int i=0;

    for(i=0; i<CHAPTER_NUM; i++)
        szMP3URLArray[i]="";
    
    i=0;
    if (szOrgMP3URLList.length() > 0) {
        while (szOrgMP3URLList.length() > 0) {
            int nKeyWordPos=szOrgMP3URLList.indexOf(", ");
            if (nKeyWordPos == -1)
                nKeyWordPos = szOrgMP3URLList.length();
            
            szMP3URLArray[i++]=szOrgMP3URLList.substring(0, nKeyWordPos);
            Serial.println(szMP3URLArray[i-1]);
            szOrgMP3URLList = szOrgMP3URLList.substring(nKeyWordPos+2);
        }
        Serial.println("Split URL done!");
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
        szMP3URLList = szMQTTData;
        fnSplitMP3URL(szMP3URLList);
        digitalWrite(2,LOW);
  }
}