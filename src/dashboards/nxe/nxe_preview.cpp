// Neutro X - pre-visualizacao do dashboard NXE (inspirado na geracao 2008-2010).
// Canais empilhados a esquerda, cartoes grandes em profundidade, avatar
// generico proprio e fundo claro esverdeado.
#include <cmath>
#include "dashboards/dashboard.h"
#include "graphics/ui.h"

namespace nx {

void drawNxePreview(IRenderer& r, float time) {
    const Rectf full{0, 0, float(kLogicalW), float(kLogicalH)};
    r.fillGradientV(Rectf{0, 0, 1280, 420}, Color::hex(0x8CC63A), Color::hex(0xE9F1DC));
    r.fillGradientV(Rectf{0, 420, 1280, 300}, Color::hex(0xE4E8EA), Color::hex(0xAEB4BA));
    r.fillRadial(900, 120, 520, Color(255, 255, 255, 120), Color(255, 255, 255, 0), full);
    // bolhas suaves
    for (int i = 0; i < 14; ++i) {
        float x = std::fmod(i * 173.0f + time * 6.0f, 1300.0f);
        float y = 60 + std::fmod(i * 97.0f, 330.0f);
        r.fillCircle(x, y, 10.0f + (i % 4) * 9.0f, Color(255, 255, 255, uint8_t(18 + (i % 3) * 10)));
    }
    // aneis decorativos no chao
    r.strokeCircle(1180, 690, 120, 2, Color(255, 255, 255, 60));
    r.strokeCircle(1180, 690, 170, 2, Color(255, 255, 255, 40));

    // Canais (empilhados, o atual maior)
    TextStyle ch;
    ch.color = Color(255, 255, 255, 110);
    ch.size = 22;
    r.drawText("Configurações", 40, 40, ch);
    ch.size = 26;
    ch.color = Color(255, 255, 255, 160);
    r.drawText("Mídia", 40, 70, ch);
    ch.size = 32;
    ch.color = Color(255, 255, 255, 210);
    r.drawText("Biblioteca de Jogos", 40, 104, ch);
    ch.size = 52;
    ch.color = Color(255, 255, 255);
    ch.weight = FontWeight::Bold;
    r.fillRect(Rectf{22, 164, 9, 9}, Color(255, 255, 255));
    r.drawText("Meu Xbox", 38, 146, ch);

    // Perfil no canto
    TextStyle tag;
    tag.size = 26;
    tag.align = Align::Right;
    tag.color = Color(255, 255, 255);
    r.drawText("Jogador", 1150, 30, tag);
    r.drawText("1245 P", 1150, 62, tag);
    r.fillRoundRect(Rectf{1166, 28, 66, 66}, 6, Color::hex(0xF08A24));
    drawIcon(r, Icon::Person, 1199, 64, 50, Color(255, 255, 255, 220));

    // Cartoes em profundidade (do fundo para frente)
    struct Card { float x, y, w, h; Icon icon; const char* label; };
    const Card cards[] = {{1180, 322, 100, 140, Icon::Picture, ""},
                          {1048, 310, 150, 166, Icon::Music, ""},
                          {846, 296, 220, 206, Icon::Video, "Mídia"},
                          {502, 272, 352, 262, Icon::Controller, "Biblioteca de Jogos"}};
    for (const Card& c : cards) {
        Rectf rc{c.x, c.y, c.w, c.h};
        r.fillRoundRect(rc.offset(3, 6), 6, Color(0, 0, 0, 40));
        r.fillRoundRect(rc, 6, Color::hex(0x6FB12A));
        r.pushClip(rc);
        r.fillGradientV(rc, Color::hex(0xC5E453), Color::hex(0x5AA01E));
        r.fillCircle(rc.x + rc.w * 0.3f, rc.y + rc.h * 0.55f, rc.h * 0.45f, Color(255, 255, 255, 18));
        r.popClip();
        drawIcon(r, c.icon, rc.cx(), rc.y + rc.h * 0.42f, rc.h * 0.36f, Color(255, 255, 255, 235), Color::hex(0x8BC43A));
        if (*c.label) {
            TextStyle ls;
            ls.size = c.h > 250 ? 26 : 18;
            ls.color = Color(255, 255, 255);
            r.drawText(c.label, rc.x + 14, rc.bottom() - ls.size - 12, ls);
        }
    }
    // Cartao principal (perfil) com avatar
    Rectf main{36, 236, 466, 352};
    r.fillRoundRect(main.offset(4, 8), 8, Color(0, 0, 0, 50));
    r.fillRoundRect(main, 8, Color::hex(0x6BB024));
    r.pushClip(main);
    r.fillGradientV(main, Color::hex(0xD3EC5A), Color::hex(0x4E9418));
    r.strokeCircle(main.x + 90, main.y + 250, 70, 6, Color(255, 255, 255, 25));
    r.popClip();
    TextStyle ps;
    ps.size = 32;
    ps.color = Color(255, 255, 255);
    r.drawText("Jogador", 56, 258, ps);
    r.drawText("1245 P", 56, 298, ps);
    ps.size = 22;
    r.drawText("Jogados recentemente", 56, 392, ps);
    const Color thumbs[4] = {Color::hex(0xE85A2A), Color::hex(0x2A6FD8), Color::hex(0x1F1F1F), Color::hex(0xD8B02A)};
    for (int i = 0; i < 4; ++i) {
        Rectf t{56.0f + i * 70, 424, 60, 60};
        drawAbstractArt(r, t, 900 + i, thumbs[i], brighten(thumbs[i], -0.5f));
        r.strokeRect(t, 2, Color(255, 255, 255, 150));
    }
    AvatarLook look;
    drawAvatar(r, 420, 640, 430, look, time);

    TextStyle cnt;
    cnt.size = 20;
    cnt.color = Color(90, 96, 104);
    r.drawText("1 de 8", 40, 600, cnt);
    drawHints(r, {{Button::A, "Selecionar"}}, 40, 680, 32, Color(70, 76, 84));
}

}  // namespace nx
