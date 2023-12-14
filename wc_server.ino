//
//

#include <WiFi.h>
#include <WiFiMulti.h>
#include <ArduinoJson.h>
#include "time.h"
#include "sntp.h"
#include "esp_log.h"
#include "FastBot.h"

#include "mosobleirc.h"
#include "saver.h"

#define HOT_PIN 23
#define COL_PIN 18

#include "var_git.h"

WiFiMulti WiFiMulti;

Saver pref( bot );

struct Value {
    const uint8_t PIN;
    const uint32_t id;
    uint32_t val;
    uint32_t last_trigered;
    bool trig;
    uint32_t max_low;
    uint32_t max_hig;
    uint32_t cont_low;
    uint32_t cont_hig;
    bool state;
};

//            PIN  ID       Value
Value hot_t = {HOT_PIN, HOT_ID, 0, 0, false, 0, 0, 0, 0, false };
Value col_t = {COL_PIN, COL_ID, 0, 0, false, 0, 0, 0, 0, false };

uint32_t lastSent = 0;
uint32_t cur_time = 0;

struct tm curt;  //variable for getting current time 

hw_timer_t *pull_timer = NULL;

portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

void IRAM_ATTR pullTimer(){
    portENTER_CRITICAL_ISR(&mux);

    int h_val = digitalRead(hot_t.PIN);

    if ( h_val == 1 && hot_t.state == 1 ) hot_t.cont_hig += 1;
    if ( hot_t.cont_hig > hot_t.max_hig ) hot_t.max_hig = hot_t.cont_hig;

    if (h_val == 0 && hot_t.state == 0) hot_t.cont_low += 1;
    if ( hot_t.cont_low > hot_t.max_low ) hot_t.max_low = hot_t.cont_low;

    if (h_val == 0 && hot_t.state == 1) {
      hot_t.cont_low == 0;
      hot_t.cont_hig == 0;
    }
  
    if (h_val == 1 && hot_t.state == 0) {
      hot_t.cont_low == 0;
      hot_t.cont_hig == 0;
      if ( hot_t.max_low > 10 && hot_t.max_hig > 10 ) {
        hot_t.val += 1;
        hot_t.trig = true;
        hot_t.max_low = 0;
        hot_t.max_hig = 0;
      }
    }

    hot_t.state = h_val;
    
    int c_val = digitalRead(col_t.PIN);
    
    if ( c_val == 1 && col_t.state == 1 ) col_t.cont_hig += 1;
    if ( col_t.cont_hig > col_t.max_hig ) col_t.max_hig = col_t.cont_hig;

    if (c_val == 0 && col_t.state == 0) col_t.cont_low += 1;
    if ( col_t.cont_low > col_t.max_low ) col_t.max_low = col_t.cont_low;

    if (c_val == 0 && col_t.state == 1) {
      col_t.cont_low == 0;
      col_t.cont_hig == 0;
    }
  
    if (c_val == 1 && col_t.state == 0) {
      col_t.cont_low == 0;
      col_t.cont_hig == 0;
      if ( col_t.max_low > 10 && col_t.max_hig > 10 ) {
        col_t.val += 1;
        col_t.trig = true;
        col_t.max_low = 0;
        col_t.max_hig = 0;
      }
    }

    col_t.state = c_val;

    portEXIT_CRITICAL_ISR(&mux);
}

// Callback function (get's called when time adjusts via NTP)
void timeavailable(struct timeval *t) {
  struct tm timeinfo;
  char strftime_buf[64];

  log_i("Got time adjustment from NTP!");
  if(!getLocalTime(&timeinfo)){
    log_e("No time available (yet)");
    return;
  }
  strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
  log_i( "Time : %s", strftime_buf );
}

