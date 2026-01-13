#include <string.h>
#include "audio.h"

// duty cycle patterns (8 steps each)
// 0 = 12.5%, 1 = 25%, 2 = 50%, 3 = 75%
static const u8 duty_table[4] = {
    0x01, // 00000001
    0x03, // 00000011
    0x0f, // 00001111
    0xfc  // 11111100
};

// noise divisor table
static const u8 divisor_table[8] = { 8, 16, 32, 48, 64, 80, 96, 112 };

// precomputed LFSR output bits (generated at init)
static u8 lfsr15_bits[4096]; // 32768 bits for 15-bit mode
static u8 lfsr7_bits[16];    // 128 bits for 7-bit mode

static void update_phase_inc(struct audio_channel *ch)
{
    // GB frequency to Hz: 131072 / (2048 - freq_reg)
    // phase_inc = (freq_hz / sample_rate) * 65536
    // combined: phase_inc = (131072 * 65536) / ((2048 - freq_reg) * sample_rate)
    // 2^33 / 11127.27 = 771971
    u32 freq_reg = ch->freq_reg;
    if (freq_reg >= 2048)
        freq_reg = 2047;

    u32 divisor = 2048 - freq_reg;
    if (divisor == 0)
        divisor = 1;

    ch->phase_inc = 771971 / divisor;
}

static void trigger_channel(struct audio_channel *ch)
{
    ch->enabled = 1;
    ch->phase = 0;
    ch->volume = ch->env_initial;
    ch->env_timer = ch->env_pace;
    update_phase_inc(ch);
}

static void step_envelope(struct audio_channel *ch)
{
    if (ch->env_pace == 0) {
        return;
    }

    if (ch->env_timer > 0) {
        ch->env_timer--;
    }

    if (ch->env_timer == 0) {
        ch->env_timer = ch->env_pace;

        if (ch->env_dir) {
            // increase
            if (ch->volume < 15) {
                ch->volume++;
            }
        } else {
            // decrease
            if (ch->volume > 0) {
                ch->volume--;
            }
        }
    }
}

void audio_init(struct audio *audio)
{
    int k;
    u16 lfsr;

    memset(audio, 0, sizeof(*audio));

    // generate 15-bit LFSR output table (period 32767)
    memset(lfsr15_bits, 0, sizeof(lfsr15_bits));
    lfsr = 0x7fff;
    for (k = 0; k < 32767; k++) {
        if (lfsr & 1)
            lfsr15_bits[k >> 3] |= (1 << (k & 7));
        int xor_bit = (lfsr ^ (lfsr >> 1)) & 1;
        lfsr = (lfsr >> 1) | (xor_bit << 14);
    }

    // generate 7-bit LFSR output table (period 127)
    memset(lfsr7_bits, 0, sizeof(lfsr7_bits));
    lfsr = 0x7f;
    for (k = 0; k < 127; k++) {
        if (lfsr & 1)
            lfsr7_bits[k >> 3] |= (1 << (k & 7));
        int xor_bit = (lfsr ^ (lfsr >> 1)) & 1;
        lfsr = (lfsr >> 1) | (xor_bit << 6);
    }
}

