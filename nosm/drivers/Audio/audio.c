#include "audio.h"
#include "../IO/serial.h"
#include <stdint.h>
#include <stddef.h>

#define AUDIO_BUFFER_SAMPLES 4096

static int16_t audio_buffer[AUDIO_BUFFER_SAMPLES];
static size_t head = 0;
static size_t tail = 0;
static int initialized = 0;

static size_t buffer_free(void) {
    if (head >= tail)
        return AUDIO_BUFFER_SAMPLES - (head - tail) - 1;
    return tail - head - 1;
}

void audio_init(void) {
    head = tail = 0;
    initialized = 1;
    serial_puts("[audio] initialized\n");
}

void audio_play_pcm(const int16_t *data, size_t len) {
    if (!initialized || !data || !len)
        return;
    for (size_t i = 0; i < len; ++i) {
        if (buffer_free() == 0)
            break; // buffer full, drop remaining samples
        audio_buffer[head] = data[i];
        head = (head + 1) % AUDIO_BUFFER_SAMPLES;
    }
}
