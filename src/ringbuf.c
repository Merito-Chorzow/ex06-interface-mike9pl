#include "ringbuf.h"

void rb_init(rb_t* r){ r->head = r->tail = r->dropped = 0; }

size_t rb_count(const rb_t* r){
    return (r->head - r->tail) % RB_SIZE;
}

size_t rb_free(const rb_t* r){
    return RB_SIZE - 1 - rb_count(r); // jeden slot pusty dla rozróżnienia pełny/pusty
}

int rb_put(rb_t* r, uint8_t b){
    if (rb_free(r) == 0){
        r->dropped++;                 // polityka: odrzucamy NOWE bajty
        return 0;
    }
    r->q[r->head % RB_SIZE] = b;
    r->head = (r->head + 1) % RB_SIZE;
    return 1;
}

int rb_get(rb_t* r, uint8_t* out){
    if (rb_count(r) == 0) return 0;
    *out = r->q[r->tail % RB_SIZE];
    r->tail = (r->tail + 1) % RB_SIZE;
    return 1;
}
