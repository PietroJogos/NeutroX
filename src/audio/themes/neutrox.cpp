// Neutro X - tema sonoro do proprio aplicativo (menu, configuracoes, transicoes).
// Timbre: senoides limpas, acorde maior com brilho. Sons originais.
#include "audio/synth.h"

namespace nx {

using synth::Buf;

std::vector<int16_t> synthNeutroX(Sfx sfx) {
    switch (sfx) {
        case Sfx::Navigate: { Buf b(0.06); b.tone(0, 0.06, 1560, 1500, 0.22, 0.002, 60, 0.15); return b.toPcm(); }
        case Sfx::Confirm: {
            Buf b(0.20);
            b.tone(0.00, 0.12, 880, 880, 0.30, 0.003, 22, 0.2);
            b.tone(0.06, 0.14, 1318.5, 1318.5, 0.30, 0.003, 18, 0.2);
            return b.toPcm();
        }
        case Sfx::Back: { Buf b(0.16); b.tone(0, 0.16, 1046, 620, 0.28, 0.003, 18, 0.1); return b.toPcm(); }
        case Sfx::Tab: { Buf b(0.09); b.tone(0, 0.09, 980, 1320, 0.22, 0.002, 30, 0.25); return b.toPcm(); }
        case Sfx::Open: {
            Buf b(0.30);
            b.tone(0.00, 0.25, 523.25, 523.25, 0.18, 0.01, 9);
            b.tone(0.04, 0.24, 659.25, 659.25, 0.18, 0.01, 9);
            b.tone(0.08, 0.22, 783.99, 783.99, 0.18, 0.01, 9);
            return b.toPcm();
        }
        case Sfx::Error: {
            Buf b(0.22);
            b.tone(0.00, 0.08, 233, 220, 0.30, 0.002, 12, 0.6);
            b.tone(0.11, 0.10, 233, 207, 0.30, 0.002, 12, 0.6);
            return b.toPcm();
        }
        case Sfx::Startup: {
            Buf b(1.8);
            const double notes[] = {261.63, 329.63, 392.0, 493.88};
            for (int i = 0; i < 4; ++i) b.tone(0.05 * i, 1.7 - 0.05 * i, notes[i], notes[i], 0.13, 0.25, 1.6, 0.05);
            b.tone(0.45, 1.2, 1046.5, 1046.5, 0.07, 0.02, 3.0);
            b.tone(0.60, 1.1, 1567.98, 1567.98, 0.05, 0.02, 3.5);
            b.whoosh(0.0, 0.7, 0.25);
            return b.toPcm();
        }
        case Sfx::Switch: {
            Buf b(0.42);
            b.whoosh(0.0, 0.42, 0.45);
            b.tone(0.18, 0.22, 660, 990, 0.10, 0.02, 10);
            return b.toPcm();
        }
        default: return {};
    }
}

}  // namespace nx
