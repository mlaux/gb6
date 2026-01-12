/* Game Boy emulator for 68k Macs
   audio_mac.c - Sound Manager integration */
   
#include <Sound.h>
#include <Memory.h>
#include <Gestalt.h>
#include <Files.h>
#include <string.h>

#include "audio_mac.h"
#include "../src/audio.h"

// 2 frames worth of samples at 11127 Hz / 60 fps
#define BUFFER_SAMPLES 371
#define SAMPLE_RATE_FIXED 0x2b7745d1

static SndChannelPtr snd_channel;
static struct audio *g_audio;
static int audio_inited;

static int write_idx;   // where audio_mac_pump writes (main loop)
static int read_idx;    // where PlayNextBuffer reads (callback)
static int num_ready;   // buffers ready to play

typedef struct {
    Ptr samplePtr;
    unsigned long length;
    Fixed sampleRate;
    unsigned long loopStart;
    unsigned long loopEnd;
    unsigned char encode;
    unsigned char baseFrequency;
    unsigned char samples[BUFFER_SAMPLES];
} SimpleSoundBuffer;

static SimpleSoundBuffer sound_buffers[3];

// forward declaration
static pascal void SoundCallback(SndChannelPtr chan, SndCommand *cmd);

static int HasASC(void)
{
    long response;
    OSErr err;

    err = Gestalt(gestaltHardwareAttr, &response);
    if (err == noErr && (response & (1L << gestaltHasASC)))
        return 1;

    return 0;
}

int audio_mac_available(void)
{
    return HasASC();
}

// play the next pre-generated buffer (called from callback)
static void PlayNextBuffer(void)
{
    SndCommand cmd;

    if (!audio_inited || !snd_channel)
        return;

    if (num_ready == 0)
        return;

    // play the ready buffer
    cmd.cmd = bufferCmd;
    cmd.param1 = 0;
    cmd.param2 = (long) &sound_buffers[read_idx];
    SndDoImmediate(snd_channel, &cmd);

    read_idx = (read_idx + 1) % 3;
    num_ready--;

    // queue callback for when this buffer finishes
    cmd.cmd = callBackCmd;
    cmd.param1 = 0;
    cmd.param2 = 0;
    SndDoCommand(snd_channel, &cmd, true);
}

// callback - called at interrupt time when buffer finishes playing
static pascal void SoundCallback(SndChannelPtr chan, SndCommand *cmd)
{
    PlayNextBuffer();
}

int audio_mac_init(struct audio *audio)
{
    OSErr err;
    int k;

    g_audio = audio;

    snd_channel = NULL;
    err = SndNewChannel(&snd_channel, sampledSynth, initMono,
                (SndCallBackProcPtr) SoundCallback);
    if (err != noErr) {
        return 0;
    }

    for (k = 0; k < 3; k++) {
        memset(&sound_buffers[k], 0, sizeof(sound_buffers[k]));

        sound_buffers[k].samplePtr = (Ptr)sound_buffers[k].samples;
        sound_buffers[k].length = BUFFER_SAMPLES;
        sound_buffers[k].sampleRate = SAMPLE_RATE_FIXED;
        sound_buffers[k].loopStart = 0;
        sound_buffers[k].loopEnd = 0;
        sound_buffers[k].encode = stdSH;
        sound_buffers[k].baseFrequency = 60;

        // init samples to silence
        memset(sound_buffers[k].samples, 0x80, BUFFER_SAMPLES);
    }

    write_idx = 0;
    read_idx = 0;
    num_ready = 0;
    audio_inited = 1;

    return 1;
}

void audio_mac_pump(void)
{
    unsigned char *p;
    int k;

    if (!audio_inited || !g_audio)
        return;

    // don't get too far ahead
    if (num_ready >= 2)
        return;

    p = sound_buffers[write_idx].samples;
    audio_generate(g_audio, (s8 *) p, BUFFER_SAMPLES);

    // convert signed to unsigned
    for (k = 0; k < BUFFER_SAMPLES; k++) {
        p[k] ^= 0x80;
    }

    write_idx = (write_idx + 1) % 3;
    num_ready++;
}

void audio_mac_start(void)
{
    // pre-generate a couple buffers
    audio_mac_pump();
    audio_mac_pump();

    // kick off playback - callbacks will keep it going
    PlayNextBuffer();
}

void audio_mac_stop(void)
{
    SndCommand cmd;

    if (!snd_channel)
        return;

    cmd.cmd = quietCmd;
    cmd.param1 = 0;
    cmd.param2 = 0;
    SndDoImmediate(snd_channel, &cmd);

    cmd.cmd = flushCmd;
    SndDoImmediate(snd_channel, &cmd);
}

void audio_mac_shutdown(void)
{
    audio_mac_stop();

    if (snd_channel) {
        SndDisposeChannel(snd_channel, true);
        snd_channel = NULL;
    }

    g_audio = NULL;
    audio_inited = 0;
}
