//*******************************************************
// Copyright (c) MLRS project
// GPL3
// https://www.gnu.org/licenses/gpl-3.0.de.html
// OlliW @ www.olliw.eu
//*******************************************************
// IN
//********************************************************
#ifndef IN_H
#define IN_H
#pragma once


#include <inttypes.h>
#include "..\Common\common_types.h"
#include "..\Common\frame_types.h"


//-------------------------------------------------------
// Generic In Class
//-------------------------------------------------------

typedef enum {
  IN_CONFIG_SBUS = 0,
} IN_CONFIG_ENUM;


class InBase
{
  public:
    void Init(void);

    void Configure(uint8_t new_config);

    void Update(tRcData* rc);

  private:
    virtual bool available(void);
    virtual char getc(void);
    virtual uint16_t tim_1us(void);

    virtual void puts(const char* s);

    virtual void config_sbus(void) = 0;
    void parse_sbus(tRcData* rc);
    void get_sbus_data(tRcData* rc);

    uint8_t _config;

    uint16_t _t_last_us;
    uint8_t _state;
    uint8_t _buf_pos;
    uint8_t _buf[32];
};



#endif // IN_H