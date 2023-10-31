#ifndef ALEPH_AUDIO_FILE_H
#define ALEPH_AUDIO_FILE_H

#include <aleph/defs.h>

typedef enum {
    SAMPLE_TYPE_F32,
} Sample_Type;

typedef struct {
    Sample_Type sample_t;
    int nchannels;
    union {
        float *f32;
    } data;
    size_t len; /* In samples. */
} Audio_File;

bool audio_file_load_wav(Audio_File *file, const char *path);

void audio_file_free(Audio_File *file);

#endif /* ALEPH_AUDIO_FILE_H */