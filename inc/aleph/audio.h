#ifndef ALEPH_AUDIO_H
#define ALEPH_AUDIO_H

void audio_init();

void audio_set_file_index(size_t index);
size_t audio_get_file_index();
float *audio_get_file_data(size_t *len);
void audio_stop();

void audio_set_file_repeat(size_t start, size_t end);
void audio_disable_file_repeat();

void audio_play();
void audio_pause();

typedef int Slice_Id;

Slice_Id audio_slice_begin(size_t start, size_t end, bool loop);
void audio_slice_end(Slice_Id id);
void audio_slice_set_index(Slice_Id id, size_t index);
void audio_slice_play(Slice_Id id);
void audio_slice_stop(Slice_Id id);

#endif /* ALEPH_AUDIO_H */