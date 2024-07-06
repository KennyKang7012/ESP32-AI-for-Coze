#include <WiFi.h>
#include <HTTPClient.h>
#include <PubSubClient.h>
#include <Tone32.h>
#include "SPIFFS.h"
#include "Audio.h"
#include <time.h>

#define BUTTON_START  34
#define I2S_LRC       25
#define I2S_BCLK      26
#define I2S_DIN       12
#define I2S_VOL       20

#define RTC_YEAR      0
#define RTC_MONTH     1
#define RTC_DAY       2
#define RTC_HOUR      3
#define RTC_MIN       4
#define RTC_SECOND    5
#define RTC_WEEKDAY   6

//請修改以下參數
//-------------------------------------------------------------------------------------------
const char*   szSSID  = "Your_WiFi_SSID";
const char*   szPSWD  = "Your_WiFi_Password";
const String  szCozeWebAPIToken ="Your_Coze_API_Token";
const String  szCozeWebAPIBotID = "Your_Coze_Bot_ID";
const String  szCozeWebAPIUser = "Your_Coze_User_ID";
const String  szCozeWebAPIPrompt = "高雄";
int     nClockHour=13;
int     nClockMin =50;
//-------------------------------------------------------------------------------------------
String  szCozeWebAPIConveID="", szResponse="";
WiFiClient client;
PubSubClient mqtt_client(client);
WiFiClientSecure clientSecure;
Audio   audio;

String fnTriggerBotbyCozeAPI(String szAPIKey, String szConveID, String szBotID, String szUser, String szPrompt) 
{ 
    String szRequest = "";

    szCozeWebAPIConveID = String(millis());
    szRequest = "{\"conversation_id\":\"" + szCozeWebAPIConveID + "\", \"bot_id\":\"" + szBotID + "\", \"user\":\"" + szUser + "\", \"query\":\"" + szPrompt + "\", \"stream\":false}";
    Serial.println(szRequest);

    clientSecure.setInsecure();
    if (clientSecure.connect("api.coze.com", 443)) {
        clientSecure.println(String("POST /open_api/v2/chat HTTP/1.1"));
        clientSecure.println("Host: api.coze.com");
        clientSecure.println("Authorization: Bearer " + szAPIKey);
        clientSecure.println("Content-Type: application/json; charset=utf-8");
        clientSecure.println("Content-Length: " + String(szRequest.length()));
        clientSecure.println();
        clientSecure.println(szRequest);
        
        int nTimeout = 90000;
        long lCurTime = millis();
        String szResponse="",szFeedback="";
        while ((lCurTime + nTimeout) > millis()) {
            Serial.print(".");
            delay(1000);
            while (clientSecure.available()) {
                int nPos=-1;

                szResponse=clientSecure.readStringUntil('\n');
                //Serial.println(szResponse);
                nPos=szResponse.indexOf("\"answer\",\"content\":\"");
                if(nPos>=0) {
                    szResponse=szResponse.substring(nPos+20, szResponse.length());
                    szFeedback=szResponse.substring(0, szResponse.indexOf("\",\"content_type\""));
                    szFeedback.replace("\\n\\n", "。");
                    szFeedback.replace("\\n", ", ");
                    //Serial.println("\n"+szFeedback);
                    break;
                }
                else {
                    nPos=szResponse.indexOf("content_policy_violation");
                    if(nPos>=0) {
                        szFeedback="content_policy_violation";
                        Serial.println(szFeedback);
                        break;
                    }
                }
                lCurTime = millis();
            }
            if (szFeedback.length()>0)
                break;
        }
        clientSecure.flush();
        clientSecure.stop();
        if (szFeedback.length()>0)
            return szFeedback;
        else
        {
            Serial.println("Request timeout!");
            return "Request timeout!";
        }
    }
     else {
        Serial.println("Coze connection failed!");
        return "Coze connection failed!";
    }
}

void setup() 
{
    Serial.begin(115200);
    pinMode(2, OUTPUT);
    pinMode(BUTTON_START, INPUT);

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
        for (int i=0; i<3; i++) {
            digitalWrite(2, HIGH);
            delay(500);
            digitalWrite(2, LOW);
            delay(500);
        }
        Serial.println("WiFi Connection Successful!");
    }
    else {
        Serial.println("WiFi Connection Failed!");
        delay(1000);
        ESP.restart();
    }
    configTime(8*3600, 0, "time.stdtime.gov.tw", "time.nist.gov");

    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DIN);
    audio.setVolume(I2S_VOL);
    downloadTTStoSPIFFS("Bot as API 測試機準備完成。","zh-TW","/TTS/tts.mp3");
    playFromSPIFFS("/TTS/tts.mp3");
    while (audio.isRunning()) {
        audio.loop();
        if (!audio.isRunning())
            audio.stopSong();
    }
}

void loop() 
{
    if(digitalRead(BUTTON_START)==HIGH || (get_RTC_time(RTC_HOUR) == nClockHour && get_RTC_time(RTC_MIN) == nClockMin))
    {
        tone(27,2048,100,0);
        digitalWrite(2, HIGH);
        szResponse=fnTriggerBotbyCozeAPI(szCozeWebAPIToken, szCozeWebAPIConveID, szCozeWebAPIBotID, szCozeWebAPIUser, szCozeWebAPIPrompt);
        Serial.println(szResponse);
        delay(1000);
        fnPlayVoice(szResponse);
        digitalWrite(2, LOW);

        while(get_RTC_time(RTC_HOUR) == nClockHour && get_RTC_time(RTC_MIN) == nClockMin)
        {
            digitalWrite(2, HIGH);
            delay(1000);
            digitalWrite(2, LOW);
            delay(1000);
        }
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

void fnPlayVoice(String szTextData) 
{
    if (szTextData.length() > 0) {
        while (szTextData.length() > 0) {
            int nKeyWordPos=szTextData.indexOf("。");
            if (nKeyWordPos == -1)
                nKeyWordPos = szTextData.length();
            else
                nKeyWordPos += 3;
            
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
        Serial.println("Play done!");
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