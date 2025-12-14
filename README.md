# Ćwiczenie 3 — I/O bez blokowania: ring buffer + mini‑shell

Cel: zaprojektować **nieblokujące I/O** z **pierścieniowym buforem (ring buffer)** i prostym **parserem komend**. Zrozumiesz polityki przepełnień i obsługę „strumieni” bez blokowania.

---

## Struktura
```
ex3_io_iot_c/
├─ .vscode/
│  ├─ tasks.json       # Build/Clean
│  └─ launch.json      # Debug
├─ src/
│  ├─ ringbuf.h, ringbuf.c   # bufor kołowy z licznikiem dropów
│  ├─ shell.h, shell.c       # mini-shell: set/get/stat/echo
│  └─ main.c                 # symulacja wejścia i tykanie shell_tick()
└─ Makefile
```

## Budowanie i uruchamianie
```bash
make
./build/app     # macOS/Linux
# .\build\app.exe  # Windows
```

Oczekiwany output: baner `READY`, odpowiedzi na `get`, `set 0.42`, `stat`, oraz zliczone przepełnienia po wysłaniu burstu komend.

---

## Zadania
1. **Wybierz i opisz politykę overflow** (domyślnie: odrzucamy *nowe* bajty i zwiększamy `dropped`). Możesz zmienić na „kasujemy najstarsze” — opisz konsekwencje.
2. Dodaj komendę `echo <text>` i pokaż, że RB nie miesza danych (test integralności).
3. Dodaj licznik „złamanych linii” (gdy przychodzi tylko fragment rozkazu przez overflow).
4. (Opcjonalnie) Dodaj `rate <N>` — co N ticków wypisz status.

**Raport:** `REPORT.md` — krótki opis polityki, zrzuty logów przed/po burst, wnioski.
