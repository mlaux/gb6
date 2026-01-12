#ifndef _AUDIO_MAC_H
#define _AUDIO_MAC_H

struct audio;

int audio_mac_available(void);
int audio_mac_init(struct audio *audio);
void audio_mac_start(void);
void audio_mac_stop(void);
void audio_mac_pump(void);
void audio_mac_shutdown(void);

#endif
