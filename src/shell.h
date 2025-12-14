#pragma once
#include "ringbuf.h"

typedef struct {
    rb_t rx, tx;
    float setpoint;
    unsigned ticks;
    unsigned broken_lines; // linie ucięte przez overflow
} shell_t;

void shell_init(shell_t* sh);
void shell_rx_bytes(shell_t* sh, const char* s); // wstrzyknięcie wejścia
void shell_tick(shell_t* sh);                    // "SysTick" — przetwarza RX/TX
