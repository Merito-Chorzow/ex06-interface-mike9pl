#include "device.h"

// Ogranicza wartość typu uint8_t do przedziału [lo, hi].
static uint8_t clamp_u8(uint8_t v, uint8_t lo, uint8_t hi){
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}
// Inicjalizacja stanu urządzenia.
void device_init(device_t* d){
    d->speed = 0;
    d->mode = DEVICE_MODE_OPEN;
}
// Wykonaj komendę przy założeniu, że ramka została już poprawnie zdekodowana.
// Zwraca PROTO_REASON_OK w przypadku sukcesu, w przeciwnym razie powód NACK.
proto_reason_t device_handle_cmd(device_t* d, uint8_t cmd, const uint8_t* payload, uint8_t payload_len){
    switch (cmd){
        case PROTO_CMD_SET_SPEED:
            if (payload_len != 1u) return PROTO_REASON_BAD_PAYLOAD;
            d->speed = clamp_u8(payload[0], 0u, 100u);
            return PROTO_REASON_OK;

        case PROTO_CMD_SET_MODE:
            if (payload_len != 1u) return PROTO_REASON_BAD_PAYLOAD;
            if (payload[0] == (uint8_t)DEVICE_MODE_OPEN) d->mode = DEVICE_MODE_OPEN;
            else if (payload[0] == (uint8_t)DEVICE_MODE_CLOSED) d->mode = DEVICE_MODE_CLOSED;
            else return PROTO_REASON_BAD_PAYLOAD;
            return PROTO_REASON_OK;

        case PROTO_CMD_STOP:
            if (payload_len != 0u) return PROTO_REASON_BAD_PAYLOAD;
            // Zatrzymanie — ustaw prędkość na zero.
            d->speed = 0;
            return PROTO_REASON_OK;

        case PROTO_CMD_GET_STAT:
            if (payload_len != 0u) return PROTO_REASON_BAD_PAYLOAD;
            return PROTO_REASON_OK;

        default:
            return PROTO_REASON_UNKNOWN_CMD;
    }
}
// Zapisz wartość uint32_t w formacie little-endian.
static void wr_u32_le(uint8_t* out, uint32_t v){
    out[0] = (uint8_t)(v & 0xFFu);
    out[1] = (uint8_t)((v >> 8) & 0xFFu);
    out[2] = (uint8_t)((v >> 16) & 0xFFu);
    out[3] = (uint8_t)((v >> 24) & 0xFFu);
}
// Pakuje stan urządzenia oraz dane telemetryczne do bufora wyjściowego STAT.
uint8_t device_pack_stat(
    const device_t* d,
    uint32_t ticks,
    uint32_t rx_dropped,
    const proto_stats_t* pstats,
    uint8_t* out,
    uint8_t out_cap
){
    // Layout (little-endian):
    // speed:u8, mode:u8, last_error:u8, reserved:u8,
    // ticks:u32, rx_dropped:u32, broken_frames:u32, crc_errors:u32,
    // last_cmd_latency_ms:u32

    // Sprawdź, czy mamy wystarczająco miejsca w buforze wyjściowym.
    const uint8_t need = 4u + 4u * 5u;  // Wymagane 24 bajty łącznie
    if (out_cap < need) return 0;       // Brak miejsca. Zwróć 0, aby wskazać błąd. 

    out[0] = d->speed;
    out[1] = (uint8_t)d->mode;
    out[2] = (uint8_t)pstats->last_error;
    out[3] = 0;
    // Używamy funkcji pomocniczej do zapisu little-endian.
    wr_u32_le(&out[4], ticks);
    wr_u32_le(&out[8], rx_dropped);
    wr_u32_le(&out[12], pstats->broken_frames);
    wr_u32_le(&out[16], pstats->crc_errors);
    wr_u32_le(&out[20], pstats->last_cmd_latency_ms);

    return need;
}
