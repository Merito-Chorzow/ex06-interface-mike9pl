#pragma once
#include <stdint.h>
#include <stddef.h>

#ifndef RB_SIZE
#define RB_SIZE 128
#endif

typedef struct {
    uint8_t q[RB_SIZE];
    size_t head, tail;   // head: write, tail: read
    size_t dropped;      // licznik utraconych bajtów
} rb_t;

void   rb_init(rb_t* r);
size_t rb_free(const rb_t* r);
size_t rb_count(const rb_t* r);
int    rb_put(rb_t* r, uint8_t b);     // 1=ok, 0=drop (domyślnie: odrzucamy nowe)
int    rb_get(rb_t* r, uint8_t* out);  // 1=ok, 0=empty
