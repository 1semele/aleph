#include <aleph/membuf.h>
#include <aleph/audio_file.h>

#include <stdio.h>

typedef struct {
    uint8_t riff[4];
    uint32_t size;
    uint8_t wave[4];
    uint8_t fmt[4];
    uint32_t fmtlen;
    uint16_t format;
    uint16_t chan_ct;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
    uint8_t data[4];
    uint32_t data_size;
} Wav_Header;

bool audio_file_load_wav(Audio_File *file, const char *path) {
    Membuf buf;
    if (!membuf_load(&buf, path)) {
        return false;
    }

    Wav_Header *hdr = (Wav_Header *) buf.data;

    file->nchannels = hdr->chan_ct;
    file->sample_t = SAMPLE_TYPE_F32;
    file->len = hdr->data_size / (hdr->bits_per_sample / 8);
    float *output_data = NEW_ARR(float, file->len);
    if (hdr->bits_per_sample == 16) {
        int16_t *data = (uint16_t *) (buf.data + sizeof(Wav_Header));
        for (size_t i = 0; i < file->len; i++) {
            float f = ((float) data[i]) / ((float) 32768);
            if (f > 1) f = 1.0;
            if (f < -1) f = -1.0;
            output_data[i] = f;
        }
    } else {
        printf("unknown bits per sample: %d\n", hdr->bits_per_sample);
        exit(EXIT_FAILURE);
    }

    file->data.f32 = output_data;

    membuf_free(&buf);

    return true;
}

void audio_file_free(Audio_File *file) {
    if (file->sample_t == SAMPLE_TYPE_F32) {
        FREE(file->data.f32);
    }
}
