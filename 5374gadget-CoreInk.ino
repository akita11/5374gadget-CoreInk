// for M5StackCoreInk (by akita11)

#include "M5CoreInk.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include "icon.h"
#include "esp_adc_cal.h"

// https://gist.github.com/tanakamasayuki/913b1b4c29b4f21c1018b05682566d0a
// https://lang-ship.com/tools/image2data/
#define JST     3600* 9

Ink_Sprite InkPageSprite(&M5.M5Ink);
RTC_TimeTypeDef RTCtime;
RTC_DateTypeDef RTCDate;

// ★★★★★設定項目★★★★★★★★★★
const char* ssid     = "xxxxxxxx";       // 自宅のWiFi設定
const char* password = "xxxxxxxx";

// 以下のURLにあるエリア番号を入れる
//https://github.com/PhalanXware/scraped-5374/blob/master/save.json
int area_number = 30;    // 地区の番号（例：浅野 0, 浅野川 1）

int hour_start = 6;   // 通知を開始する時刻
int hour_end   = 9;   // 通知を終了する時刻

// ★★★★★★★★★★★★★★★★★★★

int today;
int hour_wake;        // 次に起動する時刻

#define DISP_NOTGARBAGE 0
#define DISP_BURNABLE   1
#define DISP_NONBURNABLE 2
#define DISP_RECYCLABLE 3
#define DISP_BOTTLE     4

void setDisp(uint32_t color)
{
  char buf[30];
  unsigned char imgBuff[5000];

  M5.M5Ink.clear();

  M5.rtc.GetData(&RTCDate);
  M5.rtc.GetTime(&RTCtime);

  //  InkPageSprite.clear();
  if (color == DISP_BURNABLE) memcpy(imgBuff, img_BURNABLE, sizeof(imgBuff));
  else if (color == DISP_NONBURNABLE) memcpy(imgBuff, img_NONBURNABLE, sizeof(imgBuff));
  else if (color == DISP_RECYCLABLE) memcpy(imgBuff, img_RECYCLABLE, sizeof(imgBuff));
  else if (color == DISP_BOTTLE) memcpy(imgBuff, img_BOTTLE, sizeof(imgBuff));
  else memset(imgBuff, 0xff, 5000);

  InkPageSprite.clear( CLEAR_DRAWBUFF | CLEAR_LASTBUFF );
  InkPageSprite.drawBuff(0, 0, 200, 200, imgBuff);

  //  sprintf(buf, "%04d/%02d/%02d", RTCDate.Year, RTCDate.Month, RTCDate.Date);
  sprintf(buf, "%02d/%02d", RTCDate.Month, RTCDate.Date);
  InkPageSprite.drawString(0, 182, buf);

  //  sprintf(buf, "%02d:%02d:%02d", RTCtime.Hours, RTCtime.Minutes, RTCtime.Seconds);
  //  InkPageSprite.drawString(100, 180, buf);

  sprintf(buf, "%d", hour_wake); InkPageSprite.drawString(0, 0, buf);

  analogSetPinAttenuation(35, ADC_11db);
  esp_adc_cal_characteristics_t *adc_chars = (esp_adc_cal_characteristics_t *)calloc(1, sizeof(esp_adc_cal_characteristics_t));
  esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 3600, adc_chars);
  uint16_t ADCValue = analogRead(35);
  uint32_t BatVolmV  = esp_adc_cal_raw_to_voltage(ADCValue, adc_chars);
  float BatVol = float(BatVolmV) * 25.1 / 5.1 / 1000;
  sprintf(buf, "%.1fV", BatVol); InkPageSprite.drawString(160, 182, buf);

  InkPageSprite.pushSprite();
  delay(1000);
}

