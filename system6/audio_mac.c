/* Game Boy emulator for 68k Macs
   audio_mac.c - Sound Manager integration using SndPlayDoubleBuffer */

#include <Sound.h>
#include <Memory.h>
#include <Gestalt.h>
#include <string.h>

#include "audio_mac.h"
#include "../src/audio.h"

// 1 frame worth of samples at 11127 Hz / 60 fps
#define BUFFER_SAMPLES 185
#define SAMPLE_RATE_FIXED 0x2b7745d1

static SndChannelPtr snd_channel;
static struct audio *g_audio;
static int audio_inited;

// Double buffer structure - matches SndDoubleBuffer layout with our sample data
typedef struct {
    long dbNumFrames;
    long dbFlags;
    long dbUserInfo[2];
    unsigned char dbSoundData[BUFFER_SAMPLES];
} MyDoubleBuffer;

static SndDoubleBufferHeader dbl_header;
static MyDoubleBuffer dbl_buffers[2];

// forward declaration
static pascal void DoubleBackProc(SndChannelPtr chan, SndDoubleBufferPtr buf);

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

// generate audio into a buffer
static void FillBuffer(unsigned char *p)
{
    int k;

    if (!g_audio) {
        // silence if no audio context
        memset(p, 0x80, BUFFER_SAMPLES);
        return;
    }

    audio_generate(g_audio, (s8 *)p, BUFFER_SAMPLES);

    // convert signed to unsigned
    for (k = 0; k < BUFFER_SAMPLES; k++) {
        p[k] ^= 0x80;
    }
}

// doubleback procedure - called at interrupt time when a buffer is exhausted
static pascal void DoubleBackProc(SndChannelPtr chan, SndDoubleBufferPtr buf)
{
    FillBuffer(buf->dbSoundData);
    buf->dbNumFrames = BUFFER_SAMPLES;
    buf->dbFlags |= dbBufferReady;
}

int audio_mac_init(struct audio *audio)
{
    OSErr err;
    int k;

    g_audio = audio;

    snd_channel = NULL;
    err = SndNewChannel(&snd_channel, sampledSynth, initMono, NULL);
    if (err != noErr) {
        return 0;
    }

    // initialize double buffers
    for (k = 0; k < 2; k++) {
        memset(&dbl_buffers[k], 0, sizeof(dbl_buffers[k]));
        memset(dbl_buffers[k].dbSoundData, 0x80, BUFFER_SAMPLES);
        dbl_buffers[k].dbNumFrames = BUFFER_SAMPLES;
        dbl_buffers[k].dbFlags = dbBufferReady;
    }

    // initialize double buffer header
    memset(&dbl_header, 0, sizeof(dbl_header));
    dbl_header.dbhNumChannels = 1;
    dbl_header.dbhSampleSize = 8;
    dbl_header.dbhCompressionID = 0;
    dbl_header.dbhPacketSize = 0;
    dbl_header.dbhSampleRate = SAMPLE_RATE_FIXED;
    dbl_header.dbhBufferPtr[0] = (SndDoubleBufferPtr)&dbl_buffers[0];
    dbl_header.dbhBufferPtr[1] = (SndDoubleBufferPtr)&dbl_buffers[1];
    dbl_header.dbhDoubleBack = (SndDoubleBackProcPtr)DoubleBackProc;

    audio_inited = 1;

    return 1;
}

void audio_mac_pump(void)
{
    // audio is now generated in the doubleback procedure
}

void audio_mac_start(void)
{
    if (!audio_inited || !snd_channel)
        return;

    // pre-fill both buffers
    FillBuffer(dbl_buffers[0].dbSoundData);
    FillBuffer(dbl_buffers[1].dbSoundData);

    SndPlayDoubleBuffer(snd_channel, &dbl_header);
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
