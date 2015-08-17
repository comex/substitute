#include "circle.h"

void cqueue_realloc_internal(struct cqueue_internal *ci,
                             size_t new_capacity_bytes) {
    char *storage = (char *) malloc(new_capacity_bytes);
    char *new_write_ptr;
    if (c->write_ptr >= c->read_ptr) {
        size_t diff = c->write_ptr - c->read_ptr;
        memcpy(storage, c->read_ptr, diff);
        new_write_ptr = storage + diff;
    } else {
        size_t diff1 = c->end - c->read_ptr;
        size_t diff2 = c->write_ptr - c->start;
        memcpy(storage, c->read_ptr, diff1);
        char *tmp = storage + diff1;
        memcpy(tmp, c->start, diff2);
        new_write_ptr = tmp + diff2;
    }
    c->start = storage;
    c->end = storage + new_capacity_bytes;
    c->read_ptr = storage;
    c->write_ptr = new_write_ptr;
}