// the setup routine runs once when M5Stack starts up
void setup() {
  M5.begin();
  // シリアル設定
  Serial.begin(115200);
  Serial.println("");
  if ( !M5.M5Ink.isInit()) {
    Serial.printf("Ink Init faild");
  }
//  M5.M5Ink.clear(INK_CLEAR_MODE1); // clear MODE1 at initial
  InkPageSprite.creatSprite(0, 0, 200, 200, true);

  M5.update();
  // if MID button pressed at boot, NTP adjust
  if ( M5.BtnMID.isPressed()) {
    wifiConnect();
    M5.M5Ink.clear(INK_CLEAR_MODE1); // clear MODE1 at initial
    InkPageSprite.clear();
    InkPageSprite.drawString(35, 50, "NTP adjust");
    InkPageSprite.pushSprite();
    delay(1000);
    // NTP同期
    configTime( JST, 0, "ntp.nict.jp", "ntp.jst.mfeed.ad.jp");
    delay(1000);
    time_t t;
    struct tm *tm;
    t = time(NULL);
    tm = localtime(&t);
    RTCtime.Hours = tm->tm_hour; RTCtime.Minutes = tm->tm_min; RTCtime.Seconds = tm->tm_sec;
    M5.rtc.SetTime(&RTCtime);
    RTCDate.Year = tm->tm_year + 1900; RTCDate.Month = tm->tm_mon + 1; RTCDate.Date = tm->tm_mday;
    M5.rtc.SetData(&RTCDate);
    delay(1000);
    InkPageSprite.clear();
    wifiDisconnect();
  }
  if ( M5.BtnUP.isPressed()) {
    // demo mode
    M5.M5Ink.clear(INK_CLEAR_MODE1); // clear MODE1 at initial
    while (1) {
      setDisp(DISP_BURNABLE); delay(5000);
      setDisp(DISP_NONBURNABLE); delay(5000);
      setDisp(DISP_RECYCLABLE); delay(5000);
      setDisp(DISP_BOTTLE); delay(5000);
    }
  }

  M5.rtc.GetTime(&RTCtime);
  M5.rtc.GetData(&RTCDate);
  int h, m;
  h = RTCtime.Hours; m = RTCtime.Minutes;
  today = DISP_NOTGARBAGE;
  if (h == hour_start) {
    // WiFi接続
    wifiConnect();
    delay(1000);
    // 今日のデータの読み出し
    today = DISP_NOTGARBAGE;
    updateGarbageDay();
    wifiDisconnect();
    hour_wake = hour_end;
  }
  else{
    today = DISP_NOTGARBAGE;
    hour_wake = hour_start;
  }
  M5.M5Ink.clear(INK_CLEAR_MODE1); // clear MODE1 at initial
//  hour_wake = (h + 1) % 24; today = (h % 4) + 1; // for debug
  setDisp(today);
  M5.shutdown(RTC_TimeTypeDef(hour_wake, 0, 0));
}

void loop() {
}

void wifiDisconnect() {
  Serial.println("Disconnecting WiFi...");
  WiFi.disconnect(true); // disconnect & WiFi power off
}

void wifiConnect() {
  Serial.print("Connecting to " + String(ssid));

  //WiFi接続開始
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  //接続を試みる(30秒)
  for (int i = 0; i < 60; i++) {
    if (WiFi.status() == WL_CONNECTED) {
      //接続に成功。IPアドレスを表示
      Serial.println();
      Serial.print("Connected! IP address: ");
      Serial.println(WiFi.localIP());
      break;
    } else {
      Serial.print(".");
      delay(500);
    }
  }

  // WiFiに接続出来ていない場合
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("");
    Serial.println("Failed, Wifi connecting error");
  }

}

// ゴミの日の情報のアップデート
void updateGarbageDay(void) {
  // ゴミ情報の読み出し
  HTTPClient https;
  // "area"とJSONファイルのNo.のずれはここで吸収する
  String url = "https://raw.githubusercontent.com/PhalanXware/scraped-5374/master/save_" + String(area_number + 1) + ".json";
  Serial.print("connect url :");
  Serial.println(url);

  Serial.print("[HTTPS] begin...\n");
  if (https.begin(url)) {  // HTTPS
    //if (https.begin(*client, url)) {  // HTTPS

    Serial.print("[HTTPS] GET...\n");
    // start connection and send HTTP header
    int httpCode = https.GET();

    // httpCode will be negative on error
    if (httpCode > 0) {
      // HTTP header has been send and Server response header has been handled
      Serial.printf("[HTTPS] GET... code: %d\n", httpCode);
      //Serial.println(https.getSize());

      // file found at server
      String payload;
      if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
        payload = https.getString();
        Serial.println("HTTP_CODE_OK");
        //Serial.println(payload);
      }

      String html[10] = {"\0"};
      int index = split(payload, '\n', html);
      String garbageDays = {"\0"};
      garbageDays = html[5];
      Serial.println(garbageDays);

      if (garbageDays.indexOf("今日") > 0) {
        if (garbageDays.indexOf("燃やすごみ") > 0) {
          today = DISP_BURNABLE;
        } else if (garbageDays.indexOf("燃やさないごみ") > 0) {
          today = DISP_NONBURNABLE;
        } else if (garbageDays.indexOf("資源") > 0) {
          today = DISP_RECYCLABLE;
        } else if (garbageDays.indexOf("あきびん") > 0) {
          today = DISP_BOTTLE;
        } else {
          today = DISP_NOTGARBAGE;
        }
      }
      else {
        today = DISP_NOTGARBAGE;
      }
      Serial.print("今日は、");
      Serial.println(today);

    } else {
      Serial.printf("[HTTPS] GET... failed, error: %s\n", https.errorToString(httpCode).c_str());
    }
    https.end();
  } else {
    Serial.printf("[HTTPS] Unable to connect\n");
  }
}

// 文字列の分割処理
int split(String data, char delimiter, String *dst) {
  int index = 0;
  int arraySize = (sizeof(data) / sizeof((data)[0]));
  int datalength = data.length();
  for (int i = 0; i < datalength; i++) {
    char tmp = data.charAt(i);
    if ( tmp == delimiter ) {
      index++;
      if ( index > (arraySize - 1)) return -1;
    }
    else dst[index] += tmp;
  }
  return (index + 1);
}
