#ifndef _mosobleirc_h
#define _mosobleirc_h

//#define LOG_LOCAL_LEVEL ESP_LOG_ERROR
//#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE

#include <Arduino.h>
#include <StreamString.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <time.h>
#include "esp_log.h"
#include <FastBot.h>

uint32_t MONTH_LIMIT = 30;  //no more then 30m2 per month

class Mosobleirc {
  public:
    String     user;
    String     pass;
    String     token;
    uint32_t   hot_id;
    uint32_t   col_id;
    uint32_t   date_from;
    uint32_t   date_to;  
    int        status;
    time_t     lastLogin;
    uint32_t   attorney_month;
    FastBot*   bot;

    StaticJsonDocument<1024> measures;
    
    Mosobleirc( String _user, String _pass, uint32_t _hot_id, uint32_t _col_id, FastBot& _bot ) {
        user   = _user;
        pass   = _pass;
        token  = "";
        hot_id = _hot_id;
        col_id = _col_id;
        date_from = 0;
        date_to   = 32;
        status = 0;
        lastLogin = 0;
        attorney_month = 0xffffffff;
        bot = &_bot;
    }

    void pars_attorney( char* att ) {
      uint32_t year = 0;
      uint32_t month = 0;
      uint32_t v[] = {0,0,0};
      uint32_t u = 0;
      char* ptr = strtok( att, " \\/.-" );
      while( u<3 && ptr ){
        uint32_t tmp = atoi( ptr );
        if ( tmp>0 ) {
          v[u++] = tmp;
        }
        ptr = strtok( (char*)0, " \\/.-" );
      }
      if (v[0]>2000) year = v[0] - 1900;
      if (v[2]>2000) year = v[2] - 1900;
      month = year * 12 + ( v[1] - 1 );
      if (month < this->attorney_month) this->attorney_month = month;
    }

    int checkForErrors( int statusCode ) {
      log_i( "Status code : %d", statusCode );
      if (statusCode > 0 && statusCode == 200) {
          this->status = 1;
          return 0;
      } else {
          this->status = 0;
          return 1;
      }
    }

    int authorize() {
      // возврящает :
      // 1 - если авторизация пройдена
      // 0 - если авторизация не пройдена

      String json;
      int statusCode;

      time_t now;
      now = time( &now );

      char heep_size[64];
      sprintf( heep_size, "Heep size: %d", ESP.getHeapSize() );
      log_i( "%s",heep_size );
      this->bot->sendMessage(heep_size);

      log_i( "Unixtime: %d, diff : %d", now, now - this->lastLogin);
      
      if ( this->lastLogin > 0 && (now - this->lastLogin) < 60 ) {
        log_i( "Reusing token" );
        return 1;
      }

      WiFiClientSecure client;
      client.setInsecure();
      HTTPClient http;

      http.begin(client, "https://lkk.mosobleirc.ru/api/tenants-registration/v2/login");
      http.addHeader("Content-Type", "application/json");

      StaticJsonDocument<512> req;
      req["phone"] = this->user;
      req["password"] = this->pass;
      req["loginMethod"] = "PERSONAL_OFFICE";
      serializeJson(req, json);
      log_i("req memory usage %d", req.memoryUsage() );

      // getting the access token
      statusCode = http.POST( json );
      http.end();
      if ( this->checkForErrors(statusCode) )  {
        client.stop();
        return 0;
      };

      StaticJsonDocument<512> rsp1;
      DeserializationError err1 = deserializeJson(rsp1, http.getString());
      if (err1) {
        log_e( "deserializeJson() failed: %c", err1.c_str() );
        this->bot->sendMessage(err1.c_str());
      }
      log_i("rsp1 memory usage %d", rsp1.memoryUsage() );

      this->token = String( rsp1["token"].as<const char*>() );
      log_i( "Token: %s",this->token.c_str() );
      
      this->lastLogin = now;

      client.stop();
      return 1;
    }

    int post( uint32_t measureId, uint32_t newVal ) {
      // возврящает :
      // 2 - превышен лимит
      // 1 - если показания переданы
      // 0 - если показания не переданы

      String json;
      int statusCode;

      get(); // for renew data for diff

      WiFiClientSecure client;
      client.setInsecure();
      HTTPClient http;

      int32_t diff = (newVal - this->measures[String(measureId)]["value"].as<unsigned int>());

      log_i( "POST Measure: %d old : %d new : %d dif : %d", 
              measureId, 
              this->measures[String(measureId)]["value"].as<unsigned int>(),
              newVal,
              diff );


      if ( diff > MONTH_LIMIT ) return 2;  // превышен лимит или отрицательный diff
      if ( diff < 1 ) {                    // подать прежние показания
        newVal = this->measures[String(measureId)]["value"].as<unsigned int>();   
      }

      String _url = "https://lkk.mosobleirc.ru/api/api/clients/meters/";
      _url += String(measureId);
      _url += String("/values?withOptionalCheck=true");
      http.begin(client, _url);
      http.addHeader("X-Auth-Tenant-Token", this->token);     
      http.addHeader("Content-Type", "application/json");

      DynamicJsonDocument payload(128);
      payload["value1"] = newVal;
      json = "";
      serializeJson(payload, json);
      log_i("payload memory usage %d", payload.memoryUsage() );

      log_i( "POST url : %s data : % s", _url.c_str(), json.c_str() );
      statusCode = http.POST( json );
      http.end();
      if ( this->checkForErrors( statusCode ) ) {
        client.stop();
        return 0;
      };

      client.stop();
      return 1;
    }

