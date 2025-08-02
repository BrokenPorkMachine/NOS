#include "server.h"
#include "audio.h"
#include "../../libc/libc.h"
#include "../../../kernel/drivers/Audio/audio.h"

void audio_server(ipc_queue_t *q, uint32_t self_id) {
    ipc_message_t msg;
    while (1) {
        if (ipc_receive(q, self_id, &msg) != 0)
            continue;
        if (msg.type == AUDIO_MSG_PLAY)
            audio_play_pcm((const int16_t*)msg.data, msg.arg1);
    }
}