void audio_write(struct audio *audio, u16 addr, u8 value)
{
    if (addr < 0xff10 || addr > 0xff3f)
        return;

    int reg = addr - 0xff10;
    audio->regs[reg] = value;

    // wave RAM
    if (addr >= REG_WAVE_START && addr <= REG_WAVE_END) {
        audio->wave_ram[addr - REG_WAVE_START] = value;
        return;
    }

    switch (addr) {
    // CH1 - square with sweep
    case 0xff10:    // NR10 - sweep
        audio->ch1.sweep_pace = (value >> 4) & 0x07;
        audio->ch1.sweep_dir = (value >> 3) & 0x01;
        audio->ch1.sweep_step = value & 0x07;
        break;
    case 0xff11:    // NR11 - duty/length
        audio->ch1.duty = (value >> 6) & 0x03;
        break;
    case 0xff12:    // NR12 - envelope
        audio->ch1.env_initial = (value >> 4) & 0x0f;
        audio->ch1.env_dir = (value >> 3) & 0x01;
        audio->ch1.env_pace = value & 0x07;
        break;
    case 0xff13:    // NR13 - freq low
        audio->ch1.freq_reg = (audio->ch1.freq_reg & 0x700) | value;
        update_phase_inc(&audio->ch1);
        break;
    case 0xff14:    // NR14 - freq high + trigger
        audio->ch1.freq_reg = (audio->ch1.freq_reg & 0xff) | ((value & 0x07) << 8);
        update_phase_inc(&audio->ch1);
        if (value & 0x80)
            trigger_channel(&audio->ch1);
        break;

    // CH2 - square (no sweep)
    case 0xff16:    // NR21 - duty/length
        audio->ch2.duty = (value >> 6) & 0x03;
        break;
    case 0xff17:    // NR22 - envelope
        audio->ch2.env_initial = (value >> 4) & 0x0f;
        audio->ch2.env_dir = (value >> 3) & 0x01;
        audio->ch2.env_pace = value & 0x07;
        break;
    case 0xff18:    // NR23 - freq low
        audio->ch2.freq_reg = (audio->ch2.freq_reg & 0x700) | value;
        update_phase_inc(&audio->ch2);
        break;
    case 0xff19:    // NR24 - freq high + trigger
        audio->ch2.freq_reg = (audio->ch2.freq_reg & 0xff) | ((value & 0x07) << 8);
        update_phase_inc(&audio->ch2);
        if (value & 0x80)
            trigger_channel(&audio->ch2);
        break;

    // CH3 - wave
    case 0xff1a:    // NR30 - DAC enable
        audio->ch3.enabled = (value & 0x80) ? 1 : 0;
        break;
    case 0xff1c:    // NR32 - volume
        // volume code: 0=mute, 1=100%, 2=50%, 3=25%
        audio->ch3.volume = (value >> 5) & 0x03;
        break;
    case 0xff1d:    // NR33 - freq low
        audio->ch3.freq_reg = (audio->ch3.freq_reg & 0x700) | value;
        update_phase_inc(&audio->ch3);
        break;
    case 0xff1e:    // NR34 - freq high + trigger
        audio->ch3.freq_reg = (audio->ch3.freq_reg & 0xff) | ((value & 0x07) << 8);
        update_phase_inc(&audio->ch3);
        if (value & 0x80) {
            audio->ch3.enabled = 1;
            audio->ch3.phase = 0;
        }
        break;

    // CH4 - noise
    case 0xff21:    // NR42 - envelope
        audio->ch4.env_initial = (value >> 4) & 0x0f;
        audio->ch4.env_dir = (value >> 3) & 0x01;
        audio->ch4.env_pace = value & 0x07;
        break;
    case 0xff22:    // NR43 - noise params
        audio->noise_shift = (value >> 4) & 0x0f;
        audio->lfsr_width = (value >> 3) & 0x01;
        audio->noise_divisor = value & 0x07;
        break;
    case 0xff23:    // NR44 - trigger
        if (value & 0x80) {
            audio->ch4.enabled = 1;
            audio->ch4.volume = audio->ch4.env_initial;
            audio->ch4.env_timer = audio->ch4.env_pace;
            audio->noise_pos = 0;
        }
        break;

    // master control
    case 0xff24:    // NR50 - master volume
        audio->master_vol_left = (value >> 4) & 0x07;
        audio->master_vol_right = value & 0x07;
        break;
    case 0xff25:    // NR51 - panning
        audio->panning = value;
        break;
    case 0xff26:    // NR52 - master enable
        audio->master_enable = (value & 0x80) ? 1 : 0;
        if (!audio->master_enable) {
            // disable all channels when master is off
            audio->ch1.enabled = 0;
            audio->ch2.enabled = 0;
            audio->ch3.enabled = 0;
            audio->ch4.enabled = 0;
        }
        break;
    }
}

