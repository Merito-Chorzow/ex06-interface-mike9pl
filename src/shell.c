#include <stdio.h>
#include "shell.h"

// Pomocnicza funkcja do wypisywania bajtu w formacie heksadecymalnym.
static void tx_hex_byte(uint8_t b){
    static const char* hex = "0123456789ABCDEF";
    putchar(hex[(b >> 4) & 0x0F]);
    putchar(hex[b & 0x0F]);
}
// Przetwarzanie ramki protokołu. 
static void on_msg(void* ctx, const proto_msg_t* msg, uint32_t rx_frame_start_ms, uint32_t rx_frame_end_ms){
    shell_t* sh = (shell_t*)ctx;
    // Wyświetl przychodzącą ramkę (czytelne logi w trybie testowym).
    if (sh->log_io) printf("RX: cmd=%s(0x%02X) payload_len=%u crc=OK\n", proto_cmd_name(msg->cmd), msg->cmd, msg->payload_len);
    // Obsługa komendy urządzenia.
    proto_reason_t r = device_handle_cmd(&sh->dev, msg->cmd, msg->payload, msg->payload_len);
    if (r != PROTO_REASON_OK){
        if (sh->log_io) printf("EVT: NACK reason=%s(%u)\n", proto_reason_name(r), (unsigned)r);
        (void)proto_send_nack(&sh->proto, msg->cmd, r);
        return;
    }
    // Obsługa komendy GET_STAT.
    if (msg->cmd == PROTO_CMD_GET_STAT){
        uint8_t pl[64];
        uint8_t n = device_pack_stat(
            &sh->dev,
            sh->ticks,
            (uint32_t)sh->rx.dropped,
            &sh->proto.stats,
            pl,
            (uint8_t)sizeof(pl)
        );
        if (sh->log_io) printf("EVT: STAT\n");
        (void)proto_send(&sh->proto, PROTO_CMD_STAT, pl, n);
        sh->proto.stats.last_cmd_latency_ms = (rx_frame_end_ms - rx_frame_start_ms);
        return;
    }
    // Dla pozostałych komend, wysyłamy ACK.
    if (sh->log_io) printf("EVT: ACK\n");
    (void)proto_send_ack(&sh->proto, msg->cmd);
    sh->proto.stats.last_cmd_latency_ms = (rx_frame_end_ms - rx_frame_start_ms);
}
// Obsługa błędów ramki protokołu.
static void on_err(void* ctx, proto_reason_t reason, uint8_t cmd){
    shell_t* sh = (shell_t*)ctx;
    // Obsługa błędów parsera: (czytelne logi w trybie testowym).
    if (!sh->log_io) return;
    if (cmd) printf("ERR: reason=%s(%u) cmd=%s(0x%02X)\n", proto_reason_name(reason), (unsigned)reason, proto_cmd_name(cmd), cmd);
    else printf("ERR: reason=%s(%u)\n", proto_reason_name(reason), (unsigned)reason);
}
// Inicjalizacja powłoki
void shell_init(shell_t* sh){
    rb_init(&sh->rx); rb_init(&sh->tx);
    sh->now_ms = 0;
    sh->ms_per_tick = 1;
    sh->ticks = 0;
    sh->log_io = 1;
    device_init(&sh->dev);
    proto_init(&sh->proto, &sh->rx, &sh->tx);
    printf("INFO: READY\n");
}
// Wstrzyknięcie bajtów do bufora RX (symulacja UART)
void shell_rx_bytes(shell_t* sh, const uint8_t* data, size_t len){
    for (size_t i = 0; i < len; i++){
        (void)rb_put(&sh->rx, data[i]);
    }
}
// Jeden "tick" systemowy — przetwarza RX, timeouts i wysyła TX
void shell_tick(shell_t* sh){
    sh->ticks++;
    sh->now_ms += sh->ms_per_tick;

    proto_poll(&sh->proto, sh->now_ms, on_msg, on_err, sh);

    // "wysyłka" (UART) — dla czytelności wypisujemy heksami,
    // ponieważ w urządzeniu byłby to strumień bajtów.
    uint8_t b;
    int any_tx = 0;
    if (sh->log_io){
        if (rb_count(&sh->tx) > 0) { printf("TX: "); any_tx = 1; }
        while (rb_get(&sh->tx, &b)){
            tx_hex_byte(b);
            putchar(' ');
        }
        if (any_tx) putchar('\n');
    } else {
        // W trybie bez logów, opróżniamy bufor TX
        while (rb_get(&sh->tx, &b)) { (void)b; }
    }
}