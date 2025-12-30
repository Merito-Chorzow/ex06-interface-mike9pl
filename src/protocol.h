#pragma once
#include <stdint.h>
#include <stddef.h>
#include "ringbuf.h"

// Ramka: STX | LEN | CMD | PAYLOAD | CRC
// - STX: 0x02
// - LEN: liczba bajtów w polu CMD + PAYLOAD (1 + N)
// - CMD: komenda (1 bajt)
// - PAYLOAD: dane (0..64 bajtów)
// - CRC: CRC-8 obliczone po bajcie LEN (polinom 0x07, init 0x00)

#define PROTO_STX 0x02u // Start of Text. Początek ramki. 
// Maksymalny rozmiar pola PAYLOAD
#ifndef PROTO_MAX_PAYLOAD
#define PROTO_MAX_PAYLOAD 64u
#endif
// Timeouty protokołu
#ifndef PROTO_BYTE_TIMEOUT_MS
#define PROTO_BYTE_TIMEOUT_MS 20u
#endif
// Całkowity timeout ramki
#ifndef PROTO_FRAME_TIMEOUT_MS
#define PROTO_FRAME_TIMEOUT_MS 200u
#endif
// Definicje komend protokołu.
typedef enum {
    PROTO_CMD_SET_SPEED = 0x01,
    PROTO_CMD_SET_MODE  = 0x02,
    PROTO_CMD_STOP      = 0x03,
    PROTO_CMD_GET_STAT  = 0x04,

    PROTO_CMD_ACK  = 0x80,
    PROTO_CMD_NACK = 0x81,
    PROTO_CMD_STAT = 0x82,
} proto_cmd_t;
// Definicje NACK (błędów protokołu).
typedef enum {
    PROTO_REASON_OK = 0,
    PROTO_REASON_BAD_STX,
    PROTO_REASON_BAD_LEN,
    PROTO_REASON_CRC,
    PROTO_REASON_UNKNOWN_CMD,
    PROTO_REASON_BAD_PAYLOAD,
    PROTO_REASON_TIMEOUT,
} proto_reason_t;

// Pomocnicze funkcje do logów: zwracają nazwy komend i błędów.
const char* proto_cmd_name(uint8_t cmd);
const char* proto_reason_name(proto_reason_t reason);
// Statystyki protokołu.
typedef struct {
    uint32_t broken_frames;
    uint32_t crc_errors;
    uint32_t frame_timeouts;
    uint32_t last_cmd_latency_ms;
    proto_reason_t last_error;
} proto_stats_t;
// Struktura reprezentująca wiadomość protokołu.
typedef struct {
    uint8_t cmd;
    uint8_t payload[PROTO_MAX_PAYLOAD];
    uint8_t payload_len;
} proto_msg_t;
// Struktura reprezentująca stan protokołu.
typedef struct {
    // IO
    rb_t* rx;
    rb_t* tx;

    // Parser FSM
    enum {
        PROTO_FSM_IDLE = 0,
        PROTO_FSM_LEN,
        PROTO_FSM_DATA,
        PROTO_FSM_CRC,
    } state;

    uint8_t len;
    uint8_t data[1u + PROTO_MAX_PAYLOAD]; // CMD + PAYLOAD
    uint8_t data_i;
    uint8_t crc;

    uint32_t frame_start_ms;
    uint32_t last_byte_ms;

    proto_stats_t stats;
} proto_t;
// Typy funkcji callback używanych przez proto_poll().
typedef void (*proto_on_msg_fn)(void* ctx, const proto_msg_t* msg, uint32_t rx_frame_start_ms, uint32_t rx_frame_end_ms);
typedef void (*proto_on_err_fn)(void* ctx, proto_reason_t reason, uint8_t cmd);

// Inicjalizacja struktury protokołu.
void proto_init(proto_t* p, rb_t* rx, rb_t* tx);

// Protokół: obsługa timeoutów oraz parsowanie bajtów z bufora RX.
void proto_poll(proto_t* p, uint32_t now_ms, proto_on_msg_fn on_msg, proto_on_err_fn on_err, void* ctx);

// Nieblokująca wysyłka ramki. Zwraca 1 w przypadku sukcesu, 0 jeśli bufor TX nie mógł pomieścić całej ramki (częściowa ramka NIE jest wysyłana).
int proto_send(proto_t* p, uint8_t cmd, const uint8_t* payload, uint8_t payload_len);

// Nieblokująca wysyłka ACK dla podanej oryginalnej komendy.
int proto_send_ack(proto_t* p, uint8_t orig_cmd);
// Nieblokująca wysyłka NACK dla podanej oryginalnej komendy i powodu błędu.
int proto_send_nack(proto_t* p, uint8_t orig_cmd, proto_reason_t reason);