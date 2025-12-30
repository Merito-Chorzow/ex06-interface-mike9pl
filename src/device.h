#pragma once
#include <stdint.h>
#include "protocol.h"

// Definicje stanów urządzenia.
typedef enum {
    DEVICE_MODE_OPEN = 0,
    DEVICE_MODE_CLOSED = 1,
} device_mode_t;
// Struktura reprezentująca stan urządzenia.
typedef struct {
    uint8_t speed;         // 0..100
    device_mode_t mode;
} device_t;

// Inicjalizacja stanu urządzenia.
void device_init(device_t* d);

// Wykonaj komendę przy założeniu, że ramka została już poprawnie zdekodowana.
proto_reason_t device_handle_cmd(device_t* d, uint8_t cmd, const uint8_t* payload, uint8_t payload_len);

// Pakuje stan urządzenia oraz dane telemetryczne do bufora wyjściowego STAT.
uint8_t device_pack_stat(
    const device_t* d,
    uint32_t ticks,
    uint32_t rx_dropped,
    const proto_stats_t* pstats,
    uint8_t* out,
    uint8_t out_cap
);