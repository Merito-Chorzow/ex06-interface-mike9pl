# Makefile — wspólny szablon (C11 + debug)
OS := $(shell uname -s)
EXE :=
ifeq ($(OS),Windows_NT)
  EXE := .exe
endif

CC      ?= gcc
CFLAGS  ?= -std=c11 -O0 -g -Wall -Wextra -Wpedantic -Isrc
LDFLAGS ?=
SRC     := $(wildcard src/*.c)
OUTDIR  := build
OUT     := $(OUTDIR)/app$(EXE)

all: $(OUT)

$(OUT): $(SRC)
	@mkdir -p $(OUTDIR)
	$(CC) $(CFLAGS) -o $@ $(SRC) $(LDFLAGS)

run: all
	./$(OUT)

clean:
	$(RM) -r $(OUTDIR)

.PHONY: all run clean
