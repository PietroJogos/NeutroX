// Neutro X - pre-visualizacao do dashboard Blades (inspirado na geracao 2005-2008).
// Arte original: laminas verticais com abas giradas, paineis com profundidade,
// verde e cinza. Nenhum recurso dos executaveis originais e usado.
#include <cmath>
#include "dashboards/dashboard.h"
#include "graphics/ui.h"

namespace nx {

void drawBladesPreview(IRenderer& r, float time) {
    const Rectf full{0, 0, float(kLogicalW), float(kLogicalH)};
    // Fundo cinza com "orbe" claro central e aneis.
    r.fillGradientV(full, Color::hex(0x9AA0A6), Color::hex(0x5E646B));
    r.fillRadial(640, 380, 700, Color::hex(0xE4E7EA), Color::hex(0xE4E7EA, 0), full);
    r.strokeCircle(640, 380, 520 + std::sin(time) * 4, 3, Color(255, 255, 255, 40));
    r.strokeCircle(640, 380, 610, 2, Color(255, 255, 255, 25));

    // Laminas seguintes (a direita), em perspectiva.
    const Color next[3] = {Color::hex(0x2E8C6A), Color::hex(0x3F78AE), Color::hex(0x8E969E)};
    for (int i = 2; i >= 0; --i) {
        float x = 1196 + i * 24.0f;
        Vec2 q[4] = {{x, 34.0f + i * 10}, {x + 30, 40.0f + i * 10}, {x + 30, 680.0f - i * 10}, {x, 686.0f - i * 10}};
        r.fillConvex(q, 4, brighten(next[i], 0.1f));
        r.drawLine(x, 34.0f + i * 10, x, 686.0f - i * 10, 2, Color(255, 255, 255, 90));
    }

    // Lamina ativa: Xbox Live (verde).
    const Color green = Color::hex(0x5FAE2A), greenDark = Color::hex(0x2F6E12);
    Rectf blade{96, 22, 1112, 676};
    r.fillRoundRect(blade.offset(6, 8), 26, Color(0, 0, 0, 60));
    r.fillRoundRect(blade, 26, greenDark);
    r.pushClip(blade.inset(4));
    r.fillGradientV(blade, Color::hex(0x7CCB3E), Color::hex(0x3B8A18));
    r.fillRadial(700, 80, 700, Color(255, 255, 255, 70), Color(255, 255, 255, 0), blade);
    r.popClip();
    r.strokeRoundRect(blade, 26, 3, Color(255, 255, 255, 110));

    // Aba vertical da lamina (rotulo girado).
    Rectf tab{96, 22, 58, 676};
    r.fillRoundRect(tab, 24, brighten(green, -0.25f));
    r.fillRect(Rectf{130, 26, 24, 668}, brighten(green, -0.25f));
    r.drawLine(154, 40, 154, 680, 2, Color(255, 255, 255, 80));
    TextStyle tabSt;
    tabSt.size = 30;
    tabSt.rotated90 = true;
    tabSt.color = Color(255, 255, 255, 210);
    tabSt.tracking = 1.5f;
    r.drawText("xbox live", 104, 80, tabSt);

    // Titulo
    TextStyle title;
    title.size = 44;
    title.color = Color(255, 255, 255);
    r.drawText("Xbox Live", 190, 44, title);
    r.drawLine(190, 104, 1170, 104, 2, Color(255, 255, 255, 70));

    // Lista com icones
    struct Item { Icon icon; const char* label; };
    const Item items[] = {{Icon::Person, "Perfil do Jogador"}, {Icon::Friends, "Amigos"}, {Icon::Mail, "Mensagens"},
                          {Icon::Download, "Downloads"}, {Icon::Trophy, "Conquistas"}, {Icon::Gear, "Configurações"}};
    TextStyle it;
    it.size = 30;
    it.color = Color(255, 255, 255);
    for (int i = 0; i < 6; ++i) {
        float y = 118 + i * 82.0f;
        if (i == 0) {
            r.fillGradientH(Rectf{176, y, 520, 74}, Color(255, 255, 255, 150), Color(255, 255, 255, 0));
            r.fillRect(Rectf{176, y + 70, 70, 4}, Color(255, 255, 255, 230));
        }
        drawIcon(r, items[i].icon, 226, y + 37, 44, Color(255, 255, 255, 235), green);
        r.drawText(items[i].label, 290, y + 20, it);
        r.drawLine(176, y + 78, 690, y + 78, 1.5f, Color(255, 255, 255, 50));
    }

    // Painel de destaque a direita (arte propria).
    Rectf feat{722, 120, 450, 470};
    r.fillRoundRect(feat.offset(4, 6), 12, Color(0, 0, 0, 60));
    r.fillRoundRect(feat, 12, Color::hex(0xE9EEF1));
    drawAbstractArt(r, feat.inset(8), 6717, Color::hex(0x3AA0D8), Color::hex(0x1D3F6B));
    TextStyle cap;
    cap.size = 28;
    cap.weight = FontWeight::Bold;
    cap.color = Color(255, 255, 255);
    r.drawText("Destaque da semana", 746, 520, cap);
    cap.size = 20;
    cap.weight = FontWeight::Regular;
    r.drawText("Conteúdo demonstrativo offline", 746, 556, cap);

    // Rodape com dicas
    r.drawLine(176, 614, 1170, 614, 2, Color(255, 255, 255, 60));
    drawHints(r, {{Button::X, "Opções"}}, 190, 652, 34, Color(255, 255, 255));
    drawHints(r, {{Button::B, "Voltar"}, {Button::A, "Selecionar"}}, 1170, 652, 34, Color(255, 255, 255), Align::Right);
}

}  // namespace nx