void sendValuesToTelegram() {
  String page = "Данные ЕИРЦ:";
  int measuresHasFound = 0;
  
  for(JsonPair m : eirc.measures.as<JsonObject>()) {
    String key = m.key().c_str();
    uint32_t key_t = atoi( key.c_str() );
    if ( key_t == hot_t.id || key_t == col_t.id ) {
      measuresHasFound += 1;
      page += String("\n*******\n id:");
      page += String(eirc.measures[key]["id"].as<const char*>());
      page += String(" ");
      page += String(eirc.measures[key]["item"].as<const char*>());
      page += String("\n  ");
      page += String(eirc.measures[key]["type"].as<const char*>());
      page += String(" : ");
      page += String(eirc.measures[key]["value"].as<unsigned int>());
      page += String("\n  Поверка : ");
      page += String(eirc.measures[key]["attorney"].as<const char*>());
    }
  }
  if (measuresHasFound < 2) {
    page += String("\n \xe2\x80\xbc ВНИМАНИЕ \xe2\x80\xbc не все счетчики найдены в профиле\n");
  }

  page += String("\n---------------\nТекущие показания:");
  page += String("\n Горячая : ");
  page += String( hot_t.val );
  page += String("\n Холодная: ");
  page += String( col_t.val );
  page += String("\n Показания переданы: ");
  page += String( lastSent-10000 ); //minus 10000 because started from 1900
  page += String("\n\n");

  bot.sendMessage( page );
}

int postValuesToEIRC() {
  // возврящает :
  // 1 - если показания переданы
  // 0 - если показания не переданы
  
  String page;

  int32_t month_to_att = eirc.attorney_month - (curt.tm_year * 12 + curt.tm_mon);
  page = "\xf0\x9f\x94\xb4 Поверка прострочена \xf0\x9f\x94\xb4";
  if ( month_to_att < 0 ) bot.sendMessage( page );
  else if ( month_to_att < 4 ) 
    bot.sendMessage( String("\xf0\x9f\x94\xb4Месяцев до поверки : ") + String(month_to_att) );

  page = "\xf0\x9f\x94\xb4 Данные не отправлены\nДанные принимают с ";
  if (curt.tm_mday<eirc.date_from || curt.tm_mday>eirc.date_to) {
    page += String( int( eirc.date_from ) );
    page += String( " по " );
    page += String( int( eirc.date_to ) );
    page += String( " число.");
    bot.sendMessage( page );
    return 0;
  }

  if ( eirc.authorize() ) {
    uint32_t h_res = eirc.post( hot_t.id, int(hot_t.val / 100) );
    uint32_t c_res = eirc.post( col_t.id, int(col_t.val / 100) );
    if ( h_res==1 && c_res==1 ) {
      if(getLocalTime(&curt)){
        lastSent = curt.tm_year * 100 + curt.tm_mon; 
        pref.set_date(lastSent);    
      }
      page = "\xf0\x9f\x9f\xa2 Данные успешно отправлены в ЕИРЦ\nгор : ";
      page += String( int(hot_t.val/100) );
      page += String( "\nхол : ");
      page += String( int(col_t.val/100) );
      bot.sendMessage( page );
      return 1;
    }
    if ( h_res==2 || c_res==2 ) {
      bot.sendMessage( "\xf0\x9f\x94\xb4 Превышен лимит \nПодайте показания самостоятельно или выставите текущие показания командой set\n" );
    }
  }
  bot.sendMessage( "\xf0\x9f\x94\xb4 показания не переданы " );
  log_i("curt.tm_mday : %d , %d - %d ", curt.tm_mday, eirc.date_from, eirc.date_to );
  return 0; 
}

void correctValues( char* st ) {
  uint32_t v[] = {0,0,0};
  uint32_t u = 0;
  char* ptr = strtok( st, " .,-" );
  while( u<3 && ptr ){
    uint32_t tmp = atoi( ptr );
    if ( tmp>0 ) {
      v[u++] = tmp;
    }
    ptr = strtok( (char*)0, " .,-" );
  }
  saveValues( v[0], v[1], v[2] );
  bot.sendMessage( "\xf0\x9f\x9f\xa2 текущие показания запомнил" );
}

