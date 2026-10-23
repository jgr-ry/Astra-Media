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
#ifndef AUDIO_SPECTRUM_HPP
#define AUDIO_SPECTRUM_HPP

#include <QObject>
#include <QTimer>
#include <QElapsedTimer>

#include <array>
#include <atomic>
#include <deque>
#include <mutex>

#include "qt.hpp"
#include "player/player_controller.hpp"

/**
 * Receives the spectrum computed by the "astra_spectrum" audio filter and
 * exposes smoothed, latency-compensated values to QML for the visualizer.
 */
class AudioSpectrum : public QObject
{
    Q_OBJECT

    // set to true while a visualizer is on screen, nothing is computed otherwise
    Q_PROPERTY(bool active READ isActive WRITE setActive NOTIFY activeChanged FINAL)
    // incremented for every new visual frame, bind on it to refresh band()
    Q_PROPERTY(int frame READ frame NOTIFY frameChanged FINAL)
    Q_PROPERTY(int bandCount READ bandCount CONSTANT FINAL)
    // overall loudness, 0..1
    Q_PROPERTY(qreal level READ level NOTIFY frameChanged FINAL)
    // low frequencies only, 0..1, good for "breathing" effects
    Q_PROPERTY(qreal bass READ bass NOTIFY frameChanged FINAL)
    // whether actual audio data arrived recently
    Q_PROPERTY(bool receiving READ isReceiving NOTIFY receivingChanged FINAL)
    // delay (ms) applied to the analysed audio to match what is heard
    Q_PROPERTY(int latency READ latency WRITE setLatency NOTIFY latencyChanged FINAL)

public:
    static constexpr int BAND_COUNT = 48;

    AudioSpectrum(qt_intf_t* intf, PlayerController* player, QObject* parent = nullptr);
    ~AudioSpectrum() override;

    bool isActive() const { return m_active; }
    void setActive(bool active);

    int frame() const { return m_frame; }
    int bandCount() const { return BAND_COUNT; }
    qreal level() const { return m_level; }
    qreal bass() const { return m_bass; }
    bool isReceiving() const { return m_receiving; }

    int latency() const { return m_latency.load(); }
    void setLatency(int latency);

    Q_INVOKABLE qreal band(int index) const;
    // peak value per band, decays slowly (for "falling caps")
    Q_INVOKABLE qreal peak(int index) const;

signals:
    void activeChanged();
    void frameChanged();
    void receivingChanged();
    void latencyChanged();

private:
    struct Frame
    {
        qint64 due = 0; // ms, on m_clock
        float level = 0.f;
        std::array<float, BAND_COUNT> bands {};
    };

    static int onSpectrum(vlc_object_t*, const char*, vlc_value_t, vlc_value_t, void*);
    void parse(const char* text);

    void attach();
    void detach();
    void tick();

    qt_intf_t* m_intf = nullptr;
    PlayerController* m_player = nullptr;
    audio_output_t* m_aout = nullptr; // held

    bool m_active = false;
    bool m_receiving = false;
    std::atomic<int> m_latency { 140 };
    int m_frame = 0;

    QTimer m_timer;
    QElapsedTimer m_clock;
    qint64 m_lastData = 0;

    // shared with the audio thread
    std::mutex m_mutex;
    std::deque<Frame> m_pending;

    // GUI thread only
    std::array<float, BAND_COUNT> m_target {};
    std::array<float, BAND_COUNT> m_bands {};
    std::array<float, BAND_COUNT> m_peaks {};
    float m_targetLevel = 0.f;
    qreal m_level = 0.;
    qreal m_bass = 0.;
};

#endif // AUDIO_SPECTRUM_HPP
