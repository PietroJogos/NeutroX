// Neutro X - pre-visualizacao do dashboard Metro V1 (inspirado na geracao 2011-2012).
// Fundo claro, categorias horizontais em tipografia leve, grupos de blocos.
#include "dashboards/dashboard.h"
#include "graphics/ui.h"

namespace nx {

void drawMetroV1Preview(IRenderer& r, float time) {
    (void)time;
    const Rectf full{0, 0, float(kLogicalW), float(kLogicalH)};
    r.fillGradientV(full, Color::hex(0xD7D9DC), Color::hex(0xC4C7CB));
    r.fillRadial(640, 330, 760, Color::hex(0xFCFCFC), Color::hex(0xFCFCFC, 0), full);
    // sombra eliptica sob os blocos
    r.fillRadial(640, 540, 520, Color(0, 0, 0, 26), Color(0, 0, 0, 0), Rectf{100, 500, 1080, 80});

    // Perfil
    TextStyle tag;
    tag.size = 22;
    tag.align = Align::Right;
    tag.color = Color(90, 94, 100);
    r.drawText("Jogador", 1110, 50, tag);
    r.fillRect(Rectf{1122, 42, 40, 40}, Color::hex(0x6B8FB8));
    drawIcon(r, Icon::Person, 1142, 64, 30, Color(255, 255, 255, 230));

    // Categorias
    const char* cats[] = {"início", "social", "tv", "vídeo", "jogos", "música", "aplicativos", "configurações"};
    TextStyle cs;
    cs.size = 38;
    cs.weight = FontWeight::Light;
    float x = 70;
    for (int i = 0; i < 8; ++i) {
        bool sel = i == 4;
        cs.color = sel ? Color(70, 74, 80) : Color(160, 164, 170);
        r.drawText(cats[i], x, 112, cs);
        x += r.measureText(cats[i], cs) + 34;
    }

    // Blocos laterais (categorias vizinhas, esmaecidas)
    r.pushAlpha(0.55f);
    drawAbstractArt(r, Rectf{-60, 240, 115, 100}, 3, Color::hex(0x8E6A50), Color::hex(0x4A3020));
    drawAbstractArt(r, Rectf{-60, 344, 115, 100}, 4, Color::hex(0x7090A0), Color::hex(0x304050));
    r.fillRect(Rectf{1225, 240, 115, 100}, Color::hex(0x5DAE2A));
    r.fillRect(Rectf{1225, 344, 115, 100}, Color::hex(0x5DAE2A));
    r.popAlpha();

    // Grupo principal
    const Color green = Color::hex(0x5DAE2A);
    Rectf small1{248, 206, 140, 140}, small2{248, 350, 140, 140};
    Rectf big{392, 206, 380, 284};
    Rectf r1{776, 206, 186, 140}, r2{776, 350, 186, 140};
    r.fillRect(small1, green);
    drawIcon(r, Icon::Controller, small1.cx(), small1.y + 56, 54, Color(255, 255, 255), green);
    TextStyle ts;
    ts.size = 20;
    ts.color = Color(255, 255, 255);
    r.drawText("Meus jogos", small1.x + 10, small1.bottom() - 32, ts);
    drawAbstractArt(r, small2, 21, Color::hex(0xC89A5A), Color::hex(0x5A3A1A));
    r.drawText("Conquistas", small2.x + 10, small2.bottom() - 32, ts);
    drawAbstractArt(r, big, 15574, Color::hex(0x58B4E8), Color::hex(0x1A5A2A));
    r.fillGradientV(Rectf{big.x, big.bottom() - 60, big.w, 60}, Color(0, 0, 0, 0), Color(0, 0, 0, 140));
    ts.size = 26;
    r.drawText("Em destaque", big.x + 14, big.bottom() - 40, ts);
    drawAbstractArt(r, r1, 33, Color::hex(0x5A6070), Color::hex(0x20242C));
    ts.size = 20;
    r.drawText("Aventura", r1.x + 10, r1.bottom() - 32, ts);
    r.fillRect(r2, Color::hex(0x1A1A1A));
    drawIcon(r, Icon::Video, r2.cx(), r2.cy() - 8, 56, Color(235, 60, 50));
    r.drawText("Vídeos", r2.x + 10, r2.bottom() - 32, ts);
    // selecao
    r.strokeRect(big.inset(-4), 4, Color(255, 255, 255));

    // indicador de pagina
    for (int i = 0; i < 3; ++i) r.fillCircle(400.0f + i * 10, 506, i == 1 ? 4.0f : 2.5f, Color(110, 114, 120));
    drawHints(r, {{Button::A, "Selecionar"}, {Button::Y, "Pesquisar"}}, 70, 660, 30, Color(90, 94, 100));
}

}  // namespace nx
