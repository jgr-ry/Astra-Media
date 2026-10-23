/*****************************************************************************
 * astra_spectrum.c : real-time spectrum analyser for the Astra Media interface
 *****************************************************************************
 * Copyright © 2026 Astra Media contributors
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*
 * This filter does not modify the audio. It downmixes the samples to mono,
 * runs a windowed FFT roughly 60 times per second of audio and publishes the
 * result on the parent audio output through the "astra-spectrum" string
 * variable, so that the interface can draw a visualizer that reacts to the
 * music.
 *
 * Published format, one or more frames separated by ';':
 *     <offset_ms>|<level>|<band0>,<band1>,...,<bandN-1>
 * offset_ms is the position of the frame inside the processed block, level
 * and bands are normalised to [0, 1].
 *
 * Nothing is computed unless the interface sets the boolean variable
 * "astra-spectrum-active" on the audio output.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <vlc_common.h>
#include <vlc_aout.h>
#include <vlc_filter.h>
#include <vlc_plugin.h>

#define FFT_BITS     11
#define FFT_SIZE     (1 << FFT_BITS)          /* 2048 samples */
#define BAND_COUNT   48
#define FRAME_RATE   60                        /* frames per second of audio */
#define MIN_FREQ     35.f
#define MAX_FREQ     16000.f
#define DB_FLOOR     -62.f
#define DB_CEIL      -8.f
#define MAX_FRAMES_PER_BLOCK 32

static int  Open (vlc_object_t *);
static void Close(filter_t *);
static block_t *Process(filter_t *, block_t *);

vlc_module_begin()
    set_shortname(N_("Astra spectrum"))
    set_description(N_("Spectrum analyser for the Astra Media visualizer"))
    set_subcategory(SUBCAT_AUDIO_AFILTER)
    set_capability("audio filter", 0)
    set_callback(Open)
vlc_module_end()

typedef struct
{
    float ring[FFT_SIZE];     /* last FFT_SIZE mono samples */
    unsigned ring_pos;
    unsigned since_frame;     /* samples since the last published frame */
    unsigned hop;             /* samples between two frames */

    float window[FFT_SIZE];   /* Hann window */
    float re[FFT_SIZE];
    float im[FFT_SIZE];
    float cos_tab[FFT_SIZE / 2];
    float sin_tab[FFT_SIZE / 2];

    unsigned band_lo[BAND_COUNT];
    unsigned band_hi[BAND_COUNT];

    unsigned channels;
    unsigned rate;

    char *text;               /* output buffer */
    size_t text_size;
} filter_sys_t;

static unsigned BitReverse(unsigned x)
{
    unsigned r = 0;
    for (unsigned i = 0; i < FFT_BITS; i++)
    {
        r = (r << 1) | (x & 1);
        x >>= 1;
    }
    return r;
}

/* In-place iterative radix-2 FFT. */
static void FFT(filter_sys_t *sys)
{
    float *re = sys->re, *im = sys->im;

    for (unsigned i = 0; i < FFT_SIZE; i++)
    {
        unsigned j = BitReverse(i);
        if (j > i)
        {
            float t = re[i]; re[i] = re[j]; re[j] = t;
            t = im[i]; im[i] = im[j]; im[j] = t;
        }
    }

    for (unsigned size = 2; size <= FFT_SIZE; size <<= 1)
    {
        const unsigned half = size >> 1;
        const unsigned step = FFT_SIZE / size;
        for (unsigned start = 0; start < FFT_SIZE; start += size)
        {
            for (unsigned k = 0; k < half; k++)
            {
                const float wr = sys->cos_tab[k * step];
                const float wi = -sys->sin_tab[k * step];
                const unsigned a = start + k, b = a + half;
                const float tr = re[b] * wr - im[b] * wi;
                const float ti = re[b] * wi + im[b] * wr;
                re[b] = re[a] - tr;
                im[b] = im[a] - ti;
                re[a] += tr;
                im[a] += ti;
            }
        }
    }
}

static void SetupBands(filter_sys_t *sys)
{
    const float nyquist = sys->rate / 2.f;
    const float max_freq = __MIN(MAX_FREQ, nyquist * 0.95f);
    const float bin_hz = (float)sys->rate / FFT_SIZE;
    unsigned prev_hi = 1;

    for (unsigned i = 0; i < BAND_COUNT; i++)
    {
        /* logarithmic spacing, like the human ear */
        float f_lo = MIN_FREQ * powf(max_freq / MIN_FREQ, (float)i / BAND_COUNT);
        float f_hi = MIN_FREQ * powf(max_freq / MIN_FREQ, (float)(i + 1) / BAND_COUNT);
        unsigned lo = (unsigned)lrintf(f_lo / bin_hz);
        unsigned hi = (unsigned)lrintf(f_hi / bin_hz);

        if (lo < prev_hi)
            lo = prev_hi;
        if (lo < 1)
            lo = 1;
        if (hi <= lo)
            hi = lo + 1;
        if (hi > FFT_SIZE / 2)
            hi = FFT_SIZE / 2;
        if (lo >= hi)
            lo = hi - 1;

        sys->band_lo[i] = lo;
        sys->band_hi[i] = hi;
        /* low bands share bins at this resolution, let them overlap instead
         * of producing empty bands */
        prev_hi = (hi - lo > 1) ? hi : lo;
    }
}

