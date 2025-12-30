# REPORT

## Konfiguracja
- RB RX/TX: `RB_SIZE=128`, polityka overflow: **drop-new** (licznik `rx_dropped` w `rb_t.dropped`).
- Protokół: `STX|LEN|CMD|PAYLOAD|CRC` (CRC-8 poly `0x07`, init `0x00`).
- Timeouty: byte `20ms`, frame `200ms` (czas symulowany jako `ticks * ms_per_tick`).

## 1) Funkcjonalne (SET/MODE/GET/STOP)
Przykładowy wycinek logów:

```
INFO: READY
STATS: ticks=0 rx_dropped=0 broken_frames=0 crc_errors=0 last_cmd_latency=0ms

INFO: inject cmd=SET_SPEED(0x01) bytes=5
RX: cmd=SET_SPEED(0x01) payload_len=1 crc=OK
EVT: ACK
TX: 02 02 80 01 67
STATS: ticks=5 rx_dropped=0 broken_frames=0 crc_errors=0 last_cmd_latency=0ms

INFO: inject cmd=MODE(0x02) bytes=5
RX: cmd=MODE(0x02) payload_len=1 crc=OK
EVT: ACK
TX: 02 02 80 02 6E
STATS: ticks=10 rx_dropped=0 broken_frames=0 crc_errors=0 last_cmd_latency=0ms

INFO: inject cmd=GET_STAT(0x04) bytes=4
RX: cmd=GET_STAT(0x04) payload_len=0 crc=OK
EVT: STAT
TX: 02 19 82 50 01 00 00 ... <payload> ... <crc>
STATS: ticks=15 rx_dropped=0 broken_frames=0 crc_errors=0 last_cmd_latency=0ms

INFO: inject cmd=STOP(0x03) bytes=4
RX: cmd=STOP(0x03) payload_len=0 crc=OK
EVT: ACK
TX: 02 02 80 03 69
STATS: ticks=20 rx_dropped=0 broken_frames=0 crc_errors=0 last_cmd_latency=0ms
```

Wnioski:
- Komendy zmieniają stan urządzenia (speed/mode), a odpowiedzi są generowane nieblokująco.
- `GET STAT` zwraca ramkę `STAT` z telemetrią i stanem.

## 2) Błędne (CRC/LEN/UNKNOWN/niepełna ramka)
Przykładowy wycinek logów:

```
INFO: inject cmd=SET_SPEED(0x01) bytes=5 (CRC BAD)
ERR: reason=CRC(3) cmd=SET_SPEED(0x01)
TX: 02 03 81 01 03 46
STATS: ticks=30 rx_dropped=0 broken_frames=0 crc_errors=1 last_cmd_latency=0ms

INFO: inject cmd=CMD_UNKNOWN(0x55) bytes=4
RX: cmd=CMD_UNKNOWN(0x55) payload_len=0 crc=OK
EVT: NACK reason=UNKNOWN_CMD(4)
TX: 02 03 81 55 04 0B
STATS: ticks=35 rx_dropped=0 broken_frames=0 crc_errors=1 last_cmd_latency=0ms

INFO: inject partial bytes=3
ERR: reason=TIMEOUT(6) cmd=STOP(0x03)
STATS: ticks=245 rx_dropped=0 broken_frames=1 crc_errors=1 last_cmd_latency=0ms
```

Wnioski:
- Zły CRC → `crc_errors++` i `NACK:CRC`.
- Nieznany CMD → `NACK:UNKNOWN_CMD`.
- Niedomknięta ramka (timeout) → `broken_frames++` i brak wykonania komendy.

## 3) Obciążeniowe (burst ≥ 200 ramek)
Scenariusz: wysłanie 200 ramek `SET_SPEED` bez przerw czasowych (przepełnienie RX).

Wycinek logów:

```
=== 3) Obciążeniowe (burst >= 200 ramek) ===
INFO: inject cmd=GET_STAT(0x04) bytes=4
RX: cmd=GET_STAT(0x04) payload_len=0 crc=OK
EVT: STAT
TX: 02 19 82 ...
STATS: ticks=455 rx_dropped=873 broken_frames=2 crc_errors=1 last_cmd_latency=0ms
```

Wnioski:
- Występuje istotne `rx_dropped` przy burście (zgodnie z `RB_SIZE=128` i polityką drop-new).
- Część ramek jest uszkodzona wskutek utraty bajtów → rosną liczniki `broken_frames` / `crc_errors`.
- System pozostaje nieblokujący: `shell_tick()` dalej przetwarza kolejkę, a `GET STAT` raportuje telemetrię.
