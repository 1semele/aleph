#include <stdio.h>

#include <aleph/membuf.h>

bool membuf_load(Membuf *buf, const char *path) {
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        buf->data = NULL;
        buf->len = 0;
        return false;
    }

    fseek(f, 0, SEEK_END);
    size_t len = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *data = NEW_ARR(uint8_t, len + 1);
    fread(data, 1, len, f);
    fclose(f);
    data[len] = '\0';

    buf->data = data;
    buf->len = len;

    return true ;
}

void membuf_free(Membuf *buf) {
    if (buf) {
        FREE(buf->data);
        buf->data = NULL;
        buf->len = 0;
    }
}