static int Open(vlc_object_t *obj)
{
    filter_t *filter = (filter_t *)obj;

    if (filter->fmt_in.audio.i_rate == 0
     || filter->fmt_in.audio.i_channels == 0)
        return VLC_EGENERIC;

    /* work in float, the core inserts a converter when needed */
    filter->fmt_in.audio.i_format = VLC_CODEC_FL32;
    aout_FormatPrepare(&filter->fmt_in.audio);

    filter_sys_t *sys = calloc(1, sizeof (*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    sys->channels = filter->fmt_in.audio.i_channels;
    sys->rate = filter->fmt_in.audio.i_rate;
    sys->hop = __MAX(sys->rate / FRAME_RATE, 64);
    sys->since_frame = 0;

    for (unsigned i = 0; i < FFT_SIZE; i++)
        sys->window[i] = 0.5f * (1.f - cosf(2.f * (float)M_PI * i / (FFT_SIZE - 1)));
    for (unsigned i = 0; i < FFT_SIZE / 2; i++)
    {
        sys->cos_tab[i] = cosf(2.f * (float)M_PI * i / FFT_SIZE);
        sys->sin_tab[i] = sinf(2.f * (float)M_PI * i / FFT_SIZE);
    }
    SetupBands(sys);

    /* "offset|level|" + BAND_COUNT * "0.000," per frame */
    sys->text_size = MAX_FRAMES_PER_BLOCK * (32 + BAND_COUNT * 6) + 1;
    sys->text = malloc(sys->text_size);
    if (unlikely(sys->text == NULL))
    {
        free(sys);
        return VLC_ENOMEM;
    }

    filter->fmt_out.audio = filter->fmt_in.audio;
    filter->p_sys = sys;

    static const struct vlc_filter_operations filter_ops =
        { .filter_audio = Process, .close = Close };
    filter->ops = &filter_ops;

    msg_Dbg(filter, "spectrum analyser ready (%u Hz, %u channels)",
            sys->rate, sys->channels);
    return VLC_SUCCESS;
}

static float Normalize(float magnitude)
{
    float db = 20.f * log10f(magnitude + 1e-9f);
    float v = (db - DB_FLOOR) / (DB_CEIL - DB_FLOOR);
    if (v < 0.f)
        return 0.f;
    if (v > 1.f)
        return 1.f;
    return v;
}

/* Computes one frame from the ring buffer and appends it to sys->text. */
static size_t AppendFrame(filter_sys_t *sys, size_t len, unsigned offset_ms)
{
    float energy = 0.f;

    for (unsigned i = 0; i < FFT_SIZE; i++)
    {
        float s = sys->ring[(sys->ring_pos + i) % FFT_SIZE];
        energy += s * s;
        sys->re[i] = s * sys->window[i];
        sys->im[i] = 0.f;
    }
    FFT(sys);

    /* RMS level, mapped to a pleasant 0..1 curve */
    float rms = sqrtf(energy / FFT_SIZE);
    float level = Normalize(rms * 2.f);

    int n = snprintf(sys->text + len, sys->text_size - len, "%s%u|%.3f|",
                     len > 0 ? ";" : "", offset_ms, level);
    if (n < 0 || (size_t)n >= sys->text_size - len)
        return len;
    len += n;

    /* Hann window coherent gain is 0.5 */
    const float scale = 2.f / (FFT_SIZE * 0.5f);
    for (unsigned b = 0; b < BAND_COUNT; b++)
    {
        float peak = 0.f;
        for (unsigned k = sys->band_lo[b]; k < sys->band_hi[b]; k++)
        {
            float mag = sqrtf(sys->re[k] * sys->re[k] + sys->im[k] * sys->im[k]);
            if (mag > peak)
                peak = mag;
        }
        /* gently lift the treble, music has much less energy up there */
        float tilt = 1.f + 2.5f * (float)b / BAND_COUNT;
        float v = Normalize(peak * scale * tilt);

        n = snprintf(sys->text + len, sys->text_size - len, "%s%.3f",
                     b > 0 ? "," : "", v);
        if (n < 0 || (size_t)n >= sys->text_size - len)
            break;
        len += n;
    }
    return len;
}

static block_t *Process(filter_t *filter, block_t *block)
{
    filter_sys_t *sys = filter->p_sys;
    vlc_object_t *aout = vlc_object_parent(filter);

    if (!var_GetBool(aout, "astra-spectrum-active"))
        return block;

    const float *samples = (const float *)block->p_buffer;
    const unsigned channels = sys->channels;
    const unsigned frames = block->i_nb_samples;
    const float inv_channels = 1.f / channels;
    size_t len = 0;
    unsigned published = 0;

    sys->text[0] = '\0';
    for (unsigned i = 0; i < frames; i++)
    {
        float mono = 0.f;
        for (unsigned c = 0; c < channels; c++)
            mono += samples[i * channels + c];
        sys->ring[sys->ring_pos] = mono * inv_channels;
        sys->ring_pos = (sys->ring_pos + 1) % FFT_SIZE;

        if (++sys->since_frame >= sys->hop)
        {
            sys->since_frame = 0;
            if (published < MAX_FRAMES_PER_BLOCK)
            {
                unsigned offset_ms = (unsigned)((uint64_t)i * 1000 / sys->rate);
                len = AppendFrame(sys, len, offset_ms);
                published++;
            }
        }
    }

    if (len > 0)
        var_SetString(aout, "astra-spectrum", sys->text);

    return block;
}

static void Close(filter_t *filter)
{
    filter_sys_t *sys = filter->p_sys;
    free(sys->text);
    free(sys);
}
