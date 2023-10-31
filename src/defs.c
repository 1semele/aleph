#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

#include <aleph/defs.h>

void _fail(const char *fmt, const char *file, int line, ...) {
    va_list args;
    va_start(args, line);

    printf("LOG: %s:%d: ", file, line);
    vprintf(fmt, args);
    printf("\n");

    va_end(args);

    exit(EXIT_FAILURE);
}

void _log(const char *fmt, const char *file, int line, ...) {
    va_list args;
    va_start(args, line);

    printf("LOG: %s:%d: ", file, line);
    vprintf(fmt, args);
    printf("\n");

    va_end(args);
}