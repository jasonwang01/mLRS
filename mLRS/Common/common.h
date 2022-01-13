//*******************************************************
// Copyright (c) MLRS project
// GPL3
// https://www.gnu.org/licenses/gpl-3.0.de.html
// OlliW @ www.olliw.eu
//*******************************************************
// common
//*******************************************************
#ifndef COMMON_H
#define COMMON_H
#pragma once


#include "common_conf.h"
#include "sx1280_driver.h"
#include "lq_counter.h"
#include "fhss.h"
#include "frame_types.h"


//-------------------------------------------------------
// SysTask
//-------------------------------------------------------

volatile uint8_t doSysTask = 0;


void HAL_IncTick(void)
{
    uwTick += uwTickFreq;
    doSysTask = 1;
}


uint32_t millis32(void)
{
    return uwTick;
}


//-------------------------------------------------------
// Frames
//-------------------------------------------------------

STATIC_ASSERT(sizeof(tFrameStatus) == FRAME_HEADER_LEN - 2, "Frame header len missmatch")
STATIC_ASSERT(sizeof(tTxFrame) == FRAME_TX_RX_LEN, "Frame len missmatch TxFrame")
STATIC_ASSERT(sizeof(tRxFrame) == FRAME_TX_RX_LEN, "Frame len missmatch RxFrame")


typedef enum {
    CHECK_OK = 0,
    CHECK_ERROR_SYNCWORD, // 1
    CHECK_ERROR_HEADER,   // 2
    CHECK_ERROR_CRC1,     // 3
    CHECK_ERROR_CRC,      // 4
} CHECK_ENUM;


void pack_tx_frame(tTxFrame* frame, tFrameStats* frame_stats, tRcData* rc, uint8_t* payload, uint8_t payload_len)
{
uint16_t crc;

    if (payload_len > FRAME_TX_PAYLOAD_LEN) payload_len = FRAME_TX_PAYLOAD_LEN; // should never occur, but play it safe

    memset(frame, 0, sizeof(tTxFrame));

    // generate header
    frame->sync_word = FRAME_SYNCWORD;
    frame->status.seq_no = frame_stats->seq_no;
    frame->status.ack = frame_stats->ack;
    frame->status.frame_type = FRAME_TYPE_TX;
    frame->status.antenna = frame_stats->antenna;
    frame->status.rssi_u7 = -(frame_stats->rssi);
    frame->status.LQ = frame_stats->LQ;
    frame->status.LQ_rc_data = 0x7F;
    frame->status.payload_len = payload_len;

    // pack rc data
    // rcData: 0 .. 1024 .. 2047, 11 bits
    frame->rc1.ch0  = rc->ch[0]; // 0 .. 1024 .. 2047, 11 bits
    frame->rc1.ch1  = rc->ch[1];
    frame->rc1.ch2  = rc->ch[2];
    frame->rc1.ch3  = rc->ch[3];
    for (uint8_t i = 0; i < FRAME_TX_RCDATA2_LEN; i++) {
      frame->rc2.ch[i] = (rc->ch[4+i] / 8); // 0 .. 128 .. 255, 8 bits
    }
    frame->rc1.ch14 = (rc->ch[14] >= 1536) ? 1 : 0; // 0 ... 1
    frame->rc1.ch15 = (rc->ch[15] >= 1536) ? 1 : 0;
    frame->rc1.ch16 = (rc->ch[16] >= 1536) ? 1 : 0;
    frame->rc1.ch17 = (rc->ch[17] >= 1536) ? 1 : 0;

    // pack the payload
    for (uint8_t i = 0; i < payload_len; i++) {
      frame->payload[i] = payload[i];
    }

    // finalize, crc
    fmav_crc_init(&crc);
    fmav_crc_accumulate_buf(&crc, (uint8_t*)frame, FRAME_HEADER_LEN + FRAME_TX_RCDATA1_LEN);
    frame->crc1 = crc;

    fmav_crc_accumulate_buf(&crc, (uint8_t*)frame + FRAME_HEADER_LEN + FRAME_TX_RCDATA1_LEN, FRAME_TX_RX_LEN - FRAME_HEADER_LEN - FRAME_TX_RCDATA1_LEN - 2);
    frame->crc = crc;
}


// returns 0 if OK !!
uint8_t check_tx_frame(tTxFrame* frame)
{
uint16_t crc;

    if (frame->sync_word != FRAME_SYNCWORD) return CHECK_ERROR_SYNCWORD;
    if (frame->status.frame_type != FRAME_TYPE_TX) return CHECK_ERROR_HEADER;
    if (frame->status.payload_len > FRAME_TX_PAYLOAD_LEN) return CHECK_ERROR_HEADER;

    fmav_crc_init(&crc);
    fmav_crc_accumulate_buf(&crc, (uint8_t*)frame, FRAME_HEADER_LEN + FRAME_TX_RCDATA1_LEN);
    if (crc != frame->crc1) return CHECK_ERROR_CRC1;

    fmav_crc_accumulate_buf(&crc, (uint8_t*)frame + FRAME_HEADER_LEN + FRAME_TX_RCDATA1_LEN, FRAME_TX_RX_LEN - FRAME_HEADER_LEN - FRAME_TX_RCDATA1_LEN - 2);
    if (crc != frame->crc) return CHECK_ERROR_CRC;

    return CHECK_OK;
}


