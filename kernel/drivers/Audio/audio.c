#include "audio.h"
#include "../IO/serial.h"

void audio_init(void) {
    /* Stub initialization for audio hardware */
    serial_puts("Audio: initialization stub\n");
}

void audio_play_pcm(const int16_t *data, size_t len) {
    /* Placeholder for PCM playback */
    (void)data;
    (void)len;
}
