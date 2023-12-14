const char* ssid1              = "ssid1";                        // Edit me
const char* ssid2              = "ssid2";                        // Edit me
const char* password           = "wifi_password";                // Edit me
const char* ntpServer1         = "ntp3.vniiftri.ru";
const char* ntpServer2         = "time.nist.gov";
const long  gmtOffset_sec      = 10800;
const int   daylightOffset_sec = 0;

#define BOT_TOKEN "your:bot_token"                               // Edit me
#define CHAT_ID "yourchatid"                                     // Edit me

#define HOT_ID  1111111                                              // Edit me
#define COL_ID  2222222                                              // Edit me

FastBot bot(BOT_TOKEN);

Mosobleirc eirc( "eirc_login", "eirc_password", HOT_ID, COL_ID, bot);  // Edit me