void rcdata_ch0to3_from_txframe(tRcData* rc, tTxFrame* frame)
{
    rc->ch[0] = frame->rc1.ch0;
    rc->ch[1] = frame->rc1.ch1;
    rc->ch[2] = frame->rc1.ch2;
    rc->ch[3] = frame->rc1.ch3;
}


void rcdata_from_txframe(tRcData* rc, tTxFrame* frame)
{
    rc->ch[0] = frame->rc1.ch0;
    rc->ch[1] = frame->rc1.ch1;
    rc->ch[2] = frame->rc1.ch2;
    rc->ch[3] = frame->rc1.ch3;
    for (uint8_t i = 0; i < FRAME_TX_RCDATA2_LEN; i++) {
        rc->ch[4+i] = frame->rc2.ch[i] * 8;
    }
    rc->ch[14] = (frame->rc1.ch14) ? 2047 : 0;
    rc->ch[15] = (frame->rc1.ch15) ? 2047 : 0;
    rc->ch[16] = (frame->rc1.ch16) ? 2047 : 0;
    rc->ch[17] = (frame->rc1.ch17) ? 2047 : 0;
}


void pack_rx_frame(tRxFrame* frame, tFrameStats* frame_stats, uint8_t* payload, uint8_t payload_len)
{
uint16_t crc;

    if (payload_len > FRAME_RX_PAYLOAD_LEN) payload_len = FRAME_RX_PAYLOAD_LEN; // should never occur, but play it safe

    memset(frame, 0, sizeof(tRxFrame));

    frame->sync_word = FRAME_SYNCWORD;
    frame->status.seq_no = frame_stats->seq_no;
    frame->status.ack = frame_stats->ack;
    frame->status.frame_type = FRAME_TYPE_RX;
    frame->status.antenna = frame_stats->antenna;
    frame->status.rssi_u7 = -(frame_stats->rssi);
    frame->status.LQ = frame_stats->LQ;
    frame->status.LQ_rc_data = frame_stats->LQ_rc_data;
    frame->status.payload_len = payload_len;

    for (uint8_t i = 0; i < payload_len; i++) {
      frame->payload[i] = payload[i];
    }

    fmav_crc_init(&crc);
    fmav_crc_accumulate_buf(&crc, (uint8_t*)frame, FRAME_TX_RX_LEN - 2);
    frame->crc = crc;
}


// returns 0 if OK !!
uint8_t check_rx_frame(tRxFrame* frame)
{
uint16_t crc;

    if (frame->sync_word != FRAME_SYNCWORD) return CHECK_ERROR_SYNCWORD;

    if (frame->status.frame_type != FRAME_TYPE_RX) return CHECK_ERROR_HEADER;
    if (frame->status.payload_len > FRAME_RX_PAYLOAD_LEN) return CHECK_ERROR_HEADER;

    fmav_crc_init(&crc);
    fmav_crc_accumulate_buf(&crc, (uint8_t*)frame, FRAME_TX_RX_LEN - 2);
    if (crc != frame->crc) return CHECK_ERROR_CRC;

    return CHECK_OK;
}


//-------------------------------------------------------
// Stats
//-------------------------------------------------------

class Stats {
  public:
	  // this is one of the ways to calculate LQ stats
    uint32_t frames_received;
    uint32_t valid_crc1_frames_received; // received frames which passed check_rx_frame() up to crc1, but not crc
    uint32_t valid_frames_received; // received frames which also pass check_rx_frame()

    uint8_t LQ_received;
    uint8_t LQ_valid_crc1_received;
    uint8_t LQ_valid_received;

    uint32_t frames_received_last;
    uint32_t valid_crc1_frames_received_last;
    uint32_t valid_frames_received_last;

    // bytes stats
    uint32_t bytes_transmitted; // 50 * 64 = 3200 bytes/s or 50 * 82 = 4100 bytes/s => good for 12 days
    uint32_t bytes_received;

    uint32_t bytes_transmitted_last;
    uint32_t bytes_received_last;

    uint16_t bytes_per_sec_transmitted;
    uint16_t bytes_per_sec_received;

    // statistics for our device
    int8_t last_rx_rssi; // note: is negative!
    int8_t last_rx_snr; // note: can be negative!
    uint8_t rx_LQ;
    uint8_t rx_LQ_trc_data; // only used in RX frame

    // statistics received from the other end
    uint8_t received_antenna;
    int8_t received_rssi; // note: is negative!
    uint8_t received_LQ;
    uint8_t received_LQ_rc_data;

