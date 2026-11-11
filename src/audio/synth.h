// Neutro X - kit de sintese usado pelos temas sonoros.
// Todos os sons do Neutro X sao gerados por estas rotinas (osciladores,
// ruido filtrado e envelopes). Nada e extraido dos dashboards originais.
#pragma once
#include <cmath>
#include <cstdint>
#include <vector>
#include "audio/audio.h"

namespace nx {
namespace synth {

constexpr double kTwoPi = 6.283185307179586;
constexpr double kPiD = 3.141592653589793;

struct Buf {
    std::vector<float> s;
    explicit Buf(double seconds) : s(size_t(seconds * kAudioRate), 0.0f) {}

    // Nota senoidal com glissando f0->f1, ataque linear e decaimento exponencial.
    void tone(double start, double dur, double f0, double f1, double amp, double attack = 0.004, double decay = 8.0,
              double harm2 = 0.0, double harm3 = 0.0) {
        size_t i0 = size_t(start * kAudioRate), n = size_t(dur * kAudioRate);
        double phase = 0;
        for (size_t i = 0; i < n && i0 + i < s.size(); ++i) {
            double t = double(i) / kAudioRate;
            double f = f0 + (f1 - f0) * (t / dur);
            phase += kTwoPi * f / kAudioRate;
            double env = (t < attack ? t / attack : 1.0) * std::exp(-decay * t);
            double tail = 1.0 - std::pow(t / dur, 4.0);  // evita clique no fim
            s[i0 + i] += float(amp * env * tail *
                               (std::sin(phase) + harm2 * std::sin(2 * phase) + harm3 * std::sin(3 * phase)));
        }
    }

    // Timbre de marimba: fundamental + parciais inarmonicos curtos.
    void marimba(double start, double f, double amp, double decay = 12.0) {
        tone(start, 0.6, f, f, amp, 0.0015, decay);
        tone(start, 0.25, f * 3.93, f * 3.93, amp * 0.28, 0.001, decay * 2.8);
        tone(start, 0.08, f * 9.7, f * 9.7, amp * 0.09, 0.0008, decay * 5.0);
    }

    // Sino de vidro: dois osciladores levemente desafinados (batimento).
    void glass(double start, double dur, double f, double amp, double decay = 7.0) {
        tone(start, dur, f, f, amp * 0.6, 0.002, decay, 0.0, 0.12);
        tone(start, dur, f * 1.004, f * 1.004, amp * 0.4, 0.002, decay * 1.1);
        tone(start, dur * 0.5, f * 2.76, f * 2.76, amp * 0.12, 0.001, decay * 2.5);
    }

    // Ruido filtrado (passa-baixa variavel) para "sopros" e transicoes.
    // bright: 0.3 escuro .. 2.0 brilhante.
    void whoosh(double start, double dur, double amp, double bright = 1.0, uint32_t seed = 0x12345u) {
        size_t i0 = size_t(start * kAudioRate), n = size_t(dur * kAudioRate);
        double lp = 0, lp2 = 0;
        for (size_t i = 0; i < n && i0 + i < s.size(); ++i) {
            seed = seed * 1664525u + 1013904223u;
            double noise = (double((seed >> 9) & 0x7FFF) / 16383.5) - 1.0;
            double t = double(i) / n;
            double cutoff = std::min(0.9, (0.02 + 0.25 * std::sin(kPiD * t)) * bright);
            lp += cutoff * (noise - lp);
            lp2 += cutoff * (lp - lp2);
            double env = std::sin(kPiD * t);
            s[i0 + i] += float(amp * env * env * lp2 * 3.0);
        }
    }

    // Clique curto (ruido com passa-alta), para toques "secos".
    void click(double start, double amp, double dur = 0.006, double bright = 1.0, uint32_t seed = 0xBEEFu) {
        size_t i0 = size_t(start * kAudioRate), n = size_t(dur * kAudioRate);
        double prev = 0, lp = 0;
        for (size_t i = 0; i < n && i0 + i < s.size(); ++i) {
            seed = seed * 1664525u + 1013904223u;
            double noise = (double((seed >> 9) & 0x7FFF) / 16383.5) - 1.0;
            double hp = noise - prev;  // diferenca = passa-alta simples
            prev = noise;
            lp += std::min(1.0, 0.3 * bright) * (hp - lp);
            double env = std::exp(-double(i) / n * 5.0);
            s[i0 + i] += float(amp * env * lp);
        }
    }

    std::vector<int16_t> toPcm(float gain = 1.0f) const {
        std::vector<int16_t> out(s.size());
        for (size_t i = 0; i < s.size(); ++i) {
            float v = std::tanh(s[i] * gain) * 32000.0f;  // saturacao suave
            out[i] = int16_t(std::lround(v));
        }
        return out;
    }
};

}  // namespace synth

// Um gerador por tema (um arquivo por versao em audio/themes/).
std::vector<int16_t> synthNeutroX(Sfx s);
std::vector<int16_t> synthBlades6717(Sfx s);
std::vector<int16_t> synthNxe9199(Sfx s);
std::vector<int16_t> synthKinect13604(Sfx s);
std::vector<int16_t> synthMetroV1_15574(Sfx s);
std::vector<int16_t> synthMetroV2_17559(Sfx s);

}  // namespace nx