    int get() {
      // возврящает :
      // 0 - если произошла ошибка
      // 1 - если данные считаны

      String json;
      int statusCode;

      authorize();

      WiFiClientSecure client;
      client.setInsecure();
      HTTPClient http;

      // getting item IDs
      DynamicJsonDocument itemFilter(256);
      itemFilter["items"][0]["id"] = true;
      itemFilter["items"][0]["name"] = true;
      log_i("itemFilter memory usage %d", itemFilter.memoryUsage() );

      DynamicJsonDocument mesuFilter(512);
      mesuFilter[0]["meter"]["id"] = true;
      mesuFilter[0]["meter"]["name"] = true;
      mesuFilter[0]["meter"]["type"] = true;
      mesuFilter[0]["meter"]["attorneyDeadline"] = true;
      mesuFilter[0]["meter"]["lastValue"]["total"]["value"] = true;
      mesuFilter[0]["valueSendInfo"]["meterIndicationDate"]["from"] = true;
      mesuFilter[0]["valueSendInfo"]["meterIndicationDate"]["to"] = true;
      log_i("mesuFilter memory usage %d", mesuFilter.memoryUsage() );

      DynamicJsonDocument eircItems(512);
      String _url = "https://lkk.mosobleirc.ru/api/api/clients/configuration-items";
      http.begin(client, _url );
      http.addHeader("X-Auth-Tenant-Token", this->token);
      http.addHeader("Content-Type", "application/json");      
      
      log_i( "GET url : %s ", _url.c_str() );
      statusCode = http.GET();
      http.end();
      if ( this->checkForErrors( statusCode ) ) {
        client.stop();
        return 0;
      };

      DeserializationError err1 = deserializeJson(eircItems, http.getString(), DeserializationOption::Filter(itemFilter));
      if (err1) {
        this->bot->sendMessage(err1.c_str());
        log_e( "deserializeJson() failed: %s ", err1.c_str() );
      }
      log_i("eircItems memory usage %d", eircItems.memoryUsage() );

      // print values
      JsonArray i_array = eircItems["items"].as<JsonArray>();
      for(JsonVariant v : i_array) {

        unsigned int iid = v["id"].as<unsigned int>();
        String itemName = v["name"].as<const char*>();

        DynamicJsonDocument eircmeasures(2048);
        String _url = "https://lkk.mosobleirc.ru/api/api/clients/meters/for-item/";
        _url += String(iid);
        http.begin(client, _url);
        http.addHeader("X-Auth-Tenant-Token", this->token);
        http.addHeader("Content-Type", "application/json");      

        log_i( "GET url : %s ", _url.c_str() );
        statusCode = http.GET();
        http.end();
        if ( this->checkForErrors( statusCode ) ) {
          client.stop();
          return 0;
        };

        DeserializationError err2 = deserializeJson(eircmeasures, http.getString(), DeserializationOption::Filter(mesuFilter));
        if (err2) {
          this->bot->sendMessage(err1.c_str());
          log_e( "deserializeJson() failed: %s", err2.c_str() );
        }
        log_i("eircmeasures memory usage %d", eircmeasures.memoryUsage() );        

        this->date_from = 0;
        this->date_to   = 32;

        JsonArray m_array = eircmeasures.as<JsonArray>();
        for(JsonVariant m : m_array) {
          
          uint32_t id = m["meter"]["id"].as<unsigned int>();
          log_i( "id : %d compare with (%d %d) ", id, hot_id, col_id );
          if ( id == col_id || id == hot_id ) {
            String mid = String( id );
            this->measures[mid]["id"]       = mid;
            this->measures[mid]["item"]     = itemName;
            this->measures[mid]["name"]     = strdup(m["meter"]["name"].as<const char*>());
            this->measures[mid]["type"]     = strdup(m["meter"]["type"].as<const char*>());
            this->measures[mid]["attorney"] = strdup(m["meter"]["attorneyDeadline"].as<const char*>());
            this->measures[mid]["value"]    = m["meter"]["lastValue"]["total"]["value"].as<unsigned int>();
            this->measures[mid]["from"]     = m["valueSendInfo"]["meterIndicationDate"]["from"].as<unsigned int>();
            this->measures[mid]["to"]       = m["valueSendInfo"]["meterIndicationDate"]["to"].as<unsigned int>();
            if ( this->measures[mid]["from"] > this->date_from ) this->date_from = this->measures[mid]["from"].as<unsigned int>();
            if ( this->measures[mid]["to"]   < this->date_to   ) this->date_to   = this->measures[mid]["to"].as<unsigned int>();
            this->pars_attorney( strdup(this->measures[mid]["attorney"].as<const char*>()));
          }
        }
      }   
      //serializeJsonPretty(this->measures, Serial);

      this->status = 1;
      client.stop();
      return 1;
    }
};
#endif
