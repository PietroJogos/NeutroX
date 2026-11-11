// Neutro X - tema sonoro "Blades" (referencia historica 2.0.6717.0).
// Timbre original: vidro e metal - toques cristalinos, laminas que "cortam o ar".
// Sons sintetizados pelo Neutro X; nao contem audio dos dashboards originais.
#include "audio/synth.h"

namespace nx {

using synth::Buf;

std::vector<int16_t> synthBlades6717(Sfx sfx) {
    switch (sfx) {
        case Sfx::Navigate: {  // toque de vidro curto
            Buf b(0.09);
            b.glass(0, 0.09, 2350, 0.20, 38);
            b.click(0, 0.10, 0.004, 2.0);
            return b.toPcm();
        }
        case Sfx::Confirm: {  // dois sinos ascendentes com ressonancia
            Buf b(0.45);
            b.glass(0.00, 0.40, 1046.5, 0.26, 9);
            b.glass(0.07, 0.38, 1568.0, 0.24, 9);
            return b.toPcm();
        }
        case Sfx::Back: {  // sinos descendentes
            Buf b(0.32);
            b.glass(0.00, 0.28, 1568.0, 0.22, 14);
            b.glass(0.06, 0.26, 1046.5, 0.22, 14);
            return b.toPcm();
        }
        case Sfx::Tab: {  // lamina deslizando: sopro + varredura metalica
            Buf b(0.40);
            b.whoosh(0.0, 0.38, 0.55, 1.4, 0x6717u);
            b.tone(0.02, 0.32, 320, 980, 0.07, 0.03, 5, 0.0, 0.4);
            b.click(0.30, 0.10, 0.006, 1.2);
            return b.toPcm();
        }
        case Sfx::Open: {
            Buf b(0.45);
            b.whoosh(0.0, 0.25, 0.25, 1.2, 0x6718u);
            b.glass(0.08, 0.35, 784.0, 0.18, 8);
            b.glass(0.12, 0.33, 1175.0, 0.16, 8);
            return b.toPcm();
        }
        case Sfx::Error: {  // batida abafada dupla
            Buf b(0.26);
            b.tone(0.00, 0.10, 170, 150, 0.36, 0.002, 18, 0.4);
            b.click(0.00, 0.12, 0.008, 0.6);
            b.tone(0.12, 0.10, 170, 140, 0.36, 0.002, 18, 0.4);
            return b.toPcm();
        }
        case Sfx::Startup: {  // abertura das laminas: sopro + acorde cristalino
            Buf b(1.3);
            b.whoosh(0.0, 0.55, 0.45, 1.3, 0x6719u);
            const double ch[] = {523.25, 659.25, 783.99, 1046.5};
            for (int i = 0; i < 4; ++i) b.glass(0.18 + 0.06 * i, 1.05, ch[i], 0.12, 3.2);
            return b.toPcm();
        }
        case Sfx::Switch: {
            Buf b(0.42);
            b.whoosh(0.0, 0.42, 0.5, 1.3, 0x671Au);
            return b.toPcm();
        }
        default: return {};
    }
}

}  // namespace nx
