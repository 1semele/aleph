#ifndef ALEPH_DEFS_H
#define ALEPH_DEFS_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#define IGNORE(_var) ((void) (_var))

#define NEW(_type) ((calloc(1, sizeof(_type))))
#define NEW_ARR(_type, _len) ((calloc(_len, sizeof(_type))))
#define FREE(_ptr) free((void *) _ptr)

#define FAIL(_reason) _fail(_reason, __FILE__, __LINE__)
#define FAIL_FMT(_reason, ...) _fail(_reason, __FILE__, __LINE__, __VA_ARGS__)

#define LOG(_reason) _log(_reason, __FILE__, __LINE__)
#define LOG_FMT(_reason, ...) _log(_reason, __FILE__, __LINE__, __VA_ARGS__)

void _fail(const char *fmt, const char *file, int line, ...);
void _log(const char *fmt, const char *file, int line, ...);

#endif /* ALEPH_DEFS_H */
