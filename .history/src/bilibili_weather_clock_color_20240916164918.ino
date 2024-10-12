/**********************************************************************
项目名称/Project          : 哔哩哔哩天气时钟彩屏版
程序名称/Program name     : bilibili_weather_clock_color
团队/Team                : 太极创客团队 / Taichi-Maker (www.taichi-maker.com)
作者/Author              : CYNO朔
日期/Date（YYYYMMDD）     : 20210512
程序目的/Purpose          : 
彩屏哔哩哔哩天气时钟。本时钟可显示实时天气信息和B站Up的粉丝数量和视频播放量。

如需了解本程序的详细使用说明，请参考太极创客网站的本项目介绍页面：
http://www.taichi-maker.com/bilibili_weather_clock_color/
-----------------------------------------------------------------------
本程序所属仓库网址
http://www.taichi-maker.com/bilibili_weather_clock_color/
-----------------------------------------------------------------------
其它说明 / Other Description
本程序为太极创客团队制作的免费视频教程《零基础入门学用物联网 》中一部分。该教程系统的
向您讲述ESP8266的物联网应用相关的软件和硬件知识。以下是该教程目录页：
http://www.taichi-maker.com/homepage/esp8266-nodemcu-iot/                                
***********************************************************************/
#include <TimeLib.h>
#include <ESP8266WiFi.h>      // 调用ESP8266WiFi功能
#include <WiFiUdp.h>
#define FS_NO_GLOBALS         // 调用ESP8266的SPIFFS FLASH
#include <FS.h>
#include <TFT_eSPI.h>         
#include <TFT_eFEX.h>         
#include <WiFiManager.h>      
#include <ESP8266_BiliBili.h>    
#include <ESP8266_Seniverse.h>
#include <PubSubClient.h>
#include "chinese.h"  
#include "settings.h"  
#include <LittleFS.h>



//#define USE_MQTT

#define BANNER_HEIGHT 20      // 横幅高
#define BANNER_WIDTH 320      // 横幅宽
#define HALF_BANNER_WIDTH 160 // 横幅宽
#define BANNER_BG  TFT_BLACK  // 横幅背景色
#define BANNER_FG  TFT_WHITE  // 横幅文字色

//MQTT通讯相关
String controlTopic = WiFi.macAddress() + "-control"; // 设备控制主题
String chatTopic = WiFi.macAddress() + "-chat";       // 用户消息主题
String reportTopic = WiFi.macAddress() + "-report";   // 系统报告主题
String userControlString;                       // 用户通过MQTT协议发来的设置信息
String userChatString;                          // 用户通过MQTT协议发来的聊天信息
bool userChatFlag = false;                      // 告知系统是否有用户聊天信息需要显示

// TFT屏幕显示相关对象和变量
TFT_eSPI tft = TFT_eSPI();
TFT_eFEX  fex = TFT_eFEX(&tft); 
bool bgChangeFlag;    // 背景更改显示标识
int bgId = START_BG_NUM;             // 背景序号

// NTP服务
WiFiUDP Udp;
unsigned int localPort = 8888;  // local UDP port
time_t prevDisplay = 0; // 上一次显示的时间

const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

//Ticker相关
//Ticker ticker;
int bgCounter;

//b站API相关
FansInfo fansInfo(biliUserId);       // 建立对象用于获取粉丝信息
VideoInfo videoInfo(biliBV);     // 建立对象用于获取视频信息
int biliCounter;
bool biliUpdateFlag;
long biliFans;
long biliViews;

// 建立对象和变量用于获取心知天气信息
WeatherNow weatherNow;
Forecast forecast; 
int weatherUpdateCounter;
bool weatherUpdateFlag;
byte weatherInfoDisplayCounter;
byte weatherInfoId = 1;

//建立MQTT协议所需对象
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

