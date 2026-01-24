#ifndef _AUDIO_MAC_H
#define _AUDIO_MAC_H

struct audio;

int audio_mac_available(void);
int audio_mac_init(struct audio *audio);
void audio_mac_start(void);
void audio_mac_stop(void);
void audio_mac_shutdown(void);

// called from dmg_sync_hw to generate samples synchronized to GB execution
void audio_mac_sync(int cycles);

// block if ring buffer has more than ~1 frame of audio queued (for frame limiting)
void audio_mac_wait_if_ahead(void);

#endif
