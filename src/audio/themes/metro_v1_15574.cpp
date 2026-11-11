// Neutro X - tema sonoro "Metro V1" (referencia historica 2.0.15574.0).
// Timbre original: minimalista e nitido - cliques secos e tons curtos e agudos.
// Sons sintetizados pelo Neutro X; nao contem audio dos dashboards originais.
#include "audio/synth.h"

namespace nx {

using synth::Buf;

std::vector<int16_t> synthMetroV1_15574(Sfx sfx) {
    switch (sfx) {
        case Sfx::Navigate: {
            Buf b(0.04);
            b.click(0, 0.30, 0.005, 2.2, 0x15574u);
            b.tone(0, 0.025, 2900, 2800, 0.10, 0.001, 120);
            return b.toPcm();
        }
        case Sfx::Confirm: {
            Buf b(0.12);
            b.click(0, 0.25, 0.005, 2.0, 0x15575u);
            b.tone(0, 0.12, 1760, 1760, 0.24, 0.001, 32, 0.15);
            return b.toPcm();
        }
        case Sfx::Back: {
            Buf b(0.12);
            b.click(0, 0.22, 0.005, 1.4, 0x15576u);
            b.tone(0, 0.12, 1318.5, 1250, 0.22, 0.001, 32, 0.1);
            return b.toPcm();
        }
        case Sfx::Tab: {  // varrida rapida e brilhante
            Buf b(0.14);
            b.whoosh(0.0, 0.13, 0.42, 2.0, 0x15577u);
            return b.toPcm();
        }
        case Sfx::Open: {
            Buf b(0.18);
            b.tone(0.00, 0.08, 1318.5, 1318.5, 0.20, 0.001, 40);
            b.tone(0.05, 0.12, 1760.0, 1760.0, 0.20, 0.001, 34);
            return b.toPcm();
        }
        case Sfx::Error: {
            Buf b(0.16);
            b.tone(0, 0.16, 440, 415, 0.26, 0.002, 18, 0.3, 0.25);
            return b.toPcm();
        }
        case Sfx::Startup: {  // sino limpo com varrida leve
            Buf b(0.9);
            b.whoosh(0.0, 0.3, 0.28, 1.8, 0x15578u);
            b.tone(0.10, 0.75, 880.0, 880.0, 0.16, 0.003, 4.5);
            b.tone(0.16, 0.70, 1318.5, 1318.5, 0.13, 0.003, 4.8);
            return b.toPcm();
        }
        case Sfx::Switch: {
            Buf b(0.30);
            b.whoosh(0.0, 0.30, 0.40, 1.8, 0x15579u);
            return b.toPcm();
        }
        default: return {};
    }
}

}  // namespace nx
