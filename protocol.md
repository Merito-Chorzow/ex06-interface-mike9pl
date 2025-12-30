# PROTOKÓŁ RAMEK — SPECYFIKACJA #

Format ramki
------------
Każda ramka ma postać:

STX | LEN | CMD | PAYLOAD | CRC

- STX: 1 bajt — stała wartość 0x02
- LEN: 1 bajt — liczba bajtów od pola CMD do końca PAYLOAD (zatem LEN >= 1)
- CMD: 1 bajt — kod komendy
- PAYLOAD: (LEN-1) bajtów — dane zależne od komendy
- CRC: 1 bajt — CRC-8 obliczone po bajcie LEN i kolejnych LEN bajtach (CMD+PAYLOAD)

CRC
---
- CRC-8, wielomian 0x07, init 0x00
- CRC obejmuje: LEN + CMD + PAYLOAD
- STX nie jest uwzględniany w CRC

Kody CMD
-------------------
- 0x01 — SET_SPEED
  - Payload: `speed:u8` Implementacja stosuje clamp do zakresu (0..100).
  - Odp.: ACK lub NACK:BAD_PAYLOAD

- 0x02 — SET_MODE
  - Payload: `mode:u8` (0=OPEN, 1=CLOSED)
  - Odp.: ACK lub NACK:BAD_PAYLOAD

- 0x03 — STOP
  - Payload: brak
  - Efekt: przejście do stanu bezpiecznego (np. speed=0)
  - Odp.: ACK

- 0x04 — GET_STAT
  - Payload: brak
  - Odp.: STAT (z telemetrią i stanem)

Kody odpowiedzi
----------------
- 0x80 — ACK
  - Payload: 1 bajt — oryginalny `orig_cmd`
- 0x81 — NACK
  - Payload: 2 bajty — `orig_cmd`, `reason`
- 0x82 — STAT
  - Payload: struktura telemetrii

Zwracany status
---------------------
- 0 — OK
- 1 — BAD_STX
- 2 — BAD_LEN
- 3 — CRC
- 4 — UNKNOWN_CMD
- 5 — BAD_PAYLOAD
- 6 — TIMEOUT

Timeouty i timing
------------------
Implementacja symulowana w tickach, sugerowane wartości są konfigurowalne:
- timeout bajtu: 20 ms
- timeout ramki: 200 ms

Jeśli podczas składania ramki przekroczono którykolwiek timeout → ramka porzucona, licznik `broken_frames` jest zwiększany i wysłany NACK:TIMEOUT.

Telemetria / STAT payload
-------------------------
Layout (little-endian):
- `speed:u8`
- `mode:u8`
- `last_error:u8`
- `reserved:u8`
- `ticks:u32`
- `rx_dropped:u32`    -- utracone bajty (ring buffer overflow)
- `broken_frames:u32`
- `crc_errors:u32`
- `last_cmd_latency_ms:u32`