// Neutro X - pre-visualizacao do dashboard Kinect (inspirado na geracao 2010-2011).
// Grade de cartoes grandes, ondas roxas/verdes, adaptado ao controle comum.
#include <cmath>
#include "dashboards/dashboard.h"
#include "graphics/ui.h"

namespace nx {

namespace {
// Faixa ondulada: preenche entre duas senoides com fatias convexas.
void waveBand(IRenderer& r, float yTop, float yBot, float amp, float freq, float phase, Color c) {
    const int n = 64;
    for (int i = 0; i < n; ++i) {
        float x0 = 1280.0f * i / n, x1 = 1280.0f * (i + 1) / n + 0.5f;
        auto fy = [&](float x, float base) { return base + amp * std::sin(x * freq + phase); };
        Vec2 q[4] = {{x0, fy(x0, yTop)}, {x1, fy(x1, yTop)}, {x1, fy(x1, yBot)}, {x0, fy(x0, yBot)}};
        r.fillConvex(q, 4, c);
    }
}
}  // namespace

void drawKinectPreview(IRenderer& r, float time) {
    const Rectf full{0, 0, float(kLogicalW), float(kLogicalH)};
    r.fillGradientV(Rectf{0, 0, 1280, 470}, Color::hex(0x2A1546), Color::hex(0x6A4C8E));
    r.fillGradientV(Rectf{0, 470, 1280, 250}, Color::hex(0xDCD9E2), Color::hex(0xF4F3F6));
    r.fillGradientV(Rectf{0, 380, 1280, 120}, Color::hex(0x6A4C8E, 0), Color::hex(0xDCD9E2));
    waveBand(r, -40, 40, 26, 0.004f, time * 0.4f, Color::hex(0x3E8E2E));
    waveBand(r, 10, 64, 30, 0.0035f, 1.3f + time * 0.3f, Color::hex(0x7CC242, 220));
    waveBand(r, 52, 92, 22, 0.005f, 2.4f + time * 0.5f, Color::hex(0x512F7A, 200));
    (void)full;

    // Titulo translucido grande
    TextStyle big;
    big.size = 120;
    big.weight = FontWeight::Bold;
    big.align = Align::Right;
    big.color = Color(255, 255, 255, 55);
    big.tracking = 2;
    r.drawText("INÍCIO", 1130, 52, big);

    // Botao circular de voltar
    r.strokeCircle(86, 84, 44, 4, Color(255, 255, 255, 200));
    drawIcon(r, Icon::ArrowLeft, 86, 84, 44, Color(255, 255, 255, 220));

    struct Tile { const char* label; Color a, b; Icon icon; bool art; };
    const Tile tiles[8] = {
        {"Jogos", Color::hex(0xF08A24), Color::hex(0x8A2C10), Icon::Controller, true},
        {"Música", Color::hex(0x111111), Color::hex(0x2A2A2A), Icon::Music, false},
        {"Vídeos", Color::hex(0xDDEEF8), Color::hex(0x8FC0E0), Icon::Video, false},
        {"Aplicativos", Color::hex(0x3A9AD8), Color::hex(0x1B4F86), Icon::Apps, true},
        {"Perfil", Color::hex(0x8CC63A), Color::hex(0x5E9A1E), Icon::Person, false},
        {"Amigos", Color::hex(0xA8D040), Color::hex(0x6EA020), Icon::Friends, false},
        {"Avatar", Color::hex(0x7BBE3A), Color::hex(0x4F8E1C), Icon::Palette, false},
        {"Configurações", Color::hex(0x2F64B0), Color::hex(0x1F4A8A), Icon::Gear, false},
    };
    for (int i = 0; i < 8; ++i) {
        int col = i % 4, row = i / 4;
        Rectf rc{148.0f + col * 250, 156.0f + row * 196, 236, 180};
        r.fillRect(rc.offset(3, 5), Color(0, 0, 0, 50));
        if (tiles[i].art) drawAbstractArt(r, rc, 13604 + i, tiles[i].a, tiles[i].b);
        else r.fillGradientV(rc, tiles[i].a, tiles[i].b);
        bool light = i == 2;
        Color fg = light ? Color::hex(0x2B5F8A) : Color(255, 255, 255);
        drawIcon(r, tiles[i].icon, rc.cx(), rc.cy() - 10, 64, fg.fade(0.95f), tiles[i].a);
        TextStyle ls;
        ls.size = 28;
        ls.color = fg;
        r.drawText(tiles[i].label, rc.x + 12, rc.y + 10, ls);
        if (i == 0) r.strokeRect(rc.inset(-5), 5, Color(255, 255, 255));
    }
    // Detalhes do cartao de perfil
    TextStyle sm;
    sm.size = 18;
    sm.color = Color(255, 255, 255, 230);
    r.drawText("Jogador360", 160, 392, sm);
    r.drawText("Conectado", 160, 500, sm);

    AvatarLook look;
    look.shirt = Color::hex(0xB03A7A);
    look.hair = Color::hex(0x1E1410);
    look.skin = Color::hex(0x9A6A4A);
    drawAvatar(r, 330, 616, 300, look, time);
    drawIcon(r, Icon::Sensor, 120, 640, 70, Color::hex(0x55595F), Color::hex(0xF0EFF3));
    drawHints(r, {{Button::B, "Voltar"}, {Button::A, "Selecionar"}}, 1134, 670, 32, Color(80, 80, 90), Align::Right);
}

}  // namespace nx
