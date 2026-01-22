/* Game Boy emulator for 68k Macs
   audio_mac.c - Sound Manager integration using SndPlayDoubleBuffer */

// this generates 185 samples per 1/60 sec. the samples are generated at interrupt
// time based on when the Sound Manager needs them - sampling the state of the
// GB's audio registers completely desynchronized from the game execution. this
// means the tempo of the music varies and notes can also be missed, but IMO this
// is still less jarring than the drop outs that would happen if the sample
// generation was coupled to the actual GB execution.

// i'm kind of thinking of refactoring to prepare the samples synchronized to the GB
// cycles (accumulate samples until it has 185, then submit, or something fancier
// with a ring buffer), but don't want to deal with the fact that the Sound Manager
// could "run out" before it has 185 ready - would be harder to listen to than
// the current solution

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

typedef struct {
    long dbNumFrames;
    long dbFlags;
    long dbUserInfo[2];
    unsigned char dbSoundData[BUFFER_SAMPLES];
} MyDoubleBuffer;

static SndDoubleBufferHeader dbl_header;
static MyDoubleBuffer dbl_buffers[2];

// for Mac Plus/SE/Classic
// #define VIA1_T1CL (*(volatile u8 *) 0xEFE9FE)
// #define VIA1_T1CH (*(volatile u8 *) 0xEFEBFE)
// for Mac II
// #define VIA1_T1CL (*(volatile u8 *) 0x50f00800)
// #define VIA1_T1CH (*(volatile u8 *) 0x50f00a00)
// u32 audio_timer_accum;
// u32 audio_calls;

// static u16 read_via_t1(void)
// {
//     u8 lo = VIA1_T1CL;
//     u8 hi = VIA1_T1CH;
//     return (hi << 8) | lo;
// }

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

static void FillBuffer(unsigned char *p)
{
    int k;

    if (!g_audio) {
        memset(p, 0x80, BUFFER_SAMPLES);
        return;
    }

    audio_generate(g_audio, p, BUFFER_SAMPLES);
}

// called at interrupt time when a buffer is exhausted
static pascal void DoubleBackProc(SndChannelPtr chan, SndDoubleBufferPtr buf)
{
    // u16 t0, t1, delta;

    // t0 = read_via_t1();
    FillBuffer(buf->dbSoundData);
    buf->dbNumFrames = BUFFER_SAMPLES;
    buf->dbFlags |= dbBufferReady;
    // t1 = read_via_t1();
    // if (t0 >= t1) {
    //     delta = t0 - t1;
    // } else {
    //     // wrapped - t0 was near 0, t1 is near latch value after reload
    //     // latch is roughly 13050 for 60Hz tick rate
    //     delta = t0 + (13050 - t1);
    // }
    // audio_timer_accum += delta;
    // audio_calls++;
}

int audio_mac_init(struct audio *audio)
{
    OSErr err;
    int k;

    if (!audio_mac_available()) {
        return 0;
    } 

    if (audio_inited) {
        return 1;
    }

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
