/* Game Boy emulator for 68k Macs
   audio_mac.c - Sound Manager integration with deferred task processing

   Architecture (from develop magazine, August 1992):
   - SndCallbackProc runs at interrupt priority, does minimal work:
     queues next buffer and installs deferred task
   - Deferred task runs with interrupts re-enabled, does the actual
     audio_generate() call
   - This prevents the audio generation from blocking higher-priority
     interrupts and starving the ASC
*/

#include <Sound.h>
#include <Memory.h>
#include <Gestalt.h>
#include <OSUtils.h>
#include <string.h>

#include "audio_mac.h"
#include "../src/audio.h"

// 512 samples, has to be multiple of 512 for ASC buffer alignment
#define BUFFER_SAMPLES 1024
#define SAMPLE_RATE_FIXED 0x2b7745d1

// buffer states
#define kBufferReady   0
#define kBufferPlaying 1
#define kBufferFilling 2

static SndChannelPtr snd_channel;
static struct audio *g_audio;
static int audio_inited;

// sound header for bufferCmd  with samples embedded in sampleArea
typedef struct {
    Ptr samplePtr;
    unsigned long length;
    Fixed sampleRate;
    unsigned long loopStart;
    unsigned long loopEnd;
    unsigned char encode;
    unsigned char baseFrequency;
    unsigned char sampleArea[BUFFER_SAMPLES];
} MySoundHeader;

// buffer with state tracking and deferred task record
typedef struct {
    short flags;
    DeferredTask dt;
    MySoundHeader header;
} SampleBuffer;

static SampleBuffer snd_buffers[2];

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

static void FillBuffer(int which)
{
    unsigned char *p = snd_buffers[which].header.sampleArea;

    if (!g_audio) {
        memset(p, 0x80, BUFFER_SAMPLES);
        return;
    }

    audio_generate(g_audio, p, BUFFER_SAMPLES);
}

static void QueueBuffer(int which)
{
    SndCommand cmd;

    // send bufferCmd to play this buffer
    cmd.cmd = bufferCmd;
    cmd.param1 = 0;
    cmd.param2 = (long)&snd_buffers[which].header;
    SndDoCommand(snd_channel, &cmd, false);

    // send callBackCmd - fires when bufferCmd completes
    cmd.cmd = callBackCmd;
    cmd.param1 = 0;
    cmd.param2 = which;
    SndDoCommand(snd_channel, &cmd, false);

    snd_buffers[which].flags = kBufferPlaying;
}

// deferred task handler - runs with interrupts enabled
// dtParm (in A1) contains buffer index to fill
static pascal void DeferredTaskHandler(void)
{
    long param;
    int buf_idx;

    // get parameter from A1 (buffer index)
    asm volatile("move.l %%a1, %0" : "=g"(param));
    buf_idx = (int)param;

    // do the actual audio generation
    FillBuffer(buf_idx);
    snd_buffers[buf_idx].flags = kBufferReady;
}

// sound callback - runs at interrupt time, must be fast
static pascal void SndCallbackProc(SndChannelPtr chan, SndCommand *cmd)
{
    int finished = cmd->param2;
    int next = 1 - finished;

    // queue the next buffer if it's ready
    if (snd_buffers[next].flags == kBufferReady) {
        QueueBuffer(next);
    }
    // else: underrun - next buffer not ready yet
    // the chain will break, but at least we won't crash

    // install deferred task to fill the finished buffer
    snd_buffers[finished].flags = kBufferFilling;
    snd_buffers[finished].dt.qType = dtQType;
    snd_buffers[finished].dt.dtFlags = 0;
    snd_buffers[finished].dt.dtAddr = DeferredTaskHandler;
    snd_buffers[finished].dt.dtParam = finished;
    snd_buffers[finished].dt.dtReserved = 0;
    DTInstall(&snd_buffers[finished].dt);
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

    // create channel with callback procedure
    snd_channel = NULL;
    err = SndNewChannel(
        &snd_channel,
        sampledSynth,
        initMono,
        (SndCallBackProcPtr)SndCallbackProc
    );
    if (err != noErr) {
        return 0;
    }

    // initialize buffers
    for (k = 0; k < 2; k++) {
        memset(&snd_buffers[k], 0, sizeof(snd_buffers[k]));
        snd_buffers[k].flags = kBufferReady;
        snd_buffers[k].header.samplePtr = NULL;
        snd_buffers[k].header.length = BUFFER_SAMPLES;
        snd_buffers[k].header.sampleRate = SAMPLE_RATE_FIXED;
        snd_buffers[k].header.loopStart = 0;
        snd_buffers[k].header.loopEnd = 0;
        snd_buffers[k].header.encode = 0;  // stdSH
        snd_buffers[k].header.baseFrequency = 60;  // middle C
        memset(snd_buffers[k].header.sampleArea, 0x80, BUFFER_SAMPLES);
    }

    audio_inited = 1;

    return 1;
}

void audio_mac_start(void)
{
    if (!audio_inited || !snd_channel)
        return;

    // pre-fill both buffers
    FillBuffer(0);
    FillBuffer(1);
    snd_buffers[0].flags = kBufferReady;
    snd_buffers[1].flags = kBufferReady;

    // queue only the first buffer - the callback will queue the second
    // when the first finishes, starting the chain reaction
    QueueBuffer(0);
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
