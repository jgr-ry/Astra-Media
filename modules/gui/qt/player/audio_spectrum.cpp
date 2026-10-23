/*****************************************************************************
 * Copyright (C) 2026 Astra Media contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "audio_spectrum.hpp"

#include <vlc_aout.h>
#include <vlc_variables.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>

namespace {

constexpr const char* SPECTRUM_VAR = "astra-spectrum";
constexpr const char* ACTIVE_VAR = "astra-spectrum-active";
constexpr const char* FILTER_NAME = "astra_spectrum";

// the audio thread can't run indefinitely ahead of what we display
constexpr size_t MAX_PENDING_FRAMES = 240;
// after this long without data, bars fall back to rest
constexpr qint64 SILENCE_TIMEOUT_MS = 350;

constexpr float ATTACK = 0.55f;   // how fast bars rise
constexpr float RELEASE = 0.14f;  // how fast bars fall
constexpr float PEAK_FALL = 0.012f;

} // namespace

AudioSpectrum::AudioSpectrum(qt_intf_t* intf, PlayerController* player, QObject* parent)
    : QObject(parent)
    , m_intf(intf)
    , m_player(player)
{
    m_clock.start();

    m_timer.setTimerType(Qt::PreciseTimer);
    m_timer.setInterval(16);
    connect(&m_timer, &QTimer::timeout, this, &AudioSpectrum::tick);

    // the audio output can be recreated (device or module change), make sure
    // we follow the current one whenever playback starts
    connect(m_player, &PlayerController::playingStateChanged, this,
            [this](PlayerController::PlayingState state) {
        if (state == PlayerController::PLAYING_STATE_PLAYING)
            attach();
    });

    attach();
}

AudioSpectrum::~AudioSpectrum()
{
    detach();
}

void AudioSpectrum::attach()
{
    SharedAOut shared = m_player->getAout();
    audio_output_t* aout = shared.get();
    if (aout == m_aout)
    {
        // still make sure our filter is in the chain, preferences may have
        // replaced the filter list
        if (aout)
            aout_EnableFilter(aout, FILTER_NAME, true);
        return;
    }

    detach();
    if (!aout)
        return;

    aout_Hold(aout);
    m_aout = aout;

    var_Create(aout, ACTIVE_VAR, VLC_VAR_BOOL);
    var_SetBool(aout, ACTIVE_VAR, m_active);
    var_Create(aout, SPECTRUM_VAR, VLC_VAR_STRING);
    var_AddCallback(aout, SPECTRUM_VAR, &AudioSpectrum::onSpectrum, this);

    // no-op if the filter is already part of the chain
    aout_EnableFilter(aout, FILTER_NAME, true);
    msg_Dbg(m_intf, "astra spectrum attached to audio output");
}

void AudioSpectrum::detach()
{
    if (!m_aout)
        return;

    var_SetBool(m_aout, ACTIVE_VAR, false);
    // var_DelCallback waits for running callbacks to finish
    var_DelCallback(m_aout, SPECTRUM_VAR, &AudioSpectrum::onSpectrum, this);
    var_Destroy(m_aout, SPECTRUM_VAR);
    var_Destroy(m_aout, ACTIVE_VAR);
    aout_Release(m_aout);
    m_aout = nullptr;

    std::lock_guard<std::mutex> lock(m_mutex);
    m_pending.clear();
}

void AudioSpectrum::setActive(bool active)
{
    if (m_active == active)
        return;
    m_active = active;

    if (m_aout)
        var_SetBool(m_aout, ACTIVE_VAR, active);

    if (active)
    {
        m_lastData = m_clock.elapsed();
        m_timer.start();
    }
    else
    {
        // let the bars fall gracefully, the timer stops itself at rest
        std::lock_guard<std::mutex> lock(m_mutex);
        m_pending.clear();
    }
    emit activeChanged();
}

void AudioSpectrum::setLatency(int latency)
{
    latency = std::clamp(latency, 0, 2000);
    if (latency == m_latency)
        return;
    m_latency = latency;
    emit latencyChanged();
}

qreal AudioSpectrum::band(int index) const
{
    if (index < 0 || index >= BAND_COUNT)
        return 0.;
    return m_bands[index];
}

qreal AudioSpectrum::peak(int index) const
{
    if (index < 0 || index >= BAND_COUNT)
        return 0.;
    return m_peaks[index];
}

// called from the audio thread
int AudioSpectrum::onSpectrum(vlc_object_t*, const char*, vlc_value_t,
                              vlc_value_t newval, void* data)
{
    auto that = static_cast<AudioSpectrum*>(data);
    if (newval.psz_string)
        that->parse(newval.psz_string);
    return VLC_SUCCESS;
}

// called from the audio thread, must stay cheap
void AudioSpectrum::parse(const char* text)
{
    const qint64 now = m_clock.elapsed();
    const int latency = m_latency.load(std::memory_order_relaxed);

    std::lock_guard<std::mutex> lock(m_mutex);
    const char* p = text;
    while (*p)
    {
        Frame frame;
        char* end = nullptr;

        const long offset = std::strtol(p, &end, 10);
        if (end == p || *end != '|')
            break;
        p = end + 1;

        frame.level = std::strtof(p, &end);
        if (end == p || *end != '|')
            break;
        p = end + 1;

        int i = 0;
        for (; i < BAND_COUNT; ++i)
        {
            frame.bands[i] = std::strtof(p, &end);
            if (end == p)
                break;
            p = end;
            if (*p == ',')
                ++p;
            else
            {
                ++i;
                break;
            }
        }
        if (i < BAND_COUNT)
            break; // malformed frame

        frame.due = now + latency + offset;
        m_pending.push_back(frame);
        if (m_pending.size() > MAX_PENDING_FRAMES)
            m_pending.pop_front();

        if (*p != ';')
            break;
        ++p;
    }
}

void AudioSpectrum::tick()
{
    const qint64 now = m_clock.elapsed();
    bool gotFrame = false;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        while (!m_pending.empty() && m_pending.front().due <= now)
        {
            const Frame& f = m_pending.front();
            m_target = f.bands;
            m_targetLevel = f.level;
            m_pending.pop_front();
            gotFrame = true;
        }
    }

    if (gotFrame)
        m_lastData = now;

    const bool receiving = m_active && (now - m_lastData) < SILENCE_TIMEOUT_MS;
    if (!receiving)
    {
        m_target.fill(0.f);
        m_targetLevel = 0.f;
    }
    if (receiving != m_receiving)
    {
        m_receiving = receiving;
        emit receivingChanged();
    }

    bool atRest = true;
    float bass = 0.f;
    for (int i = 0; i < BAND_COUNT; ++i)
    {
        const float target = m_target[i];
        float& v = m_bands[i];
        v += (target - v) * (target > v ? ATTACK : RELEASE);
        if (v < 0.001f)
            v = 0.f;

        float& pk = m_peaks[i];
        pk = std::max(v, pk - PEAK_FALL);
        if (pk < 0.001f)
            pk = 0.f;

        if (v > 0.f || pk > 0.f)
            atRest = false;
        if (i < 6)
            bass += v;
    }
    bass /= 6.f;

    m_level += (m_targetLevel - m_level) * (m_targetLevel > m_level ? ATTACK : RELEASE);
    m_bass += (bass - m_bass) * (bass > m_bass ? 0.6 : 0.18);
    if (m_level < 0.001)
        m_level = 0.;
    if (m_bass < 0.001)
        m_bass = 0.;
    if (m_level > 0. || m_bass > 0.)
        atRest = false;

    ++m_frame;
    emit frameChanged();

    if (!m_active && atRest)
        m_timer.stop();
}