void setup() {
  Serial.begin(9600);
  
  if (!SPIFFS.begin()) {                              // 检查SPIFFS是否可以正确启动
    Serial.println("SPIFFS initialisation failed!"); // 如果SPIFFS无法启动则输出错误提示
    while (1) yield();                                // 然后程序停在此处
  }
  Serial.println("\r\nSPIFFS initialised.");         // 如果SPIFFS正确启动则输出启动成功信息 

  // 初始化tft屏幕
  tft.begin();
  tft.setRotation(1);         // 设置屏幕显示方向
  tft.fillScreen(TFT_WHITE);  // 将屏幕填充黑色

  // ---WiFi准备工作---  
  // 建立WiFiManager对象
  WiFiManager wifiManager;  

  // 自动连接WiFi。以下语句的参数是连接ESP8266时的WiFi名称
  wifiManager.autoConnect("Taichi-Maker-IoT-Display", "12345678");  

  #ifdef USE_MQTT
    // ---MQTT准备工作---
    // 设置MQTT服务器和端口号
    mqttClient.setServer(mqttServer, 1883);
    // 设置MQTT订阅回调函数
    mqttClient.setCallback(receiveCallback);
    // 连接MQTT服务器
    connectMQTTserver();
    Serial.println("Report Topic:");
    Serial.println(reportTopic);
  #endif

  //---bili信息初始化---
  biliFans = getBiliFans();
  biliViews = getBiliViews();

  // 天气信息
  weatherNow.config(reqUserKey, reqLocation, reqUnit);
  forecast.config(reqUserKey, reqLocation, reqUnit);
  updateWeatherInfo();  // 启动后获取天气信息  

  // ---NTP初始化---
  ntpInit(); 
  
  //---在屏幕显示初始信息---
  displayBg(bgId);
  tft.fillRect(0, 0, BANNER_WIDTH, BANNER_HEIGHT, BANNER_BG);
  tft.fillRect(0, 220, BANNER_WIDTH, BANNER_HEIGHT, BANNER_BG);
  
  //显示星期二字
  tft.drawBitmap(260,0,xing,20,20,BANNER_FG); // 显示星字
  tft.drawBitmap(280,0,qi1,20,20,BANNER_FG);  // 显示期字  

  //初始化时显示天气信息  
  displayWeatherInfo(1);

  //---ticker相关---
  //ticker.attach(1, tickerCount);
}



void loop(){
  if (timeStatus() != timeNotSet) {
    if (now() != prevDisplay) { //update the display only if time has changed
      prevDisplay = now();
      //digitalClockDisplay();
      #ifdef USE_MQTT
        Serial.println("");
        Serial.println("controlTopic:");
        Serial.println(controlTopic);
        Serial.println("chatTopic:");
        Serial.println(chatTopic);      
        Serial.println("reportTopic:");
        Serial.println(reportTopic);  
        Serial.println("");        
      #endif
      
      tick();  // 计时函数
    }
  }
    if (bgChangeFlag == true){
    bgChangeFlag = false;    
    bgId++;
    if (bgId > END_BG_NUM) bgId = START_BG_NUM;
    displayBg(bgId);
  }
 // 更新bili信息 
  if (biliUpdateFlag == true){
    Serial.println("Update Bili Info");
    
    biliUpdateFlag = false;    
    biliFans = getBiliFans();
    biliViews = getBiliViews();
  }

  // 更新天气信息 
  if (weatherUpdateFlag == true){
    Serial.println("Update Weather Info");
    
    weatherUpdateFlag = false;  
    updateWeatherInfo();   
    displayWeatherInfo(weatherInfoId);
  }
  #ifdef USE_MQTT
  if (mqttClient.connected()) { // 如果开发板成功连接服务器
    mqttClient.loop();          // 处理信息以及心跳
  } else {                      // 如果开发板未能成功连接服务器
    connectMQTTserver();        // 则尝试连接服务器
  } 
  #endif
}

//void digitalClockDisplay()
//{
//  // digital clock display of the time
//  Serial.print(hour());
//  printDigits(minute());
//  printDigits(second());
//  Serial.print(" ");
//  Serial.print(day());
//  Serial.print(".");
//  Serial.print(month());
//  Serial.print(".");
//  Serial.print(year());
//  Serial.println();
//}

