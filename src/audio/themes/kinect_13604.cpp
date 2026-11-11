// Neutro X - tema sonoro "Kinect" (referencia historica 2.0.13604.0).
// Timbre original: brincalhao, de madeira - notas de marimba.
// Sons sintetizados pelo Neutro X; nao contem audio dos dashboards originais.
#include "audio/synth.h"

namespace nx {

using synth::Buf;

std::vector<int16_t> synthKinect13604(Sfx sfx) {
    switch (sfx) {
        case Sfx::Navigate: { Buf b(0.18); b.marimba(0, 1046.5, 0.22, 22); return b.toPcm(); }
        case Sfx::Confirm: {
            Buf b(0.42);
            b.marimba(0.00, 783.99, 0.26, 12);
            b.marimba(0.08, 1174.66, 0.26, 12);
            return b.toPcm();
        }
        case Sfx::Back: {
            Buf b(0.38);
            b.marimba(0.00, 1174.66, 0.24, 14);
            b.marimba(0.08, 783.99, 0.24, 14);
            return b.toPcm();
        }
        case Sfx::Tab: {  // deslize de madeira
            Buf b(0.30);
            b.marimba(0.00, 659.25, 0.20, 18);
            b.marimba(0.05, 880.0, 0.20, 18);
            b.whoosh(0.0, 0.16, 0.14, 0.9, 0x13604u);
            return b.toPcm();
        }
        case Sfx::Open: {
            Buf b(0.48);
            b.marimba(0.00, 523.25, 0.20, 11);
            b.marimba(0.07, 659.25, 0.20, 11);
            b.marimba(0.14, 783.99, 0.20, 11);
            return b.toPcm();
        }
        case Sfx::Error: {
            Buf b(0.40);
            b.marimba(0.00, 220.0, 0.32, 10);
            b.marimba(0.13, 207.65, 0.32, 10);
            return b.toPcm();
        }
        case Sfx::Startup: {  // arpejo alegre + tapete
            Buf b(1.4);
            const double arp[] = {523.25, 659.25, 783.99, 1046.5, 1318.5};
            for (int i = 0; i < 5; ++i) b.marimba(0.07 * i, arp[i], 0.18, 7);
            b.tone(0.30, 1.05, 261.63, 261.63, 0.08, 0.2, 2.2);
            b.tone(0.30, 1.05, 392.0, 392.0, 0.06, 0.2, 2.2);
            return b.toPcm();
        }
        case Sfx::Switch: {
            Buf b(0.42);
            b.whoosh(0.0, 0.42, 0.42, 1.0, 0x13605u);
            b.marimba(0.22, 987.77, 0.10, 14);
            return b.toPcm();
        }
        default: return {};
    }
}

}  // namespace nx
