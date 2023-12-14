#ifndef _saver_h
#define _saver_h

#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE

#include <Arduino.h>
#include "FastBot.h"
#include "esp_log.h"


const uint32_t COL_OFF = 0x300000;
const uint32_t COL_LEN = 0x90000;
const uint32_t HOT_OFF = 0x390000;
const uint32_t HOT_LEN = 0x50000;
const uint32_t DATE_OFF = 0x3E0000;
const uint32_t DATE_LEN = 0x10000;

class Saver {
  public:
    uint32_t   hot;
    uint32_t   col;
    uint32_t   date;
    uint32_t   hot_loc;
    uint32_t   col_loc;
    uint32_t   date_loc;
    FastBot*   bot;

    Saver( FastBot& _bot ) {
        bot = &_bot;
        hot = 0;
        col = 0;
        date = 12310;
        hot_loc = HOT_OFF;
        col_loc = COL_OFF;
        date_loc = DATE_OFF;
        this->get();
    }

    void init_mem( const uint32_t OFF, const uint32_t LEN, uint32_t& loc ) {
      for( uint32_t i=OFF; i<(OFF+LEN); i+=0x1000 )
        ESP.flashEraseSector(i/0x1000);
      loc = OFF;
    }

    void init() {
      log_i("Saver Init started" );
      this->init_mem( COL_OFF, COL_LEN, this->col_loc );
      this->init_mem( HOT_OFF, HOT_LEN, this->hot_loc );
      this->init_mem( DATE_OFF, DATE_LEN, this->date_loc );
      log_i("Saver Init finished" );
    }

    uint32_t get_mem( const uint32_t OFF, const uint32_t LEN, uint32_t& val, uint32_t& loc ) {
      uint32_t tmp;
      uint32_t a;
      a=OFF; 
      while ( a<(OFF+LEN) ) {
        ESP.flashRead( a , (uint32_t*)&tmp, sizeof(tmp));
        if (tmp != 0xFFFFFFFF) {
          val = tmp;
          a = a + 4;
        } else {
          loc = a;
          break;
        }
      }
      return val;
    }

    void get() {
      log_i("Saver get started" );
      this->get_mem( COL_OFF, COL_LEN, this->col, this->col_loc );
      this->get_mem( HOT_OFF, HOT_LEN, this->hot, this->hot_loc );
      this->get_mem( DATE_OFF, DATE_LEN, this->date, this->date_loc );
      log_i("Saver get finished" );
    }

    void send_log() {
      log_i("Saver set #%d#%d#%d", this->col, this->hot, this->date );
      String s;
      s = '#';
      s += String( this->col );
      s += '#';
      s += String( this->hot );
      s += '#';
      s += String( this->date );       
      //this->bot->setMyDescription( s );
      //log_i("Desc %s", this->bot->getMyDescription());
    }

    void set_col( uint32_t val ) {
      this->col = val;
      if (this->col_loc >= (COL_OFF+COL_LEN)) 
        this->init_mem( COL_OFF, COL_LEN, this->col_loc );
      ESP.flashWrite(this->col_loc, (uint32_t*)&val, sizeof(val));
      this->col_loc += 4;
      this->send_log();
      return;
    }

    void set_hot( uint32_t val ) {
      this->hot = val;
      if (this->hot_loc >= (HOT_OFF+HOT_LEN)) 
        this->init_mem( HOT_OFF, HOT_LEN, this->hot_loc );
      ESP.flashWrite(this->hot_loc, (uint32_t*)&val, sizeof(val));
      this->hot_loc += 4;
      this->send_log();
      return;
    }

    void set_date( uint32_t val ) {
      this->date = val;
      if (this->date_loc >= (DATE_OFF+DATE_LEN)) 
        this->init_mem( DATE_OFF, DATE_LEN, this->date_loc );
      ESP.flashWrite(this->date_loc, (uint32_t*)&val, sizeof(val));
      this->date_loc += 4;
      this->send_log();
      return;
    }
};

#endif