//void printDigits(int digits){
//  // utility for digital clock display: prints preceding colon and leading 0
//  Serial.print(":");
//  if (digits < 10)
//    Serial.print('0');
//  Serial.print(digits);
//}
//---时间显示相关函数开始
void displayTime(){
  String timeString;
  String dateString;
  String dateTimeString;
  
  timeString = adjDigit(hour()) + ":" + adjDigit(minute()) + ":" + adjDigit(second());
  dateString = adjDigit(month()) + "-" + adjDigit(day());
  dateTimeString = dateString + "           " + timeString;

  // 设置时钟显示位置
  tft.setViewport(0, 0, BANNER_WIDTH, BANNER_HEIGHT);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(BANNER_FG, BANNER_BG);  //设置文字颜色和背景色
  tft.setTextPadding(10); //设置文字padding，覆盖旧字
  tft.drawString(dateTimeString, 0, 0, 4); 

  //显示星期几
  if (weekday() == 1) {
    tft.fillRect(300, 0, 20, 20, BANNER_BG);
    tft.drawBitmap(300,0,tian,20,20,BANNER_FG);// 星期天显示天字
  } else {
    tft.fillRect(300, 0, 20, 20, BANNER_BG);
    tft.setTextDatum(TR_DATUM);
    tft.drawString(String(weekday()-1), 320, 0, 4); //显示星期几数字
  }
   
  tft.resetViewport();    
}

String adjDigit(int number){
  if (number >= 10){
    return String(number);
  } else {
    return String("0" + String(number));
  }
}

//---时间显示相关函数结束---

void displayBg(int bgId){
  String bgName = "/bg" + String(bgId) + ".jpg";
  fex.drawJpeg(bgName, 0, BANNER_HEIGHT); 
  if (userChatFlag == true){
    Serial.print("userChatFlag = ");Serial.println(userChatFlag);
    showChatMessage(userChatString);
  }
  Serial.println(bgName);
}

//--- NTP相关函数开始 ---
// ntp初始化
void ntpInit(){
  Serial.println("Starting UDP");
  Udp.begin(localPort);
  Serial.print("Local UDP port: ");
  Serial.println(Udp.localPort());
  Serial.println("-------------");
  setSyncProvider(getNtpTime);// 设置对时函数
  setSyncInterval(timeCheckInterval);        // 每隔10秒钟对时一次
}

time_t getNtpTime(){
  IPAddress ntpServerIP; // NTP server's ip address

  while (Udp.parsePacket() > 0) ; // discard any previously received packets
  Serial.println("Transmit NTP Request");
  WiFi.hostByName(ntpServerName, ntpServerIP);
  Serial.print(ntpServerName);
  Serial.print(": ");
  Serial.println(ntpServerIP);
  sendNTPpacket(ntpServerIP);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Serial.println("Receive NTP Response");
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  Serial.println("No NTP Response :-(");
  return 0; // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address){
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}
//--- NTP相关函数结束 ---

//--- 定时调用计时器函数 --- 
void tick(){
  displayTime(); // 每秒更新时钟显示
  
  bgCounter++;
  // 哔哩哔哩更新计时器
  biliCounter++;

  //天气更新计时器
  weatherUpdateCounter++;
  weatherInfoDisplayCounter++;
  
  // 达到背景变换时间
  if (bgCounter > bgChgInterval){
    bgCounter = 0;
    bgChangeFlag = true;
  }

  // 达到bili信息更新时间
  if (biliCounter > biliUpdateInterval){
    biliCounter = 0;
    biliUpdateFlag = true;
  }  

  // 达到天气信息更新时间
  if (weatherUpdateCounter > weatherUpdateInterval){
    weatherUpdateCounter = 0;
    weatherUpdateFlag = true;
  }   

 // 达到天气信息变化时间
  if (weatherInfoDisplayCounter > weatherDisplayInterval){
    weatherInfoDisplayCounter = 1;
    weatherInfoId++;
    if(weatherInfoId > MAX_INFO_ID) {
      weatherInfoId = 1;
    }
    displayWeatherInfo(weatherInfoId);
  }    
}
//--- 定时调用计时器函数结束 --- 

