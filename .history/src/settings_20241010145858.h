// 用户信息
String reqUserKey = "S0zgSchuAgrndPkv4";      // 心知天气私钥
String reqLocation = "wuhan";      // 心知天气城市
String reqUnit = "c";               // 心知天气摄氏/华氏
const char* mqttServer = "test.ranye-iot.net";        // mqtt服务端网址
static const char ntpServerName[] = "ntp.aliyun.com"; // ntp时间服务器地址
const int timeZone = 8;                               // 设置时区（8为北京时间）
String biliUserId = "103589285";           // 哔哩用户id
String biliBV = "BV1L7411c7jw";            // 哔哩视频BV号

// 运行控制变量
int bgChgInterval = 60;            // 背景变换时间间隔
int timeCheckInterval = 300;       // 时间更新时间间隔
int biliUpdateInterval = 650;      // 哔哩信息更新时间间隔
int weatherUpdateInterval = 900;   // 天气信息更新时间间隔
int weatherDisplayInterval = 5;    // 天气信息显示时间间隔

const int START_BG_NUM = 1;               // 起始图片编号
const int END_BG_NUM = 50;                // 终止图片编号
const int MAX_INFO_ID = 10;               // 屏幕下方显示信息数量
