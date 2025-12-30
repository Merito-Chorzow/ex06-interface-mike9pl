#include <stdio.h>
#include <string.h>
#include "shell.h"

// Pomocnicza funkcja do obliczania CRC-8.
static uint8_t crc8_update(uint8_t crc, uint8_t data){
    crc ^= data;
    for (unsigned i = 0; i < 8; i++){
        crc = (crc & 0x80u) ? (uint8_t)((crc << 1) ^ 0x07u) : (uint8_t)(crc << 1);
    }
    return crc;
}

// Funkcja do obliczania CRC-8 dla ramki.
static uint8_t crc8_compute(uint8_t len, const uint8_t* data){
    uint8_t crc = 0;
    crc = crc8_update(crc, len);
    for (uint8_t i = 0; i < len; i++) crc = crc8_update(crc, data[i]);
    return crc;
}
// Buduje ramkę protokołu w podanym buforze wyjściowym.
static size_t build_frame(uint8_t cmd, const uint8_t* payload, uint8_t payload_len, uint8_t* out, size_t out_cap){
    // STX|LEN|CMD|PAYLOAD|CRC
    const size_t total = (size_t)(1u + 1u + 1u + payload_len + 1u);
    if (out_cap < total) return 0;
    out[0] = PROTO_STX;
    out[1] = (uint8_t)(1u + payload_len);
    out[2] = cmd;
    if (payload_len) memcpy(&out[3], payload, payload_len);
    const uint8_t crc = crc8_compute(out[1], &out[2]);
    out[3u + payload_len] = crc;
    return total;
}
// Wstrzyknięcie ramki do powłoki (symulacja przychodzących danych).
static void inject_frame(shell_t* sh, uint8_t cmd, const uint8_t* payload, uint8_t payload_len, int corrupt_crc){
    uint8_t buf[128];
    size_t n = build_frame(cmd, payload, payload_len, buf, sizeof(buf));
    if (!n){
        printf("INFO: build_frame failed\n");
        return;
    }
    if (corrupt_crc) buf[n - 1] ^= 0xFFu;
    // Symulacja wstrzyknięcia ramki do powłoki (możliwa modyfikacja CRC)
    printf("INFO: inject cmd=%s(0x%02X) bytes=%zu%s\n", proto_cmd_name(cmd), cmd, n, corrupt_crc ? " (CRC BAD)" : "");
    shell_rx_bytes(sh, buf, n);
}
// Wstrzyknięcie niekompletnej ramki do powłoki (symulacja przychodzących danych).
static void inject_partial(shell_t* sh, const uint8_t* bytes, size_t len){
    printf("INFO: inject partial bytes=%zu\n", len);
    shell_rx_bytes(sh, bytes, len);
}
// Uruchomienie określonej liczby "ticków" powłoki.
static void run_ticks(shell_t* sh, int n){
    for (int i = 0; i < n; i++) shell_tick(sh);
}
// Wyświetlanie statystyk powłoki.
static void print_stats(const shell_t* sh){
    printf(
        "STATS: ticks=%u rx_dropped=%zu broken_frames=%u crc_errors=%u last_cmd_latency=%ums\n\n",
        sh->ticks,
        sh->rx.dropped,
        sh->proto.stats.broken_frames,
        sh->proto.stats.crc_errors,
        sh->proto.stats.last_cmd_latency_ms
    );
}

int main(void){
    shell_t sh;
    shell_init(&sh);
    print_stats(&sh);

    printf("\n=== 1) Funkcjonalne ===\n\n");
    {
        uint8_t speed = 80;
        inject_frame(&sh, PROTO_CMD_SET_SPEED, &speed, 1, 0);
        run_ticks(&sh, 5);
        print_stats(&sh);

        uint8_t mode = (uint8_t)DEVICE_MODE_CLOSED;
        inject_frame(&sh, PROTO_CMD_SET_MODE, &mode, 1, 0);
        run_ticks(&sh, 5);
        print_stats(&sh);

        inject_frame(&sh, PROTO_CMD_GET_STAT, NULL, 0, 0);
        run_ticks(&sh, 5);
        print_stats(&sh);

        inject_frame(&sh, PROTO_CMD_STOP, NULL, 0, 0);
        run_ticks(&sh, 5);
        print_stats(&sh);

        inject_frame(&sh, PROTO_CMD_GET_STAT, NULL, 0, 0);
        run_ticks(&sh, 5);
        print_stats(&sh);
    }

    printf("\n=== 2) Błędne ===\n\n");
    {
        uint8_t speed = 42;
        inject_frame(&sh, PROTO_CMD_SET_SPEED, &speed, 1, 1); // zły CRC
        run_ticks(&sh, 5);
        print_stats(&sh);

        // Nieznana komenda
        inject_frame(&sh, 0x55, NULL, 0, 0);
        run_ticks(&sh, 5);
        print_stats(&sh);

        // Zły payload (SET_SPEED bez payload)
        inject_frame(&sh, PROTO_CMD_SET_SPEED, NULL, 0, 0);
        run_ticks(&sh, 5);
        print_stats(&sh);

        // Niekompletna ramka -> timeout
        // Rozpocznij ramkę i porzuć resztę (STX, LEN=2, CMD=STOP),bez CRC.
        uint8_t partial[] = { PROTO_STX, 0x01, PROTO_CMD_STOP };
        inject_partial(&sh, partial, sizeof(partial));
        run_ticks(&sh, (int)(PROTO_FRAME_TIMEOUT_MS + 5u));
        print_stats(&sh);
    }

    printf("\n=== 3) Obciążeniowe (burst >= 200 ramek) ===\n\n");
    {
        // Wysłanie 200 ramek SET_SPEED z prędkościami 0..100 bez logów.
        sh.log_io = 0;
        for (int i = 0; i < 200; i++){
            uint8_t speed = (uint8_t)(i % 101);
            uint8_t frame[8];
            size_t n = build_frame(PROTO_CMD_SET_SPEED, &speed, 1, frame, sizeof(frame));
            shell_rx_bytes(&sh, frame, n);
        }
        run_ticks(&sh, 200);
        // Włącz logi i wyślij GET_STAT, aby zobaczyć statystyki.
        sh.log_io = 1;
        inject_frame(&sh, PROTO_CMD_GET_STAT, NULL, 0, 0);
        run_ticks(&sh, 10);
        print_stats(&sh);
    }

    return 0;
}
