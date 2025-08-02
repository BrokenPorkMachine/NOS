# Audio Server

Provides a simple IPC interface for PCM audio playback. Clients send `AUDIO_MSG_PLAY` with raw samples and the server forwards them to the kernel audio driver.
