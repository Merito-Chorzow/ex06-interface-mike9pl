#include "protocol.h"
#include <string.h>

// Aktualizuje CRC-8 z polinomem 0x07 (używane do walidacji ramek).
static uint8_t crc8_update(uint8_t crc, uint8_t data){
    crc ^= data;
    for (unsigned i = 0; i < 8; i++){
        crc = (crc & 0x80u) ? (uint8_t)((crc << 1) ^ 0x07u) : (uint8_t)(crc << 1);
    }
    return crc;
}

// Oblicza CRC-8 po bajcie LEN oraz po wszystkich bajtach danych.
static uint8_t crc8_compute(uint8_t len, const uint8_t* data /* LEN bytes */){
    uint8_t crc = 0;
    crc = crc8_update(crc, len);
    for (uint8_t i = 0; i < len; i++) crc = crc8_update(crc, data[i]);
    return crc;
}

// Resetuje stan parsera protokołu do stanu początkowego.
static void proto_reset(proto_t* p){
    p->state = PROTO_FSM_IDLE;
    p->len = 0;
    p->data_i = 0;
    p->crc = 0;
    p->frame_start_ms = 0;
    p->last_byte_ms = 0;
}
// Inicjalizacja struktury protokołu.
void proto_init(proto_t* p, rb_t* rx, rb_t* tx){
    memset(p, 0, sizeof(*p));
    p->rx = rx;
    p->tx = tx;
    proto_reset(p);
}

// Zanotuj ostatni błąd w statystykach (nie resetuje FSM).
static void proto_note_error(proto_t* p, proto_reason_t reason){
    p->stats.last_error = reason;
}