//--- b站API相关函数 ---
long getBiliFans(){
  if(fansInfo.update()){  // 更新信息成功   
    return fansInfo.getFansNumber();       
  } else {     // 更新失败
    Serial.println("Update Fail...");  
    return -1;  
  }
}

long getBiliViews(){
  if(videoInfo.update()){  // 更新信息成功       
   return videoInfo.getPlay();        
  } else {     // 更新失败
    Serial.println("Update Fail...");  
    return -1;        
  }    
}
//--- b站API相关函数结束 ---


//--- 心知天气相关函数 ---
void updateWeatherInfo(){

  weatherUpdateCounter = 0;
  if(weatherNow.update()){  // 更新天气信息
    Serial.println("Weather Now Updated: "); 
    Serial.println(weatherNow.getLastUpdate()); // 获取服务器更新天气信息时间 
  } else {    // 更新失败
    Serial.println("Weather Now Update Fail.");   
    Serial.print("Server Response: ");          // 输出服务器响应状态码供用户查找问题
    Serial.println(weatherNow.getServerCode()); // 心知天气服务器错误代码说明可通过以下网址获取
  } 
  
  if(forecast.update()){  // 更新天气信息  
      Serial.println("Weather Forecast Updated: "); 
      Serial.println(forecast.getLastUpdate());                
  } else {    // 更新失败
    Serial.println("Weather Forecast Update Fail.");   
    Serial.print("Server Response: ");          // 输出服务器响应状态码供用户查找问题
    Serial.println(weatherNow.getServerCode()); // 心知天气服务器错误代码说明可通过以下网址获取
  } 
}
// 显示天气信息
void displayWeatherInfo(int infoId){
  const uint8_t* chn1;
  const uint8_t* chn2;
  const uint8_t* chn3;
  const uint8_t* chn4;
  int weatherCode;    // 天气或温度代码信息
  bool chineseFlag;   // 中文显示标识
  
  if (infoId == 1){
    chn1 = dang;
    chn2 = qian;
    chn3 = tian;
    chn4 = qi;
    weatherCode = weatherNow.getWeatherCode();
    chineseFlag = true;
    
  } else if (infoId == 2){
    chn1 = dang;
    chn2 = qian;
    chn3 = qi;
    chn4 = wen;
    weatherCode = weatherNow.getDegree();
    chineseFlag = false;
    
  } else if (infoId == 3){
    chn1 = jin;
    chn2 = tian;
    chn3 = zui;
    chn4 = gao;
    weatherCode = forecast.getHigh(0);
    chineseFlag = false;
    
  } else if (infoId == 4){
    chn1 = jin;
    chn2 = tian;
    chn3 = zui;
    chn4 = di;
    weatherCode = forecast.getLow(0);   
    chineseFlag = false;
    
  } else if (infoId == 5){
    chn1 = ming;
    chn2 = tian;
    chn3 = tian;
    chn4 = qi;
    weatherCode = forecast.getDayCode(1);  
    chineseFlag = true;  
    
  } else if (infoId == 6){
    chn1 = ming;
    chn2 = wan;
    chn3 = tian;
    chn4 = qi;
    weatherCode = forecast.getNightCode(1);
    chineseFlag = true;
    
  } else if (infoId == 7){
    chn1 = ming;
    chn2 = tian;
    chn3 = zui;
    chn4 = gao;
    weatherCode = forecast.getHigh(1); 
    chineseFlag = false;
    
  } else if (infoId == 8){
    chn1 = ming;
    chn2 = tian;
    chn3 = zui;
    chn4 = di;
    weatherCode = forecast.getLow(1); 
    chineseFlag = false;

  } else if (infoId == 9){
    chn1 = bi;
    chn2 = li;
    chn3 = fen;
    chn4 = si;
    weatherCode = biliFans; 
    chineseFlag = false;
    
  } else if (infoId == 10){
    chn1 = bo;
    chn2 = fang;
    chn3 = shu;
    chn4 = liang;
    weatherCode = biliViews; 
    chineseFlag = false;
    
  }
  
  showWeatherDetail(weatherCode, chineseFlag);
  showWeatherChnDesc(chn1, chn2, chn3, chn4);
}

