#include "audio.h"
#include "../../libc/libc.h"

int audio_play(ipc_queue_t *q, uint32_t server, const int16_t *data, size_t samples) {
    ipc_message_t msg;
    if (samples * sizeof(int16_t) > IPC_MSG_DATA_MAX)
        return -1;
    memset(&msg, 0, sizeof(msg));
    msg.type = AUDIO_MSG_PLAY;
    msg.arg1 = samples;
    msg.len  = samples * sizeof(int16_t);
    memcpy(msg.data, data, msg.len);
    return ipc_send(q, server, &msg);
}