// Obsługa timeoutu: zwiększa statystyki oraz wywołuje callback błędu.
static void proto_on_timeout(proto_t* p, proto_on_err_fn on_err, void* ctx){
    p->stats.broken_frames++;
    p->stats.frame_timeouts++;
    proto_note_error(p, PROTO_REASON_TIMEOUT);
    uint8_t cmd = (p->data_i > 0) ? p->data[0] : 0;
    if (on_err) on_err(ctx, PROTO_REASON_TIMEOUT, cmd);
    proto_reset(p);
}
// Sprawdza, czy bufor TX może pomieścić ramkę o podanym rozmiarze payload.
static int tx_can_fit_frame(proto_t* p, uint8_t payload_len){
    // total = STX(1)+LEN(1)+CMD(1)+PAYLOAD+CRC(1)
    size_t need = (size_t)(1u + 1u + 1u + payload_len + 1u);
    return rb_free(p->tx) >= need;
}
// Nieblokująca wysyłka ramki. Zwraca 1 w przypadku sukcesu, 0 jeśli bufor TX nie mógł pomieścić całej ramki (częściowa ramka NIE jest wysyłana).
int proto_send(proto_t* p, uint8_t cmd, const uint8_t* payload, uint8_t payload_len){
    if (payload_len > PROTO_MAX_PAYLOAD) return 0;
    if (!tx_can_fit_frame(p, payload_len)) return 0;

    uint8_t len = (uint8_t)(1u + payload_len);
    uint8_t data[1u + PROTO_MAX_PAYLOAD];
    data[0] = cmd;
    if (payload_len) memcpy(&data[1], payload, payload_len);
    uint8_t crc = crc8_compute(len, data);

    // Kolejność: STX | LEN | CMD | PAYLOAD | CRC
    (void)rb_put(p->tx, PROTO_STX);
    (void)rb_put(p->tx, len);
    for (uint8_t i = 0; i < len; i++) (void)rb_put(p->tx, data[i]);
    (void)rb_put(p->tx, crc);
    return 1;
}
// Nieblokująca wysyłka ACK dla podanej oryginalnej komendy.
int proto_send_ack(proto_t* p, uint8_t orig_cmd){
    uint8_t pl[1] = { orig_cmd };
    return proto_send(p, PROTO_CMD_ACK, pl, 1);
}
// Nieblokująca wysyłka NACK dla podanej oryginalnej komendy i powodu błędu.
int proto_send_nack(proto_t* p, uint8_t orig_cmd, proto_reason_t reason){
    uint8_t pl[2] = { orig_cmd, (uint8_t)reason };
    return proto_send(p, PROTO_CMD_NACK, pl, 2);
}
// Dostarcza poprawnie zdekodowaną wiadomość do callbacka.
static void proto_deliver_msg(proto_t* p, proto_on_msg_fn on_msg, void* ctx, uint32_t rx_start_ms, uint32_t rx_end_ms){
    proto_msg_t msg;
    msg.cmd = p->data[0];
    msg.payload_len = (uint8_t)(p->len - 1u);
    if (msg.payload_len) memcpy(msg.payload, &p->data[1], msg.payload_len);

    if (on_msg) on_msg(ctx, &msg, rx_start_ms, rx_end_ms);
}
// Protokół: obsługa timeoutów oraz parsowanie bajtów z bufora RX.
void proto_poll(proto_t* p, uint32_t now_ms, proto_on_msg_fn on_msg, proto_on_err_fn on_err, void* ctx){
    // Polling parsera: obsługa timeoutów oraz parsowanie bajtów z bufora RX.
    if (p->state != PROTO_FSM_IDLE){
        if ((now_ms - p->last_byte_ms) > PROTO_BYTE_TIMEOUT_MS){
            proto_on_timeout(p, on_err, ctx);
        } else if ((now_ms - p->frame_start_ms) > PROTO_FRAME_TIMEOUT_MS){
            proto_on_timeout(p, on_err, ctx);
        }
    }
    // Przetwarzanie bajtów z bufora RX.
    uint8_t b;
    while (rb_get(p->rx, &b)){
        if (p->state == PROTO_FSM_IDLE){
            if (b == PROTO_STX){
                p->state = PROTO_FSM_LEN;
                p->frame_start_ms = now_ms;
                p->last_byte_ms = now_ms;
                p->crc = 0;
            }
            continue;
        }

        // W każdym innym stanie aktualizujemy czas ostatniego bajtu.
        p->last_byte_ms = now_ms;
        // Przetwarzanie stanów FSM parsera.
        if (p->state == PROTO_FSM_LEN){
            p->len = b;
            if (p->len < 1u || p->len > (uint8_t)(1u + PROTO_MAX_PAYLOAD)){
                p->stats.broken_frames++;
                proto_note_error(p, PROTO_REASON_BAD_LEN);
                if (on_err) on_err(ctx, PROTO_REASON_BAD_LEN, 0);
                proto_reset(p);
                continue;
            }
            p->data_i = 0;
            p->state = PROTO_FSM_DATA;
            continue;
        }
        // Dane CMD + PAYLOAD
        if (p->state == PROTO_FSM_DATA){
            p->data[p->data_i++] = b;
            if (p->data_i >= p->len){
                p->state = PROTO_FSM_CRC;
            }
            continue;
        }
        // CRC
        if (p->state == PROTO_FSM_CRC){
            uint32_t rx_start_ms = p->frame_start_ms;
            uint32_t rx_end_ms = now_ms;

            uint8_t expected = crc8_compute(p->len, p->data);
            if (b != expected){
                p->stats.crc_errors++;
                proto_note_error(p, PROTO_REASON_CRC);
                if (on_err) on_err(ctx, PROTO_REASON_CRC, p->data[0]);
                // Best-effort: NACK with cmd we did parse.
                (void)proto_send_nack(p, p->data[0], PROTO_REASON_CRC);
                proto_reset(p);
                continue;
            }

            // Poprawna ramka
            proto_deliver_msg(p, on_msg, ctx, rx_start_ms, rx_end_ms);
            proto_reset(p);
            continue;
        }

        // Nieoczekiwany stan — reset parsera.
        proto_reset(p);
    }
}
// Pomocnicze funkcja do logów: zwracają nazwy komend.
const char* proto_cmd_name(uint8_t cmd){
    switch (cmd){
        case PROTO_CMD_SET_SPEED: return "SET_SPEED";
        case PROTO_CMD_SET_MODE:  return "MODE";
        case PROTO_CMD_STOP:      return "STOP";
        case PROTO_CMD_GET_STAT:  return "GET_STAT";
        case PROTO_CMD_ACK:       return "ACK";
        case PROTO_CMD_NACK:      return "NACK";
        case PROTO_CMD_STAT:      return "STAT";
        default:                  return "CMD_UNKNOWN";
    }
}
// Pomocnicze funkcja do logów: zwracają nazwy błędów.
const char* proto_reason_name(proto_reason_t reason){
    switch (reason){
        case PROTO_REASON_OK:          return "OK";
        case PROTO_REASON_BAD_STX:     return "BAD_STX";
        case PROTO_REASON_BAD_LEN:     return "BAD_LEN";
        case PROTO_REASON_CRC:         return "CRC";
        case PROTO_REASON_UNKNOWN_CMD: return "UNKNOWN_CMD";
        case PROTO_REASON_BAD_PAYLOAD: return "BAD_PAYLOAD";
        case PROTO_REASON_TIMEOUT:     return "TIMEOUT";
        default:                       return "REASON_UNKNOWN";
    }
}
