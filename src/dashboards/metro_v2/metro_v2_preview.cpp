// Neutro X - pre-visualizacao do dashboard Metro V2 (inspirado na geracao final, 2013-2019).
// Fundo escuro, destaques verdes, grade compacta de blocos.
#include "dashboards/dashboard.h"
#include "graphics/ui.h"

namespace nx {

void drawMetroV2Preview(IRenderer& r, float time) {
    (void)time;
    const Rectf full{0, 0, float(kLogicalW), float(kLogicalH)};
    r.fillGradientV(full, Color::hex(0x3A3D42), Color::hex(0x1B1D20));
    r.fillRadial(560, 200, 700, Color(255, 255, 255, 22), Color(255, 255, 255, 0), full);

    TextStyle tag;
    tag.size = 22;
    tag.align = Align::Right;
    tag.color = Color(225, 228, 232);
    r.drawText("Jogador  ·  0 pontos", 1130, 50, tag);
    r.fillRect(Rectf{1142, 42, 40, 40}, Color::hex(0x3E8FD0));
    drawIcon(r, Icon::Person, 1162, 64, 30, Color(255, 255, 255, 230));

    const char* cats[] = {"início", "jogos", "filmes e tv", "aplicativos", "configurações"};
    TextStyle cs;
    cs.size = 36;
    cs.weight = FontWeight::Light;
    float x = 150;
    for (int i = 0; i < 5; ++i) {
        bool sel = i == 1;
        cs.color = sel ? Color(255, 255, 255) : Color(140, 145, 152);
        cs.weight = sel ? FontWeight::Regular : FontWeight::Light;
        r.drawText(cats[i], x, 116, cs);
        x += r.measureText(cats[i], cs) + 40;
    }

    const Color g = Color::hex(0x1F8A1F), gHi = Color::hex(0x2BA52B);
    struct T { int col, row; Icon icon; const char* label; };
    const T tiles[] = {{0, 0, Icon::Controller, "Meus jogos"}, {1, 0, Icon::Plus, "Complementos"},
                       {0, 1, Icon::Bag, "Explorar jogos"},    {1, 1, Icon::Download, "Demos"},
                       {0, 2, Icon::Search, "Pesquisar"},      {1, 2, Icon::List, "A - Z"}};
    TextStyle ls;
    ls.size = 19;
    ls.color = Color(255, 255, 255);
    for (const T& t : tiles) {
        Rectf rc{268.0f + t.col * 168, 186.0f + t.row * 128, 162, 122};
        bool sel = (t.col == 0 && t.row == 1);
        if (sel) {
            rc = rc.scaledAboutCenter(1.07f);
            r.fillRect(rc.offset(6, 8), Color(0, 0, 0, 110));
        }
        r.fillRect(rc, sel ? gHi : g);
        drawIcon(r, t.icon, rc.cx(), rc.y + 46, 40, Color(255, 255, 255), sel ? gHi : g);
        r.drawText(t.label, rc.x + 10, rc.bottom() - 30, ls);
    }
    Rectf feat{604, 186, 336, 250};
    drawAbstractArt(r, feat, 17559, Color::hex(0xE8E8E8), Color::hex(0x5A5A5A));
    r.fillGradientV(Rectf{feat.x, feat.bottom() - 52, feat.w, 52}, Color(0, 0, 0, 0), Color(0, 0, 0, 170));
    r.drawText("Em destaque: Corrida Neutra", feat.x + 10, feat.bottom() - 32, ls);
    Rectf a{604, 442, 166, 122}, b{774, 442, 166, 122};
    drawAbstractArt(r, a, 51, Color::hex(0xF0C020), Color::hex(0x6A4A10));
    r.drawText("Pista 2", a.x + 10, a.bottom() - 30, ls);
    drawAbstractArt(r, b, 52, Color::hex(0x60A040), Color::hex(0x203A20));
    r.drawText("Cidade Aberta", b.x + 10, b.bottom() - 30, ls);

    // coluna seguinte (esmaecida)
    r.pushAlpha(0.45f);
    r.fillRect(Rectf{1050, 186, 162, 122}, g);
    drawIcon(r, Icon::Video, 1131, 232, 40, Color(255, 255, 255), g);
    r.fillRect(Rectf{1050, 314, 162, 122}, g);
    drawIcon(r, Icon::Search, 1131, 360, 40, Color(255, 255, 255), g);
    r.popAlpha();

    drawHints(r, {{Button::A, "Selecionar"}, {Button::Y, "Pesquisar"}}, 268, 640, 30, Color(225, 228, 232));
}

}  // namespace nx
