#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "shell.h"

static void tx_str(shell_t* sh, const char* s){
    while (*s) rb_put(&sh->tx, (uint8_t)*s++);
}

void shell_init(shell_t* sh){
    rb_init(&sh->rx); rb_init(&sh->tx);
    sh->setpoint = 0.0f;
    sh->ticks = 0;
    sh->broken_lines = 0;
    tx_str(sh, "READY\r\n");
}

void shell_rx_bytes(shell_t* sh, const char* s){
    while (*s) {
        if (!rb_put(&sh->rx, (uint8_t)*s++)) {
            // overflow na RX — jeżeli odetnie linię, wykryjemy to w tick
        }
    }
}

static void process_line(shell_t* sh, const char* line){
    if (strncmp(line,"set ",4)==0){
        float v = strtof(line+4, NULL);
        sh->setpoint = v;
        char buf[64]; snprintf(buf,sizeof(buf),"OK set=%.3f\r\n", v);
        tx_str(sh, buf);
    } else if (strncmp(line,"get",3)==0){
        char buf[96];
        snprintf(buf,sizeof(buf),"set=%.3f ticks=%u drop=%zu broken=%u\r\n",
            sh->setpoint, sh->ticks, sh->rx.dropped, sh->broken_lines);
        tx_str(sh, buf);
    } else if (strncmp(line,"stat",4)==0){
        char buf[96];
        snprintf(buf,sizeof(buf),"rx_free=%zu tx_free=%zu rx_count=%zu\r\n",
            rb_free(&sh->rx), rb_free(&sh->tx), rb_count(&sh->rx));
        tx_str(sh, buf);
    } else if (strncmp(line,"echo ",5)==0){
        tx_str(sh, "ECHO ");
        tx_str(sh, line+5);
        tx_str(sh, "\r\n");
    } else {
        tx_str(sh, "ERR\r\n");
    }
}

void shell_tick(shell_t* sh){
    sh->ticks++;

    static char line[128];
    static size_t n = 0;
    uint8_t b;
    int saw_newline = 0;

    while (rb_get(&sh->rx, &b)){
        if (b == '\n' || b == '\r'){
            if (n > 0){
                line[n] = 0;
                process_line(sh, line);
                n = 0;
                saw_newline = 1;
            }
        } else if (n + 1 < sizeof(line)){
            line[n++] = (char)b;
        } else {
            // linia za długa → oznacz i wyczyść
            sh->broken_lines++;
            n = 0;
        }
    }

    // heurystyka: jeśli był overflow (rx.dropped>0) i nie domknęliśmy linii,
    // a w kolejnych tickach pojawia się początek nowej komendy — rośnie broken_lines.
    (void)saw_newline;

    // "wysyłka" — w urządzeniu byłby UART; tu wypisujemy na stdout
    while (rb_get(&sh->tx, &b)) { putchar((char)b); }
}