void newMsg(FB_msg& msg) {

  log_i( "MSG username: %s chatID : %s text : %s", msg.username, msg.chatID, msg.text );
  
  String ms = msg.text;
  ms.toLowerCase();
  if ( ms.indexOf("get")>-1 ) {
    eirc.get();
    sendValuesToTelegram();
  } else if ( ms.indexOf("set")>-1 ) {
    correctValues( strdup(ms.c_str()) );
  } else if ( ms.indexOf("push")>-1 ) {
    postValuesToEIRC();
  } else if ( ms.indexOf("val")>-1 ) {
    sendValuesToTelegram();
  } else {
    String page = String("get - запросить данные ЕИРЦ\n");
    page += String("val - как get но не запрашивать ЕИРЦ\n");
    page += String("push - отправить показания в ЕИРЦ\n");
    page += String("set <hot> <col>[<data>] - утановить текущие значения.\n");
    page += String("  hot и col указываются в десятках литров (до двух знаков после запятой) но запятая не указывается.\n");    
    page += String("  если прибор показывает 000100,120 то нужно указать 10012\n");
    page += String("  data - 4 цифры, год и месяц. Например: 2304\n");
    bot.sendMessage( page );
  }
}

void saveValues( uint32_t h, uint32_t c, uint32_t d ) {
  // сохраняем данные во flash чтобы пережили reboot
  hot_t.val = h;
  col_t.val = c;
  pref.set_hot( h );
  pref.set_col( c );
  if ( d>0 ) {
    lastSent = d;
    pref.set_date( d );
  }
}

void setup(void) {

  esp_log_level_set( "*", ESP_LOG_DEBUG );

  Serial.begin(115200);

  // init pins
  pinMode(hot_t.PIN, INPUT_PULLUP);
  pinMode(col_t.PIN, INPUT_PULLUP);

  // init timer reading and debounce pins
  pull_timer = timerBegin(0, 80, true);
  timerAttachInterrupt(pull_timer, &pullTimer, true);
  timerAlarmWrite(pull_timer, 100000, true);
  timerAlarmEnable(pull_timer);

  // init wifi
  connectWiFi();

  // bot settings
  bot.setChatID(CHAT_ID);
  bot.attach(newMsg);
  bot.sendMessage( "Started" );

  // init ntp callback
  sntp_set_time_sync_notification_cb( timeavailable );
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer1, ntpServer2);

  eirc.get();
  log_i( "Got data from eirc. From : %d  To : %d", eirc.date_from, eirc.date_to );

  pref.get();
  hot_t.val = pref.hot;
  col_t.val = pref.col;
  lastSent = pref.date;
  log_i( "Got pref. Col: %d, Hot: %d, date: %d", pref.col, pref.hot, pref.date );
}

uint64_t lastTry = 0;

void loop(void) {
    delay(1000);//allow the cpu to switch to other tasks

    if (hot_t.trig) {
        Serial.printf("Save hot_t: %u \n", hot_t.val);
        pref.set_hot( hot_t.val );
        hot_t.trig = false;
    }

    if (col_t.trig) {
        Serial.printf("Save col_t: %u \n", col_t.val);
        pref.set_col( hot_t.val );
        col_t.trig = false;
    }

    bot.tick();

    if(getLocalTime(&curt)){
      uint32_t cur_mon;
      uint64_t cur_hour;

      cur_mon = curt.tm_year * 100 + curt.tm_mon;
      cur_hour = curt.tm_year * 1000000 + curt.tm_mon * 10000 + curt.tm_mday * 100 + curt.tm_hour;

      if ( eirc.date_from > 0 and            \
           curt.tm_mday > eirc.date_from and \
           curt.tm_mday < eirc.date_to and   \
           cur_mon > lastSent  and           \
           curt.tm_hour == 17 and            \
           cur_hour > lastTry ) {

        bot.sendMessage( "Пора передать показания. \nкоманда push");
        if ( !postValuesToEIRC() ) {
          lastTry = cur_hour;
        }

      } 
    }  
}

void connectWiFi() {
  delay(2000);
  Serial.println();

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect( true );

  WiFiMulti.addAP(ssid1, password);
  WiFiMulti.addAP(ssid2, password);
  
  while (WiFiMulti.run() != WL_CONNECTED) {
    delay(500);
    if (millis() > 15000) ESP.restart();
  }
  
  log_i("Connected to SSID: %s with an IP address: %s ", WiFi.SSID(), WiFi.localIP().toString().c_str() );
  //WiFi.printDiag(Serial);
}
