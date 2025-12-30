#pragma once
#include "ringbuf.h"
#include "protocol.h"
#include "device.h"

// Struktura reprezentująca powłokę.
typedef struct {
    rb_t rx, tx;
    proto_t proto;
    device_t dev;
    uint32_t now_ms;
    uint32_t ms_per_tick;
    uint32_t ticks;
    int log_io;
} shell_t;

// Inicjalizacja powłoki
void shell_init(shell_t* sh);
// Wstrzyknięcie bajtów do bufora RX (symulacja UART)
void shell_rx_bytes(shell_t* sh, const uint8_t* data, size_t len);
// Jeden "tick" systemowy — przetwarza RX, timeouts i wysyła TX
void shell_tick(shell_t* sh);