//显示网络获取的具体天气信息
void showWeatherDetail(int weatherCode, bool chineseFlag){
   
  tft.setViewport(160, 220, HALF_BANNER_WIDTH, BANNER_HEIGHT);
  tft.fillScreen(BANNER_BG);
  // 设置信息显示位置
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(BANNER_FG, BANNER_BG);  //设置文字颜色和背景色
  tft.setTextPadding(10); //设置文字padding，覆盖旧字  
  
  if (chineseFlag == false){ // 如果不是中文   
    tft.drawString(": " + String(weatherCode) + "°", 0, 0, 4); 
  } else {                   // 如果是中文
    tft.drawString(": ", 0, 0, 4);  

    if (weatherCode >= 0 && weatherCode <= 3) {
      tft.drawBitmap(15, 0, qing, 20, 20, BANNER_FG);   
    } else if (weatherCode >= 4 && weatherCode <= 8) {
      tft.drawBitmap(15, 0, duo, 20, 20, BANNER_FG);   
      tft.drawBitmap(35, 0, yun, 20, 20, BANNER_FG);  
    } else if (weatherCode == 9) {
      tft.drawBitmap(15, 0, yin, 20, 20, BANNER_FG); 
    } else if (weatherCode >= 10 && weatherCode <= 12) {
      tft.drawBitmap(15, 0, zhen, 20, 20, BANNER_FG);   
      tft.drawBitmap(35, 0, yu, 20, 20, BANNER_FG);        
    } else if (weatherCode == 13) {
      tft.drawBitmap(15, 0, xiao, 20, 20, BANNER_FG);   
      tft.drawBitmap(35, 0, yu, 20, 20, BANNER_FG);        
    } else if (weatherCode == 14) {
      tft.drawBitmap(15, 0, zhong, 20, 20, BANNER_FG);   
      tft.drawBitmap(35, 0, yu, 20, 20, BANNER_FG);        
    } else if (weatherCode >= 15 && weatherCode <= 18) {
      tft.drawBitmap(15, 0, da, 20, 20, BANNER_FG);   
      tft.drawBitmap(35, 0, yu, 20, 20, BANNER_FG);        
    } else if (weatherCode == 19 || weatherCode == 20) {
      tft.drawBitmap(15, 0, dong, 20, 20, BANNER_FG);   
      tft.drawBitmap(35, 0, yu, 20, 20, BANNER_FG);        
    } else if (weatherCode == 21) {
      tft.drawBitmap(15, 0, zhen, 20, 20, BANNER_FG);   
      tft.drawBitmap(35, 0, xue, 20, 20, BANNER_FG);        
    } else if (weatherCode == 22) {
      tft.drawBitmap(15, 0, xiao, 20, 20, BANNER_FG);   
      tft.drawBitmap(35, 0, xue, 20, 20, BANNER_FG);        
    } else if (weatherCode == 23) {
      tft.drawBitmap(15, 0, zhong, 20, 20, BANNER_FG);   
      tft.drawBitmap(35, 0, xue, 20, 20, BANNER_FG);        
    } else if (weatherCode == 24 || weatherCode == 25 ) {
      tft.drawBitmap(15, 0, da, 20, 20, BANNER_FG);   
      tft.drawBitmap(35, 0, xue, 20, 20, BANNER_FG);        
    } else if (weatherCode >= 26 && weatherCode <= 29 ) {
      tft.drawBitmap(15, 0, sha, 20, 20, BANNER_FG);   
      tft.drawBitmap(35, 0, chen, 20, 20, BANNER_FG);        
    } else if (weatherCode == 30 || weatherCode == 31 ) {
      tft.drawBitmap(15, 0, wu, 20, 20, BANNER_FG);   
      tft.drawBitmap(35, 0, mai, 20, 20, BANNER_FG);        
    } else if (weatherCode >= 32 || weatherCode <= 36 ) {
      tft.drawBitmap(15, 0, da, 20, 20, BANNER_FG);   
      tft.drawBitmap(35, 0, feng, 20, 20, BANNER_FG);        
    } else {
      tft.drawString(": Unknown", 0, 0, 4);  
    }
  }
  
  tft.resetViewport();
}

