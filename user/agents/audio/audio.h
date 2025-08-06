#pragma once
#include <stdint.h>
#include "../../../kernel/IPC/ipc.h"

enum {
    AUDIO_MSG_PLAY = 1
};

int audio_play(ipc_queue_t *q, uint32_t server, const int16_t *data, size_t samples);
