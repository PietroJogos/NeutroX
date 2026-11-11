// Neutro X - tema sonoro "Metro V2" (referencia historica 2.0.17559.0).
// Timbre original: escuro e macio - toques graves "abafados", tons curtos.
// Sons sintetizados pelo Neutro X; nao contem audio dos dashboards originais.
#include "audio/synth.h"

namespace nx {

using synth::Buf;

std::vector<int16_t> synthMetroV2_17559(Sfx sfx) {
    switch (sfx) {
        case Sfx::Navigate: {  // "toc" grave
            Buf b(0.05);
            b.tone(0, 0.05, 620, 420, 0.28, 0.001, 70);
            b.click(0, 0.12, 0.004, 0.7, 0x17559u);
            return b.toPcm();
        }
        case Sfx::Confirm: {
            Buf b(0.16);
            b.tone(0.00, 0.08, 990, 990, 0.22, 0.002, 34);
            b.tone(0.04, 0.12, 1320, 1320, 0.20, 0.002, 30);
            return b.toPcm();
        }
        case Sfx::Back: {
            Buf b(0.14);
            b.tone(0.00, 0.07, 880, 880, 0.22, 0.002, 36);
            b.tone(0.04, 0.10, 660, 660, 0.22, 0.002, 32);
            return b.toPcm();
        }
        case Sfx::Tab: {  // varrida curta e escura
            Buf b(0.14);
            b.whoosh(0.0, 0.13, 0.42, 0.6, 0x1755Au);
            b.tone(0.0, 0.05, 520, 380, 0.10, 0.001, 60);
            return b.toPcm();
        }
        case Sfx::Open: {
            Buf b(0.20);
            b.tone(0.00, 0.10, 660, 660, 0.18, 0.002, 26);
            b.tone(0.05, 0.14, 990, 990, 0.18, 0.002, 22);
            return b.toPcm();
        }
        case Sfx::Error: {
            Buf b(0.18);
            b.tone(0, 0.18, 200, 170, 0.34, 0.002, 16, 0.5);
            b.click(0, 0.12, 0.006, 0.5, 0x1755Bu);
            return b.toPcm();
        }
        case Sfx::Startup: {  // tapete escuro + sino
            Buf b(1.1);
            const double ch[] = {220.0, 329.63, 440.0};
            for (int i = 0; i < 3; ++i) b.tone(0.0, 1.0, ch[i], ch[i], 0.11, 0.15, 2.6, 0.05);
            b.tone(0.25, 0.8, 880.0, 880.0, 0.09, 0.003, 4.0);
            b.whoosh(0.0, 0.35, 0.22, 0.6, 0x1755Cu);
            return b.toPcm();
        }
        case Sfx::Switch: {
            Buf b(0.32);
            b.whoosh(0.0, 0.32, 0.40, 0.6, 0x1755Du);
            return b.toPcm();
        }
        default: return {};
    }
}

}  // namespace nx
