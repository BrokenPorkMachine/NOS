#pragma once
#include <stdint.h>
#include <stddef.h>

void audio_init(void);
void audio_play_pcm(const int16_t *data, size_t len);
