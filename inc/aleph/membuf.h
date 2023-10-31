#ifndef ALEPH_MEMBUF_H
#define ALEPH_MEMBUF_H

#include <aleph/defs.h>

typedef struct {
    const uint8_t *data;
    size_t len;
} Membuf;

bool membuf_load(Membuf *buf, const char *path);
void membuf_free(Membuf *buf);

#endif /* ALEPH_MEMBUF_H */