void showWeatherChnDesc(const uint8_t chnChar1[], const uint8_t chnChar2[], const uint8_t chnChar3[], const uint8_t chnChar4[] ){
  // 设置信息显示位置
  tft.setViewport(0, 220, HALF_BANNER_WIDTH, BANNER_HEIGHT);
  tft.fillScreen(BANNER_BG);
  tft.setTextColor(BANNER_FG, BANNER_BG);  //设置文字颜色和背景色

  //显示中文说明
  tft.drawBitmap(80,0,chnChar1,20,20,BANNER_FG);
  tft.drawBitmap(100,0,chnChar2,20,20,BANNER_FG);
  tft.drawBitmap(120,0,chnChar3,20,20,BANNER_FG);
  tft.drawBitmap(140,0,chnChar4,20,20,BANNER_FG); 

  tft.resetViewport();   
}
//--- b站API相关函数结束 ---

//--- MQTT相关函数开始 ---
// 连接MQTT服务器并订阅信息
void connectMQTTserver(){
  // 根据ESP8266的MAC地址生成客户端ID（避免与其它ESP8266的客户端ID重名）
  String clientId = "esp8266-" + WiFi.macAddress();

  // 连接MQTT服务器
  if (mqttClient.connect(clientId.c_str())) { 
    Serial.println("MQTT Server Connected.");
    Serial.println("Server Address:");
    Serial.println(mqttServer);
    Serial.println("ClientId: ");
    Serial.println(clientId);
    subscribeTopic(); // 订阅指定主题
  } else {
    Serial.print("MQTT Server Connect Failed. Client State:");
    Serial.println(mqttClient.state());
    delay(5000);
  }   
}

// 订阅指定主题
void subscribeTopic(){
  String topicString = controlTopic;
  char subTopic[topicString.length() + 1];  
  strcpy(subTopic, topicString.c_str());
  
  // 建立订阅主题2
  String topicString2 = chatTopic;
  char subTopic2[topicString2.length() + 1];  
  strcpy(subTopic2, topicString2.c_str());
  
  // 通过串口监视器输出是否成功订阅主题1以及订阅的主题1名称
  if(mqttClient.subscribe(subTopic)){
    Serial.println("Subscribe Control Topic");
    Serial.println(subTopic);
    Serial.println("OK");
  } else {
    Serial.print("Subscribe Fail...");
  }  

  // 通过串口监视器输出是否成功订阅主题2以及订阅的主题2名称
  if(mqttClient.subscribe(subTopic2)){
    Serial.println("Subscribe Chat Topic ");
    Serial.println(subTopic2);
    Serial.println("OK");
  } else {
    Serial.print("Subscribe Fail...");
  }    
}