u8 audio_read(struct audio *audio, u16 addr)
{
    if (addr < 0xff10 || addr > 0xff3f)
        return 0xff;

    // NR52 returns channel status in low bits
    if (addr == 0xff26) {
        u8 status = audio->master_enable ? 0x80 : 0;
        status |= audio->ch1.enabled ? 0x01 : 0;
        status |= audio->ch2.enabled ? 0x02 : 0;
        status |= audio->ch3.enabled ? 0x04 : 0;
        status |= audio->ch4.enabled ? 0x08 : 0;
        return status | 0x70;   // bits 4-6 always read as 1
    }

    if (addr >= REG_WAVE_START && addr <= REG_WAVE_END)
        return audio->wave_ram[addr - REG_WAVE_START];

    return audio->regs[addr - 0xff10];
}

static s8 generate_square(struct audio_channel *ch, u8 duty_pattern)
{
    if (!ch->enabled || ch->volume == 0)
        return 0;

    // phase is 16.16 fixed point, use upper 3 bits of low 16 for duty position
    int duty_pos = (ch->phase >> 13) & 0x07;
    int high = (duty_pattern >> (7 - duty_pos)) & 0x01;

    // output range: -volume to +volume
    return high ? ch->volume : -(s8)ch->volume;
}

static s8 generate_wave(struct audio *audio)
{
    struct audio_channel *ch = &audio->ch3;
    if (!ch->enabled || ch->volume == 0)
        return 0;

    // phase is 16.16 fixed point, use upper 5 bits of low 16 for sample index
    int sample_idx = (ch->phase >> 11) & 0x1f;
    int byte_idx = sample_idx >> 1;
    int nibble = audio->wave_ram[byte_idx];

    if (sample_idx & 1)
        nibble &= 0x0f;
    else
        nibble = (nibble >> 4) & 0x0f;

    // volume shift: 0=mute, 1=100%, 2=50%, 3=25%
    int shift = ch->volume;
    if (shift == 0)
        return 0;
    shift--; // now 0=100%, 1=50%, 2=25%

    // center around 0 (nibble is 0-15, center at 7.5)
    return (s8)((nibble - 8) >> shift);
}

static s8 generate_noise(struct audio *audio)
{
    struct audio_channel *ch = &audio->ch4;
    if (!ch->enabled || ch->volume == 0)
        return 0;

    // get output bit from precomputed table
    int pos, high;
    if (audio->lfsr_width) {
        // should be 0-126 but whatever, use & bc modulo is slow
        pos = (audio->noise_pos >> 16) & 0x7f;
        high = (lfsr7_bits[pos >> 3] >> (pos & 7)) & 1;
    } else {
        pos = (audio->noise_pos >> 16) & 0x7fff;
        high = (lfsr15_bits[pos >> 3] >> (pos & 7)) & 1;
    }

    return high ? ch->volume : -(s8)ch->volume;
}

void audio_generate(struct audio *audio, s8 *buffer, int samples)
{
    int k;

    if (!audio->master_enable) {
        memset(buffer, 0, samples);
        return;
    }

    // noise freq = 4194304 / (divisor << shift)
    // phase_inc = (4194304 * 65536) / (divisor * sample_rate)
    // 4194304 * 65536 / 11127.27 = 24703086
    u32 divisor = divisor_table[audio->noise_divisor];
    if (audio->noise_shift < 14)
        divisor <<= audio->noise_shift;

    u32 noise_phase_inc = 0;
    if (divisor > 0) {
        noise_phase_inc = 24703086 / divisor;
    }

    for (k = 0; k < samples; k++) {
        s16 mix = 0;

        mix += generate_square(&audio->ch1, duty_table[audio->ch1.duty]);
        mix += generate_square(&audio->ch2, duty_table[audio->ch2.duty]);
        mix += generate_wave(audio);
        mix += generate_noise(audio);

        // advance phase accumulators
        audio->ch1.phase += audio->ch1.phase_inc;
        audio->ch2.phase += audio->ch2.phase_inc;
        audio->ch3.phase += audio->ch3.phase_inc;
        audio->noise_pos += noise_phase_inc;

        // envelope tick at 64 Hz (11127 / 64 = 174 samples)
        audio->env_counter++;
        if (audio->env_counter >= 174) {
            audio->env_counter = 0;
            step_envelope(&audio->ch1);
            step_envelope(&audio->ch2);
            step_envelope(&audio->ch4);
        }

        int master = audio->master_vol_right + 1;
        mix = (mix * master) >> 3;
        buffer[k] = (s8) mix;
    }
}