    uint8_t tx_seq_no;
    uint8_t received_seq_no;

    uint8_t tx_ack;
    uint8_t received_ack;

    void Init(void)
    {
        frames_received = valid_crc1_frames_received = valid_frames_received = 0;
        LQ_received = LQ_valid_crc1_received = LQ_valid_received = 0; //UINT8_MAX;
        frames_received_last = valid_crc1_frames_received_last = valid_frames_received_last = 0;

        bytes_transmitted = bytes_received = 0;
        bytes_transmitted_last = bytes_received_last = 0;

        last_rx_rssi = INT8_MAX;
        last_rx_snr = INT8_MAX;
        rx_LQ = 0; //UINT8_MAX;

        received_antenna = UINT8_MAX;
        received_rssi = INT8_MAX;
        received_LQ = 0; //UINT8_MAX;
    }

    void Clear(void)
    {
        last_rx_rssi = INT8_MAX;
        last_rx_snr = INT8_MAX;
        rx_LQ = 0; //UINT8_MAX;

        received_rssi = INT8_MAX;
        received_LQ = 0; //UINT8_MAX;
    }

    void AddBytesTransmitted(uint16_t len)
    {
        bytes_transmitted += len;
    }

    void AddBytesReceived(uint16_t len)
    {
        bytes_received += len;
    }

    void Update1Hz(void)
    {
    uint32_t diff;

        diff = frames_received - frames_received_last;
        LQ_received = (diff * 100 * FRAME_RATE_MS) / 1000;

        diff = valid_crc1_frames_received - valid_crc1_frames_received_last;
        LQ_valid_crc1_received = (diff * 100 * FRAME_RATE_MS) / 1000;

        diff = valid_frames_received - valid_frames_received_last;
        LQ_valid_received = (diff * 100 * FRAME_RATE_MS) / 1000;

        frames_received_last = frames_received;
        valid_crc1_frames_received_last = valid_crc1_frames_received;
        valid_frames_received_last = valid_frames_received;

        diff = bytes_transmitted - bytes_transmitted_last;
        bytes_per_sec_transmitted = diff;

        diff = bytes_received - bytes_received_last;
        bytes_per_sec_received = diff;

        bytes_transmitted_last = bytes_transmitted;
        bytes_received_last = bytes_received;

#ifdef DEVICE_IS_TRANSMITTER
        rx_LQ = LQ_valid_received; // this should always be <= valid_received !
#endif
#ifdef DEVICE_IS_RECEIVER
        // same logic as for rxstats
        if (LQ_valid_received >= 25) {
            rx_LQ = LQ_valid_received;
        } else
        if (LQ_valid_crc1_received >= 25) {
            rx_LQ = 25;
        } else {
            rx_LQ = LQ_valid_crc1_received;
        }
#endif
    }

    uint16_t GetTransmitBytesPerSec(void)
    {
        return bytes_per_sec_transmitted;
    }

    uint16_t GetReceiveBytesPerSec(void)
    {
        return bytes_per_sec_received;
    }

    uint8_t GetTransmitBandwidthUsage(void)
    {
      // just simply scale it always to the largest theoretical bandwidth
      // is 4100 bytes/s max
      uint8_t bw =  (bytes_per_sec_transmitted + 20) / 41;
      if ((bw == 0) && (bytes_per_sec_transmitted > 0)) bw = 1; // ensure it's always at least 1% if some bytes are transmitted
      return bw;
    }

    uint8_t GetReceiveBandwidthUsage(void)
    {
      // just simply scale it always to the largest theoretical bandwidth
      // is 4100 bytes/s max
      uint8_t bw =  (bytes_per_sec_received + 20) / 41;
      if ((bw == 0) && (bytes_per_sec_received > 0)) bw = 1; // ensure it's always at least 1% if some bytes are received
      return bw;
    }
};


//-------------------------------------------------------
// Generic Serial Class
//-------------------------------------------------------

class tSerialBase
{
  public:
    void Init(void) {};
    virtual void putc(char c) {}
    void putbuf(void* buf, uint16_t len) { for (uint16_t i = 0; i < len; i++) putc(((char*)buf)[i]); }
    bool available(void) { return 0; }
    char getc(void) { return '\0'; }
    void flush(void) {};
};

// this is the serial port
// we have setup the hals such that it is always uartb
class tSerialPort : public tSerialBase
{
  public:
    void Init(void) { uartb_init(); }
    void putc(char c) override { uartb_putc(c); }
    bool available(void) { return uartb_rx_available(); }
    char getc(void) { return uartb_getc(); }
    void flush(void) { uartb_rx_flush(); uartb_tx_flush(); }
};


//-------------------------------------------------------
// Common Variables
//-------------------------------------------------------

tSerialPort serial;

tRcData rcData;
tTxFrame txFrame;
tRxFrame rxFrame;

SxDriver sx;
Stats stats;

FhssBase fhss;



#endif // COMMON_H