// 收到信息后的回调函数
void receiveCallback(char* topic, byte* payload, unsigned int length) {

  int cmdId;    // 系统控制指令id
  char cmdInfo; // 系统控制信息具体内容
    
  Serial.print("Message Received [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println("");
  Serial.print("Message Length(Bytes) ");
  Serial.println(length);

  if(String(topic) == controlTopic){
    userControlString = "";
    for (int i = 0; i < length; i++) {
      userControlString += (char)payload[i];
    }
    Serial.print("userControlString = ");Serial.println(userControlString); 

    if(payload[0] == 'b' && payload[1] == 'j' ){
      cmdId = 1;            // 修改背景显示间隔
      cmdInfo = payload[2];
      
    } else  if(payload[0] == 's' && payload[1] == 'j' ){
      cmdId = 2;            // 修改时间更新间隔
      cmdInfo = payload[2];
      
    } else  if(payload[0] == 'b' && payload[1] == 'l' ){
      cmdId = 3;            // 修改bili更新间隔
      cmdInfo = payload[2];
      
    } else  if(payload[0] == 't' && payload[1] == 'q' ){
      cmdId = 4;            // 修改bili更新间隔
      cmdInfo = payload[2];
      
    } else  if(payload[0] == 'x' && payload[1] == 's' ){
      cmdId = 5;            // 修改信息显示时间间隔
      cmdInfo = payload[2];
      
    } else  if(payload[0] == 'h' && payload[1] == 'b' ){
      cmdId = 6;            // 修改信息显示时间间隔
      cmdInfo = payload[2];
      
    } else {
      return;
    }

    changeSettings(cmdId, cmdInfo);
    
  } else if(String(topic) == chatTopic){
    userChatString = "";
    for (int i = 0; i < length; i++) {
      userChatString += (char)payload[i];
    }
    Serial.print("userChatString = ");Serial.println(userChatString); 
  
    showChatMessage(userChatString);  // 屏幕上显示用户聊天消息
    pubReportMsg(userChatString);  // 通过系统报告主题发布用户聊天信息
  } 
}

// 发布信息
void pubReportMsg(String reportMsg){

  String topicString = reportTopic;
  char publishTopic[topicString.length() + 1];  
  strcpy(publishTopic, topicString.c_str());
 
  char publishMsg[reportMsg.length() + 1];   
  strcpy(publishMsg, reportMsg.c_str());
  
  // 实现ESP8266向主题发布信息
  if(mqttClient.publish(publishTopic, publishMsg)){
    Serial.println("Publish Report Topic:");Serial.println(publishTopic);
    Serial.println("Publish Report Message:");Serial.println(publishMsg);    
  } else {
    Serial.println("Report Message Publish Failed."); 
  }
}

//--- MQTT相关函数结束 ---

//--- 显示用户聊天信息相关函数开始 ---
void showChatMessage(String userChatString){
  if (userChatString == NULL){
    userChatFlag = false;  // 告知系统目前没有用户聊天信息
    displayBg(bgId);  
  } else {
    userChatFlag = true;  // 告知系统目前有用户聊天信息
    if(userChatString.length() > 24){
      userChatString = "[MESSAGE TOO LONG!]";
      
    } else {
      displayUserMessage(userChatString);
    }
  }
}

void displayUserMessage(String userString){
    tft.setViewport(0, 200, BANNER_WIDTH, BANNER_HEIGHT);
    tft.fillScreen(BANNER_BG);
    // 设置信息显示位置
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(BANNER_FG, BANNER_BG);  //设置文字颜色和背景色
    tft.setTextPadding(10); //设置文字padding，覆盖旧字  
    
    tft.drawString(userString, 160, 0, 4); 
    
    tft.resetViewport();  

}

//--- 显示用户聊天信息相关函数结束 ---

//--- 设置系统相关函数开始 ---
void changeSettings(int cmdId, char cmdInfo){
  String userSettingMessage;
  Serial.print("cmdId = ");Serial.println(cmdId);
  Serial.print("cmdInfo = ");Serial.println(cmdInfo);

  if(cmdId == 1){       // 收到修改背景变化间隔调整的指令 
    if(cmdInfo == '1'){
      bgChgInterval = 5;
    } else if(cmdInfo == '2'){
      bgChgInterval = 10;
    } else if(cmdInfo == '3'){
      bgChgInterval = 30;
    } else if(cmdInfo == '4'){
      bgChgInterval = 60;
    } else if(cmdInfo == '5'){
      bgChgInterval = 300;
    } else {
      return;
    }
    
    userSettingMessage = "bgChgInterval = " + String(bgChgInterval); 
     
  } else if(cmdId == 2){       // 收到修改更新间隔调整的指令 
    if(cmdInfo == '1'){
      timeCheckInterval = 300;
    } else if(cmdInfo == '2'){
      timeCheckInterval = 600;
    } else if(cmdInfo == '3'){
      timeCheckInterval = 900;
    } else if(cmdInfo == '4'){
      timeCheckInterval = 1200;
    } else if(cmdInfo == '5'){
      timeCheckInterval = 1800;
    } else {
      return;
    }

    userSettingMessage = "timeCheckInterval = " + String(timeCheckInterval);
    
  } else if(cmdId == 3){  // 收到修改哔哩哔哩信息更新时间设置
    if(cmdInfo == '1'){
      biliUpdateInterval = 350;
    } else if(cmdInfo == '2'){
      biliUpdateInterval = 650;
    } else if(cmdInfo == '3'){
      biliUpdateInterval = 950;
    } else if(cmdInfo == '4'){
      biliUpdateInterval = 1250;
    } else if(cmdInfo == '5'){
      biliUpdateInterval = 1850;
    } else {
      return;
    }

    userSettingMessage = "biliUpdateInterval = " + String(biliUpdateInterval);      
    
  } else if(cmdId == 4){  // 收到修改天气信息更新时间设置
    if(cmdInfo == '1'){
      weatherUpdateInterval = 300;
    } else if(cmdInfo == '2'){
      weatherUpdateInterval = 600;
    } else if(cmdInfo == '3'){
      weatherUpdateInterval = 900;
    } else if(cmdInfo == '4'){
      weatherUpdateInterval = 1200;
    } else if(cmdInfo == '5'){
      weatherUpdateInterval = 1800;
    } else {
      return;
    }

    userSettingMessage = "weatherUpdateInterval = " + String(weatherUpdateInterval);      
  } else if(cmdId == 5){  // 收到修改天气信息显示时间设置
    if(cmdInfo == '1'){
      weatherDisplayInterval = 2;
    } else if(cmdInfo == '2'){
      weatherDisplayInterval = 5;
    } else if(cmdInfo == '3'){
      weatherDisplayInterval = 10;
    } else if(cmdInfo == '4'){
      weatherDisplayInterval = 15;
    } else if(cmdInfo == '5'){
      weatherDisplayInterval = 20;
    } else if (cmdId == 6) {
      
    } else {
      return;
    }

    userSettingMessage = "weatherDisplayInterval = " + String(weatherDisplayInterval);      
  } else if (cmdId == 6){  // 收到要求系统汇报状态的指令
    String systemReportString;
    systemReportString = "bj:" + String(bgChgInterval) + ", ";
    systemReportString = systemReportString + "sj:" + String(timeCheckInterval) + ", ";
    systemReportString = systemReportString + "bl:" + String(biliUpdateInterval) + ", ";
    systemReportString = systemReportString + "tq:" + String(weatherUpdateInterval) + ", ";    
    systemReportString = systemReportString + "xs:" + String(weatherDisplayInterval); 
    
    pubReportMsg(systemReportString);
    return;
  } else {
    return;
  }

  displayUserMessage(userSettingMessage);
  pubReportMsg(userSettingMessage);
}

/*
MQTT控制指令说明
bj - 修改背景变换时间间隔
1 - 5秒 / 2 -10秒 / 3 - 30秒 / 4 - 60秒（默认） / 5 - 300秒
sj - 修改系统NTP对时时间间隔
1 - 300秒（默认） / 2 -600秒 / 3 - 900秒 / 4 - 1200秒 / 5 - 1800秒
bl - 修改哔哩哔哩信息更新时间间隔
1 - 350秒（默认） / 2 -650秒 / 3 - 950秒 / 4 - 1250秒 / 5 - 1850秒
tq - 修改天气信息更新时间间隔
1 - 300秒 / 2 -600秒 / 3 - 900秒（默认） / 4 - 1200秒 / 5 - 1800秒
xs - 修改屏幕下方信息显示变换时间间隔
1 - 2秒 / 2 -5秒 / 3 - 10秒 / 4 - 15秒 / 5 - 20秒（默认）
hb - 指示系统汇报运行状态
*/
