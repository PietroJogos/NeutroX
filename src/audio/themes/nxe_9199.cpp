// Neutro X - tema sonoro "NXE" (referencia historica 2.0.9199.0).
// Timbre original: leve e "aerado" - bolhas que sobem, sopros suaves.
// Sons sintetizados pelo Neutro X; nao contem audio dos dashboards originais.
#include "audio/synth.h"

namespace nx {

using synth::Buf;

std::vector<int16_t> synthNxe9199(Sfx sfx) {
    switch (sfx) {
        case Sfx::Navigate: {  // bolha: glissando ascendente curtissimo
            Buf b(0.07);
            b.tone(0, 0.07, 720, 1150, 0.24, 0.002, 40);
            return b.toPcm();
        }
        case Sfx::Confirm: {  // duas bolhas ascendentes
            Buf b(0.24);
            b.tone(0.00, 0.10, 640, 860, 0.26, 0.002, 24);
            b.tone(0.07, 0.14, 960, 1290, 0.26, 0.002, 18);
            return b.toPcm();
        }
        case Sfx::Back: {  // bolha descendo
            Buf b(0.16);
            b.tone(0, 0.16, 1150, 560, 0.26, 0.002, 18);
            return b.toPcm();
        }
        case Sfx::Tab: {  // troca de canal: sopro curto + tom subindo
            Buf b(0.24);
            b.whoosh(0.0, 0.22, 0.32, 0.8, 0x9199u);
            b.tone(0.03, 0.18, 480, 880, 0.10, 0.02, 10);
            return b.toPcm();
        }
        case Sfx::Open: {  // tres bolhas em escada
            Buf b(0.34);
            b.tone(0.00, 0.10, 520, 700, 0.20, 0.002, 26);
            b.tone(0.07, 0.10, 700, 940, 0.20, 0.002, 26);
            b.tone(0.14, 0.16, 940, 1260, 0.20, 0.002, 20);
            return b.toPcm();
        }
        case Sfx::Error: {  // oscilacao grave suave
            Buf b(0.26);
            b.tone(0.00, 0.11, 320, 280, 0.28, 0.004, 14, 0.3);
            b.tone(0.12, 0.12, 300, 250, 0.28, 0.004, 14, 0.3);
            return b.toPcm();
        }
        case Sfx::Startup: {  // tapete de acorde arejado (Fa maior)
            Buf b(1.5);
            b.whoosh(0.0, 0.8, 0.30, 0.7, 0x919Au);
            const double ch[] = {349.23, 440.0, 523.25, 698.46};
            for (int i = 0; i < 4; ++i) b.tone(0.05 * i, 1.4 - 0.05 * i, ch[i], ch[i], 0.12, 0.18, 2.0);
            b.tone(0.35, 0.18, 880, 1320, 0.10, 0.002, 16);  // bolha final
            return b.toPcm();
        }
        case Sfx::Switch: {
            Buf b(0.42);
            b.whoosh(0.0, 0.42, 0.45, 0.8, 0x919Bu);
            b.tone(0.15, 0.24, 520, 880, 0.08, 0.03, 8);
            return b.toPcm();
        }
        default: return {};
    }
}

}  // namespace nx
