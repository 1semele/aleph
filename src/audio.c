#include <SDL.h>

#include <portaudio.h>

#include <aleph/defs.h>
#include <aleph/audio.h>
#include <aleph/audio_file.h>

#define SAMPLE_RATE 44100
#define FRAMES_PER_BUFFER 64
#define SAMPLES_PER_BUFFER (FRAMES_PER_BUFFER * 2)

typedef enum {
    ADSR_RISING,
    ADSR_DECAYING,
    ADSR_SUSTAINED,
    ADSR_RELEASED,
} ADSR_State;

typedef struct Slice {
    size_t start, end;
    size_t index;
    bool loop;
    struct Slice *next;
    bool playing;

    size_t a, d, r;
    float s;
    ADSR_State adsr;
    size_t adsr_index;
} Slice;

#define MAX_DELAY_SECONDS 5
#define MAX_DELAY_SAMPLES ((SAMPLE_RATE * 2) * MAX_DELAY_SECONDS)

typedef struct {
    int output; /* Index of the its output channel. -1 for direct output. */
    float data[SAMPLES_PER_BUFFER];
    float delay_data[MAX_DELAY_SAMPLES];
    size_t delay_idx, delay_len;
    size_t data_idx;
    float delay_damp;
} Channel;

#define MAX_SLICES 64

#define NUM_SENDS 10

#define NUM_CHANNELS (MAX_SLICES + NUM_SENDS)

struct {
    bool ready, stop, has_stopped;
    PaStream *stream;
    Audio_File file;
    SDL_Thread *thread;
    size_t repeat_start, repeat_end;
    Slice slices[MAX_SLICES];
    Slice *cur_slice, *free_slice;

    Channel chans[NUM_CHANNELS];
} audio_sys;

static int audio_thread_callback(void *ud);
static void channel_get_samples(Channel *chan, float *l, float *r);
static float get_adsr_scale(Slice *slice);
static int pa_callback(const void *in_buf, void *out_buf, 
    unsigned long frames_per_buffer, const PaStreamCallbackTimeInfo *time_info, 
    PaStreamCallbackFlags status_flags, void *ud);
static void pa_finished_callback(void *ud);

void audio_init() {
    if (!audio_file_load_wav(&audio_sys.file, "test.wav")) {
        FAIL("failed to open audio file: 'test.wav'");
    }

    Slice *free_slices = NULL;
    for (int i = 0; i < MAX_SLICES; i++) {
        audio_sys.slices[i].next = free_slices;
        free_slices = &audio_sys.slices[i];
    }

    for (int i = 0; i < NUM_CHANNELS; i++) {
        Channel *chan = &audio_sys.chans[i];
        chan->output = -1;
        chan->data_idx = 0;
        chan->delay_damp = 0.2;
        memset(chan->delay_data, 0, sizeof(chan->delay_data));
        chan->delay_len = ((float) SAMPLE_RATE) / 2; 
    }

    audio_sys.free_slice = free_slices;
    audio_sys.cur_slice = NULL;

    audio_sys.repeat_start = 0;
    audio_sys.repeat_end = audio_sys.file.len;

    audio_sys.ready = false;
    audio_sys.stop = false;
    audio_sys.thread = SDL_CreateThread(audio_thread_callback, "audio", NULL);

    while (!audio_sys.ready) {
        SDL_Delay(100);
    }

    LOG("audio thread launched");
}

void audio_stop() {
    audio_sys.has_stopped = false;
    audio_sys.stop = true;
    while (!audio_sys.has_stopped) {
        SDL_Delay(100);
    }

    LOG("audio thread stopped");

}

Slice_Id audio_slice_begin(size_t start, size_t end, bool loop) {
    Slice *next = audio_sys.free_slice;
    if (!next) {
        FAIL("max slices reached");
    }

    next->playing = false;
    next->start = start;
    next->end = end;
    next->loop = loop;
    audio_sys.free_slice = next->next;
    next->next = audio_sys.cur_slice;
    audio_sys.cur_slice = next;
    next->index = start;
    next->a = SAMPLE_RATE / 2;
    next->d = 0;
    next->s = 1.0f;
    next->r = SAMPLE_RATE / 2;


    return next - audio_sys.slices;
}

void audio_slice_end(Slice_Id id) {
    Slice *iter = audio_sys.cur_slice;
    if ((iter - audio_sys.slices) == id) {
        Slice *killed_slice = iter;
        audio_sys.cur_slice = iter->next;
        killed_slice = audio_sys.free_slice;
        audio_sys.free_slice = killed_slice;
        return;
    }

    while (iter && (iter->next - audio_sys.slices) != id) {
        iter = iter->next;
    }

    if (!iter) {
        FAIL("slice killed twice");
    } else {
        Slice *killed_slice = iter->next;
        iter->next = iter->next->next;
        killed_slice = audio_sys.free_slice;
        audio_sys.free_slice = killed_slice;
    }
}

void audio_slice_play(Slice_Id id) {
    Slice *slice = &audio_sys.slices[id];
    slice->playing = true;
    slice->adsr = ADSR_RISING;
    slice->adsr_index = 0;
}

void audio_slice_stop(Slice_Id id) {
    Slice *slice = &audio_sys.slices[id];
    slice->adsr = ADSR_RELEASED;
    slice->adsr_index = 0;
}

void audio_slice_set_index(Slice_Id id, size_t index) {
    Slice *slice = &audio_sys.slices[id];
    slice->index = slice->start + index;
}

float *audio_get_file_data(size_t *len) {
    *len = audio_sys.file.len;
    return audio_sys.file.data.f32;
}

static int audio_thread_callback(void *ud) {
    IGNORE(ud);

    Pa_Initialize();
    PaStreamParameters out_params = {
        .device = Pa_GetDefaultOutputDevice(),
        .channelCount = 2,
        .sampleFormat = paFloat32,
    };

    out_params.suggestedLatency = 
        Pa_GetDeviceInfo(out_params.device)->defaultLowOutputLatency;
    Pa_OpenStream(&audio_sys.stream, NULL, &out_params, SAMPLE_RATE, 
        FRAMES_PER_BUFFER, paClipOff, pa_callback, NULL);

    Pa_SetStreamFinishedCallback(audio_sys.stream, pa_finished_callback);
    Pa_StartStream(audio_sys.stream);

    audio_sys.ready = true;

    while (!audio_sys.stop) {
        Pa_Sleep(100);
    }

    Pa_StopStream(audio_sys.stream);
    Pa_CloseStream(audio_sys.stream);
    Pa_Terminate();

    audio_sys.has_stopped = true;

    return 1;
}

static int pa_callback(const void *in_buf, void *out_buf, 
    unsigned long frames_per_buffer, const PaStreamCallbackTimeInfo *time_info, 
    PaStreamCallbackFlags status_flags, void *ud) {

    IGNORE(ud);
    IGNORE(in_buf);
    IGNORE(time_info);
    IGNORE(status_flags);

    float *out = (float *) out_buf;

    for (unsigned long i = 0; i < frames_per_buffer; i++) {
        out[i * 2] = 0.0f;
        out[i * 2 + 1] = 0.0f;
    }

    for (int i = 0; i < NUM_CHANNELS; i++) {
        Channel *chan = &audio_sys.chans[i];
        for (int j = 0; j < SAMPLES_PER_BUFFER; j++) {
            chan->data[j] = 0.0f;
        }
    }

    Slice *iter = audio_sys.cur_slice;
    while (iter) {
        if (iter->playing) {
            Channel *chan = &audio_sys.chans[iter - audio_sys.slices];
            for (unsigned long i = 0; i < frames_per_buffer; i++) {
                if (iter->index > iter->end) {
                    if (iter->loop) {
                        iter->index = iter->start;
                    } else {
                        iter->playing = false;
                        goto done;
                    }
                }

                float adsr_scale = get_adsr_scale(iter);

                chan->data[i * 2] += audio_sys.file.data.f32[iter->index * 2] * adsr_scale;
                chan->data[i * 2 + 1] += audio_sys.file.data.f32[iter->index * 2 + 1] * adsr_scale;
                iter->index++;
            }
        }
done:

        iter = iter->next;
    }

    for (int i = 0; i < NUM_CHANNELS; i++) {
        Channel *chan = &audio_sys.chans[i];
        if (chan->output == -1) {
            for (unsigned long j = 0; j < frames_per_buffer; j++) {
                float left, right;
                channel_get_samples(chan, &left, &right);
                out[j * 2] += left;
                out[j * 2 + 1] += right;
            }
        } else {
            Channel *dst = &audio_sys.chans[chan->output];
            for (unsigned long j = 0; j < frames_per_buffer; j++) {
                dst->data[j * 2] += chan->data[j * 2];
                dst->data[j * 2 + 1] += chan->data[j * 2 + 1];
            }
        }

        chan->data_idx = 0;
    }

    return paContinue;
}

static void pa_finished_callback(void *ud) {
    IGNORE(ud);
}

static void channel_get_samples(Channel *chan, float *l_out, float *r_out) {
    float input_l = chan->data[chan->data_idx * 2];
    float input_r = chan->data[chan->data_idx * 2 + 1];

    float delay_l = chan->delay_data[chan->delay_idx * 2] * chan->delay_damp;
    float delay_r = chan->delay_data[chan->delay_idx * 2 + 1] * chan->delay_damp;

    float total_l = input_l + delay_l;
    float total_r = input_r + delay_r;

    chan->delay_data[chan->delay_idx * 2] = total_l;
    chan->delay_data[chan->delay_idx * 2 + 1] = total_r;

    if (chan->delay_idx > chan->delay_len) {
        chan->delay_idx = 0;
    } else {
        chan->delay_idx++;
    }

    chan->data_idx++;

    *l_out = total_l;
    *r_out = total_r;
}

static float get_adsr_scale(Slice *slice) {
    float scale;
start:
    switch (slice->adsr) {
        case ADSR_RISING:
            if (slice->adsr_index >= slice->a) {
                slice->adsr = ADSR_DECAYING;
                goto start;
            } else {
                scale = ((float) slice->adsr_index) / slice->a;
            }
            break;
        case ADSR_DECAYING:
            if (slice->adsr_index >= (slice->a + slice->d)) {
                slice->adsr = ADSR_SUSTAINED;
                goto start;
            } else {
                size_t decay_index = slice->adsr_index - slice->a;
                scale = 1.0 - (1.0 - slice->s) * ((float) decay_index) / slice->d;
            }
            break;
        case ADSR_SUSTAINED:
            /* Skip incrementing the ADSR index. */
            return slice->s;
        case ADSR_RELEASED:
            if (slice->adsr_index >= slice->r) {
                slice->playing = false;
                return 0.0f;
            } else {
                scale = ((float) slice->adsr_index) / slice->r;
            }
            break;
    }

    slice->adsr_index++;
    return scale;
}