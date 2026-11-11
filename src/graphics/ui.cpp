// Neutro X - elementos de interface. Ver ui.h.
#include "graphics/ui.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>
#include "dashboards/dashboard.h"

namespace nx {

void strokeArc(IRenderer& r, float cx, float cy, float radius, float a0, float a1, float th, Color c) {
    int steps = std::max(6, int(std::fabs(a1 - a0) / 8.0f));
    float px = 0, py = 0;
    for (int i = 0; i <= steps; ++i) {
        float a = (a0 + (a1 - a0) * i / steps) * kPi / 180.0f;
        float x = cx + radius * std::cos(a), y = cy + radius * std::sin(a);
        if (i > 0) r.drawLine(px, py, x, y, th, c);
        px = x;
        py = y;
    }
    // pontas arredondadas
    r.fillCircle(cx + radius * std::cos(a0 * kPi / 180.0f), cy + radius * std::sin(a0 * kPi / 180.0f), th * 0.5f, c);
    r.fillCircle(cx + radius * std::cos(a1 * kPi / 180.0f), cy + radius * std::sin(a1 * kPi / 180.0f), th * 0.5f, c);
}

void drawIcon(IRenderer& r, Icon icon, float cx, float cy, float size, Color col, Color bg) {
    const float h = size * 0.5f;
    const bool cut = bg.a > 0;
    auto X = [&](float v) { return cx + v * h; };
    auto Y = [&](float v) { return cy + v * h; };
    auto R = [&](float x, float y, float w, float hh) { return Rectf{X(x), Y(y), w * h, hh * h}; };
    auto poly = [&](std::initializer_list<Vec2> pts, Color c) {
        std::vector<Vec2> v;
        for (const Vec2& p : pts) v.push_back(Vec2{X(p.x), Y(p.y)});
        r.fillConvex(v.data(), int(v.size()), c);
    };
    auto line = [&](float x0, float y0, float x1, float y1, float th, Color c) { r.drawLine(X(x0), Y(y0), X(x1), Y(y1), th * h, c); };
    auto person = [&](float ox, float oy, float s, Color c) {
        r.fillCircle(X(ox), Y(oy - 0.45f * s), 0.32f * s * h, c);
        std::vector<Vec2> v;
        for (int i = 0; i <= 12; ++i) {
            float a = kPi + kPi * i / 12.0f;
            v.push_back(Vec2{X(ox + 0.68f * s * std::cos(a)), Y(oy + 0.9f * s + 0.8f * s * std::sin(a))});
        }
        r.fillConvex(v.data(), int(v.size()), c);
    };

    switch (icon) {
        case Icon::Controller:
            r.fillRoundRect(R(-0.95f, -0.5f, 1.9f, 0.8f), 0.35f * h, col);
            r.fillCircle(X(-0.6f), Y(0.28f), 0.38f * h, col);
            r.fillCircle(X(0.6f), Y(0.28f), 0.38f * h, col);
            if (cut) {
                r.fillRect(R(-0.68f, -0.2f, 0.36f, 0.12f), bg);
                r.fillRect(R(-0.56f, -0.32f, 0.12f, 0.36f), bg);
                r.fillCircle(X(0.5f), Y(-0.28f), 0.08f * h, bg);
                r.fillCircle(X(0.5f), Y(0.0f), 0.08f * h, bg);
                r.fillCircle(X(0.36f), Y(-0.14f), 0.08f * h, bg);
                r.fillCircle(X(0.64f), Y(-0.14f), 0.08f * h, bg);
            }
            break;
        case Icon::Music:
            r.fillCircle(X(-0.45f), Y(0.55f), 0.26f * h, col);
            r.fillCircle(X(0.48f), Y(0.4f), 0.26f * h, col);
            r.fillRect(R(-0.27f, -0.62f, 0.13f, 1.17f), col);
            r.fillRect(R(0.66f, -0.78f, 0.13f, 1.18f), col);
            poly({{-0.27f, -0.62f}, {0.79f, -0.8f}, {0.79f, -0.52f}, {-0.27f, -0.34f}}, col);
            break;
        case Icon::Video:
            r.fillRoundRect(R(-0.9f, -0.2f, 1.25f, 0.8f), 0.12f * h, col);
            poly({{0.3f, 0.2f}, {0.95f, -0.2f}, {0.95f, 0.6f}}, col);
            r.fillCircle(X(-0.55f), Y(-0.48f), 0.27f * h, col);
            r.fillCircle(X(0.02f), Y(-0.48f), 0.27f * h, col);
            break;
        case Icon::Gear:
            for (int i = 0; i < 8; ++i) {
                float a = i * kPi / 4.0f, ca = std::cos(a), sa = std::sin(a);
                float w = 0.17f;
                poly({{ca * 0.5f - sa * w, sa * 0.5f + ca * w}, {ca * 0.95f - sa * w, sa * 0.95f + ca * w},
                      {ca * 0.95f + sa * w, sa * 0.95f - ca * w}, {ca * 0.5f + sa * w, sa * 0.5f - ca * w}}, col);
            }
            r.fillCircle(cx, cy, 0.7f * h, col);
            if (cut) r.fillCircle(cx, cy, 0.28f * h, bg);
            break;
        case Icon::Person:
            person(0, 0, 1.0f, col);
            break;
        case Icon::Friends:
            person(-0.42f, 0.05f, 0.82f, col.fade(0.7f));
            person(0.3f, 0, 1.0f, col);
            break;
        case Icon::Family:
            person(-0.35f, 0, 1.0f, col);
            person(0.5f, 0.3f, 0.7f, col);
            break;
        case Icon::Bag:
            poly({{-0.75f, -0.3f}, {0.75f, -0.3f}, {0.85f, 0.9f}, {-0.85f, 0.9f}}, col);
            strokeArc(r, cx, Y(-0.3f), 0.42f * h, 180, 360, 0.15f * h, col);
            break;
        case Icon::Search:
            r.strokeCircle(X(-0.18f), Y(-0.18f), 0.52f * h, 0.2f * h, col);
            line(0.2f, 0.2f, 0.85f, 0.85f, 0.26f, col);
            break;
        case Icon::List:
            for (int i = 0; i < 3; ++i) {
                float y = -0.6f + i * 0.6f;
                r.fillCircle(X(-0.75f), Y(y), 0.13f * h, col);
                r.fillRect(R(-0.45f, y - 0.09f, 1.3f, 0.18f), col);
            }
            break;
        case Icon::Download:
            r.fillRect(R(-0.15f, -0.9f, 0.3f, 0.9f), col);
            poly({{-0.6f, -0.05f}, {0.6f, -0.05f}, {0.0f, 0.55f}}, col);
            r.fillRect(R(-0.85f, 0.72f, 1.7f, 0.18f), col);
            break;
        case Icon::Star: {
            Vec2 outer[5], inner[5];
            for (int k = 0; k < 5; ++k) {
                float ao = (-90.0f + 72.0f * k) * kPi / 180.0f, ai = (-90.0f + 36.0f + 72.0f * k) * kPi / 180.0f;
                outer[k] = Vec2{X(std::cos(ao)), Y(std::sin(ao) + 0.08f)};
                inner[k] = Vec2{X(0.42f * std::cos(ai)), Y(0.42f * std::sin(ai) + 0.08f)};
            }
            r.fillConvex(inner, 5, col);
            for (int k = 0; k < 5; ++k) {
                Vec2 tri[3] = {outer[k], inner[k], inner[(k + 4) % 5]};
                r.fillConvex(tri, 3, col);
            }
            break;
        }
        case Icon::Home:
            poly({{-1.0f, 0.02f}, {0.0f, -0.95f}, {1.0f, 0.02f}}, col);
            r.fillRect(R(-0.7f, -0.05f, 1.4f, 0.95f), col);
            if (cut) r.fillRect(R(-0.18f, 0.35f, 0.36f, 0.55f), bg);
            break;
        case Icon::Globe:
            r.strokeCircle(cx, cy, 0.85f * h, 0.14f * h, col);
            line(-0.85f, 0, 0.85f, 0, 0.12f, col);
            line(-0.7f, -0.45f, 0.7f, -0.45f, 0.1f, col);
            line(-0.7f, 0.45f, 0.7f, 0.45f, 0.1f, col);
            r.strokeCircle(cx, cy, 0.38f * h, 0.11f * h, col);
            line(0, -0.85f, 0, 0.85f, 0.12f, col);
            break;
        case Icon::Power:
            strokeArc(r, cx, Y(0.06f), 0.75f * h, -55, 235, 0.17f * h, col);
            line(0, -0.95f, 0, -0.12f, 0.18f, col);
            break;
        case Icon::Info:
            if (cut) {
                r.fillCircle(cx, cy, 0.95f * h, col);
                r.fillCircle(cx, Y(-0.45f), 0.13f * h, bg);
                r.fillRect(R(-0.1f, -0.2f, 0.2f, 0.75f), bg);
            } else {
                r.strokeCircle(cx, cy, 0.88f * h, 0.14f * h, col);
                r.fillCircle(cx, Y(-0.42f), 0.12f * h, col);
                r.fillRect(R(-0.09f, -0.18f, 0.18f, 0.7f), col);
            }
            break;
        case Icon::Apps:
            for (int i = 0; i < 4; ++i)
                r.fillRoundRect(R(i % 2 ? 0.08f : -0.88f, i / 2 ? 0.08f : -0.88f, 0.8f, 0.8f), 0.12f * h, col);
            break;
        case Icon::Picture:
            r.strokeRect(R(-0.9f, -0.72f, 1.8f, 1.44f), 0.14f * h, col);
            poly({{-0.68f, 0.56f}, {-0.15f, -0.2f}, {0.38f, 0.56f}}, col);
            poly({{0.08f, 0.56f}, {0.42f, 0.06f}, {0.72f, 0.56f}}, col);
            r.fillCircle(X(0.45f), Y(-0.35f), 0.15f * h, col);
            break;
        case Icon::Disc:
            r.fillCircle(cx, cy, 0.95f * h, col);
            if (cut) {
                r.strokeCircle(cx, cy, 0.62f * h, 0.05f * h, bg.fade(0.6f));
                r.fillCircle(cx, cy, 0.28f * h, bg);
            }
            break;
        case Icon::Plus:
            r.fillRect(R(-0.85f, -0.13f, 1.7f, 0.26f), col);
            r.fillRect(R(-0.13f, -0.85f, 0.26f, 1.7f), col);
            break;
        case Icon::ArrowLeft:
            line(-0.75f, 0, 0.8f, 0, 0.2f, col);
            line(-0.8f, 0, -0.25f, -0.55f, 0.2f, col);
            line(-0.8f, 0, -0.25f, 0.55f, 0.2f, col);
            break;
        case Icon::Trophy:
            poly({{-0.65f, -0.85f}, {0.65f, -0.85f}, {0.55f, -0.2f}, {0.2f, 0.2f}, {-0.2f, 0.2f}, {-0.55f, -0.2f}}, col);
            r.fillRect(R(-0.1f, 0.15f, 0.2f, 0.42f), col);
            r.fillRoundRect(R(-0.5f, 0.55f, 1.0f, 0.28f), 0.06f * h, col);
            strokeArc(r, X(-0.62f), Y(-0.5f), 0.25f * h, 90, 270, 0.12f * h, col);
            strokeArc(r, X(0.62f), Y(-0.5f), 0.25f * h, -90, 90, 0.12f * h, col);
            break;
        case Icon::Mail:
            r.strokeRect(R(-0.9f, -0.62f, 1.8f, 1.24f), 0.14f * h, col);
            line(-0.82f, -0.55f, 0, 0.12f, 0.14f, col);
            line(0, 0.12f, 0.82f, -0.55f, 0.14f, col);
            break;
        case Icon::Tv:
            r.strokeRoundRect(R(-0.95f, -0.75f, 1.9f, 1.25f), 0.12f * h, 0.14f * h, col);
            r.fillRect(R(-0.35f, 0.66f, 0.7f, 0.14f), col);
            break;
        case Icon::Storage:
            r.fillRoundRect(R(-0.95f, -0.45f, 1.9f, 0.9f), 0.15f * h, col);
            if (cut) {
                r.fillCircle(X(0.62f), cy, 0.1f * h, bg);
                r.fillRect(R(-0.7f, -0.05f, 0.9f, 0.1f), bg);
            }
            break;
        case Icon::Clock:
            r.strokeCircle(cx, cy, 0.85f * h, 0.14f * h, col);
            line(0, 0, 0, -0.55f, 0.13f, col);
            line(0, 0, 0.4f, 0.0f, 0.13f, col);
            break;
        case Icon::Sensor:
            r.fillRoundRect(R(-0.95f, -0.4f, 1.9f, 0.52f), 0.22f * h, col);
            if (cut) {
                r.fillCircle(X(-0.45f), Y(-0.14f), 0.12f * h, bg);
                r.fillCircle(X(0.45f), Y(-0.14f), 0.12f * h, bg);
            }
            r.fillRect(R(-0.08f, 0.1f, 0.16f, 0.45f), col);
            r.fillRoundRect(R(-0.42f, 0.55f, 0.84f, 0.16f), 0.06f * h, col);
            break;
        case Icon::Palette:
            r.fillCircle(cx, cy, 0.9f * h, col);
            if (cut) {
                r.fillCircle(X(-0.35f), Y(-0.4f), 0.13f * h, bg);
                r.fillCircle(X(0.15f), Y(-0.5f), 0.13f * h, bg);
                r.fillCircle(X(-0.55f), Y(0.1f), 0.13f * h, bg);
                r.fillCircle(X(0.42f), Y(0.42f), 0.24f * h, bg);
            }
            break;
        case Icon::Speaker:
            r.fillRect(R(-0.85f, -0.3f, 0.5f, 0.6f), col);
            poly({{-0.4f, -0.3f}, {0.1f, -0.75f}, {0.1f, 0.75f}, {-0.4f, 0.3f}}, col);
            strokeArc(r, X(0.1f), cy, 0.42f * h, -45, 45, 0.12f * h, col);
            strokeArc(r, X(0.1f), cy, 0.78f * h, -45, 45, 0.12f * h, col);
            break;
        case Icon::Network:
            r.fillCircle(cx, Y(0.62f), 0.13f * h, col);
            strokeArc(r, cx, Y(0.62f), 0.42f * h, -135, -45, 0.13f * h, col);
            strokeArc(r, cx, Y(0.62f), 0.82f * h, -135, -45, 0.13f * h, col);
            strokeArc(r, cx, Y(0.62f), 1.22f * h, -130, -50, 0.13f * h, col);
            break;
        default:
            r.fillRoundRect(R(-0.8f, -0.8f, 1.6f, 1.6f), 0.2f * h, col);
            break;
    }
}

float drawButtonGlyph(IRenderer& r, Button b, float x, float cy, float size) {
    TextStyle st;
    st.weight = FontWeight::Bold;
    st.align = Align::Center;
    const char* letter = nullptr;
    Color fill;
    switch (b) {
        case Button::A: letter = "A"; fill = Color::hex(0x5FB22E); break;
        case Button::B: letter = "B"; fill = Color::hex(0xD8323C); break;
        case Button::X: letter = "X"; fill = Color::hex(0x2F7FD8); break;
        case Button::Y: letter = "Y"; fill = Color::hex(0xE2AE1C); break;
        default: break;
    }
    if (letter) {
        float rad = size * 0.5f;
        r.fillCircle(x + rad, cy + size * 0.04f, rad, Color(0, 0, 0, 90));
        r.fillCircle(x + rad, cy, rad, fill);
        r.fillCircle(x + rad, cy - rad * 0.35f, rad * 0.55f, Color(255, 255, 255, 40));
        st.size = size * 0.66f;
        st.color = (b == Button::Y) ? Color(50, 35, 0) : Color(255, 255, 255);
        r.drawText(letter, x + rad, textTopForCenter(cy, st.size), st);
        return size;
    }
    const char* label = "";
    switch (b) {
        case Button::LB: label = "LB"; break;
        case Button::RB: label = "RB"; break;
        case Button::LT: label = "LT"; break;
        case Button::RT: label = "RT"; break;
        case Button::Start: label = "START"; break;
        case Button::Back: label = "BACK"; break;
        case Button::Up: label = "\xE2\x86\x91"; break;     // seta cima
        case Button::Down: label = "\xE2\x86\x93"; break;   // seta baixo
        case Button::Left: label = "\xE2\x86\x90"; break;   // seta esquerda
        case Button::Right: label = "\xE2\x86\x92"; break;  // seta direita
        default: break;
    }
    st.size = size * 0.52f;
    st.color = Color(255, 255, 255);
    float tw = r.measureText(label, st);
    float w = std::max(size, tw + size * 0.6f);
    Rectf pill{x, cy - size * 0.5f, w, size};
    r.fillRoundRect(pill.offset(0, size * 0.04f), size * 0.3f, Color(0, 0, 0, 90));
    r.fillRoundRect(pill, size * 0.3f, Color(78, 82, 90));
    r.drawText(label, pill.cx(), textTopForCenter(cy, st.size), st);
    return w;
}

float drawHints(IRenderer& r, std::initializer_list<Hint> hints, float x, float cy, float size, Color textColor,
                Align align) {
    TextStyle st;
    st.size = size * 0.72f;
    st.color = textColor;
    const float gapIn = size * 0.3f, gapOut = size * 0.9f;
    // mede
    float total = 0;
    for (const Hint& h : hints) {
        float gw = (h.button == Button::A || h.button == Button::B || h.button == Button::X || h.button == Button::Y)
                       ? size
                       : 0;
        if (!gw) {
            // largura da pilula
            TextStyle ps;
            ps.size = size * 0.52f;
            ps.weight = FontWeight::Bold;
            gw = size;  // aproximacao; recalculada ao desenhar
            const char* l = h.button == Button::Start ? "START" : (h.button == Button::Back ? "BACK" : "LB");
            gw = std::max(size, r.measureText(l, ps) + size * 0.6f);
        }
        total += gw + gapIn + r.measureText(h.label, st) + gapOut;
    }
    total -= gapOut;
    float px = align == Align::Right ? x - total : (align == Align::Center ? x - total * 0.5f : x);
    for (const Hint& h : hints) {
        px += drawButtonGlyph(r, h.button, px, cy, size) + gapIn;
        r.drawText(h.label, px, textTopForCenter(cy, st.size), st);
        px += r.measureText(h.label, st) + gapOut;
    }
    return total;
}

void drawNeutroLogo(IRenderer& r, float x, float y, float size, Color accent, float progress) {
    TextStyle st;
    st.size = size;
    st.weight = FontWeight::Bold;
    st.tracking = 2.5f;
    st.color = Color(240, 244, 248);
    float p = clampf(progress, 0, 1);
    r.pushAlpha(clampf(p * 1.6f, 0, 1));
    r.drawText("NEUTRO", x, y, st);
    r.popAlpha();
    float w = r.measureText("NEUTRO", st);
    // "X" proprio: dois tracos que se cruzam, com brilho.
    float cap = size * 10.0f / 16.0f;
    float bx = x + w + size * 0.18f, by = y + size * 2.0f / 16.0f;
    float xs = cap * 1.05f;
    float t1 = clampf(p * 2.0f - 0.2f, 0, 1), t2 = clampf(p * 2.0f - 0.8f, 0, 1);
    float th = size * 0.17f;
    r.fillCircle(bx + xs * 0.5f, by + cap * 0.5f, cap * 0.9f, accent.withAlpha(uint8_t(28 * p)));
    if (t1 > 0) r.drawLine(bx, by, bx + xs * t1, by + cap * t1, th, accent);
    if (t2 > 0) r.drawLine(bx + xs, by, bx + xs - xs * t2, by + cap * t2, th, brighten(accent, 0.35f));
}

void drawSpinner(IRenderer& r, float cx, float cy, float radius, float time, Color c) {
    for (int i = 0; i < 10; ++i) {
        float a = time * 4.0f + i * (2 * kPi / 10.0f);
        float k = float(i) / 9.0f;
        r.fillCircle(cx + std::cos(a) * radius, cy + std::sin(a) * radius, radius * (0.08f + 0.1f * k), c.fade(0.15f + 0.85f * k));
    }
}

float drawWrapped(IRenderer& r, const char* utf8, float x, float y, float width, const TextStyle& st, float lineHeight,
                  int maxLines) {
    std::string text(utf8 ? utf8 : "");
    std::string line, word;
    float yy = y;
    int lines = 0;
    auto flush = [&]() {
        if (lines < maxLines) r.drawText(line.c_str(), x, yy, st);
        yy += lineHeight;
        ++lines;
        line.clear();
    };
    size_t i = 0;
    while (i <= text.size()) {
        char ch = i < text.size() ? text[i] : '\0';
        if (ch == ' ' || ch == '\n' || ch == '\0') {
            if (!word.empty()) {
                std::string cand = line.empty() ? word : line + " " + word;
                if (!line.empty() && r.measureText(cand.c_str(), st) > width) {
                    flush();
                    line = word;
                } else {
                    line = cand;
                }
                word.clear();
            }
            if (ch == '\n') flush();
            if (ch == '\0') break;
        } else {
            word += ch;
        }
        ++i;
    }
    if (!line.empty()) flush();
    return yy - y;
}

void drawNeutroBackground(IRenderer& r, Color accent) {
    const Rectf full{0, 0, float(kLogicalW), float(kLogicalH)};
    r.fillGradientV(full, Color::hex(0x0B1016), Color::hex(0x151D27));
    r.fillRadial(1080, 40, 760, accent.withAlpha(46), accent.withAlpha(0), full);
    r.fillRadial(120, 760, 620, Color::hex(0x2A3B52, 60), Color::hex(0x2A3B52, 0), full);
    // grade de pontos sutil
    for (int y = 0; y < kLogicalH; y += 32)
        for (int x = (y / 32) % 2 ? 16 : 0; x < kLogicalW; x += 32) r.fillRect(Rectf{float(x), float(y), 2, 2}, Color(255, 255, 255, 10));
    // vinheta inferior
    r.fillGradientV(Rectf{0, 560, float(kLogicalW), 160}, Color(0, 0, 0, 0), Color(0, 0, 0, 110));
}

void drawNeutroBands(IRenderer& r, float time, Color accent) {
    // Faixas diagonais translucidas que deslizam lentamente.
    for (int i = 0; i < 3; ++i) {
        float speed = 18.0f + i * 9.0f;
        float base = std::fmod(time * speed + i * 520.0f, 2000.0f) - 400.0f;
        float w = 90.0f + i * 50.0f;
        Vec2 q[4] = {{base, 0}, {base + w, 0}, {base + w - 420, float(kLogicalH)}, {base - 420, float(kLogicalH)}};
        r.fillConvex(q, 4, accent.withAlpha(uint8_t(10 + i * 4)));
    }
}

void drawAvatar(IRenderer& r, float cx, float footY, float height, const AvatarLook& L, float time, float wave) {
    // Proporcoes estilizadas (cabeca grande), unidade u = altura/10.
    const float u = height / 10.0f;
    const float sway = std::sin(time * 1.6f) * 0.06f * u;  // respiracao/balanco leve
    const float top = footY - height;
    // sombra no chao
    {
        std::vector<Vec2> sh;
        for (int i = 0; i < 16; ++i) {
            float a = 2 * kPi * i / 16;
            sh.push_back(Vec2{cx + std::cos(a) * u * 1.7f, footY + std::sin(a) * u * 0.32f});
        }
        r.fillConvex(sh.data(), int(sh.size()), Color(0, 0, 0, 50));
    }
    // pernas
    r.fillRoundRect(Rectf{cx - 1.05f * u, top + 6.2f * u, 0.95f * u, 3.6f * u}, 0.3f * u, L.pants);
    r.fillRoundRect(Rectf{cx + 0.1f * u, top + 6.2f * u, 0.95f * u, 3.6f * u}, 0.3f * u, L.pants);
    // sapatos
    r.fillRoundRect(Rectf{cx - 1.25f * u, top + 9.45f * u, 1.25f * u, 0.55f * u}, 0.25f * u, L.shoes);
    r.fillRoundRect(Rectf{cx + 0.05f * u, top + 9.45f * u, 1.25f * u, 0.55f * u}, 0.25f * u, L.shoes);
    // bracos
    r.fillRoundRect(Rectf{cx - 1.95f * u, top + 3.75f * u + sway, 0.7f * u, 3.0f * u}, 0.35f * u, L.shirt);
    const bool waving = wave > 0.01f;
    if (!waving) r.fillRoundRect(Rectf{cx + 1.25f * u, top + 3.75f * u + sway, 0.7f * u, 3.0f * u}, 0.35f * u, L.shirt);
    r.fillCircle(cx - 1.6f * u, top + 6.85f * u + sway, 0.38f * u, L.skin);
    if (!waving) r.fillCircle(cx + 1.6f * u, top + 6.85f * u + sway, 0.38f * u, L.skin);
    // tronco
    r.fillRoundRect(Rectf{cx - 1.35f * u, top + 3.6f * u + sway * 0.5f, 2.7f * u, 3.0f * u}, 0.6f * u, L.shirt);
    r.fillRect(Rectf{cx - 1.3f * u, top + 6.05f * u, 2.6f * u, 0.35f * u}, brighten(L.pants, -0.2f));
    if (waving) {
        // braco direito girando do ombro: para baixo (90 graus) -> para cima, oscilando
        float w = clampf(wave, 0, 1);
        float ang = lerpf(90.0f, -65.0f + 18.0f * std::sin(time * 11.0f), w) * kPi / 180.0f;
        float sx = cx + 1.55f * u, sy = top + 4.1f * u + sway;
        float hx = sx + std::cos(ang) * 2.9f * u, hy = sy + std::sin(ang) * 2.9f * u;
        r.fillCircle(sx, sy, 0.36f * u, L.shirt);
        r.drawLine(sx, sy, hx, hy, 0.7f * u, L.shirt);
        r.fillCircle(hx, hy, 0.42f * u, L.skin);
    }
    // estampa propria na camiseta (anel)
    r.strokeCircle(cx, top + 4.9f * u + sway * 0.5f, 0.42f * u, 0.12f * u, Color(255, 255, 255, 150));
    // pescoco e cabeca
    r.fillRect(Rectf{cx - 0.3f * u, top + 3.1f * u, 0.6f * u, 0.7f * u}, brighten(L.skin, -0.1f));
    float hy = top + 1.75f * u + sway * 0.3f;
    r.fillCircle(cx, hy, 1.55f * u, L.skin);
    // cabelo (calota + franja)
    {
        std::vector<Vec2> hair;
        for (int i = 0; i <= 14; ++i) {
            float a = kPi + kPi * i / 14.0f;
            hair.push_back(Vec2{cx + std::cos(a) * 1.68f * u, hy - 0.15f * u + std::sin(a) * 1.6f * u});
        }
        r.fillConvex(hair.data(), int(hair.size()), L.hair);
        Vec2 fringe[4] = {{cx - 1.6f * u, hy - 0.2f * u}, {cx + 1.0f * u, hy - 0.95f * u},
                          {cx + 1.62f * u, hy - 0.3f * u}, {cx - 1.4f * u, hy + 0.15f * u}};
        r.fillConvex(fringe, 4, L.hair);
    }
    // rosto
    r.fillCircle(cx - 0.55f * u, hy + 0.25f * u, 0.17f * u, Color(40, 30, 30));
    r.fillCircle(cx + 0.55f * u, hy + 0.25f * u, 0.17f * u, Color(40, 30, 30));
    strokeArc(r, cx, hy + 0.55f * u, 0.5f * u, 30, 150, 0.12f * u, Color(120, 50, 50));
}

void drawAbstractArt(IRenderer& r, const Rectf& rc, int seed, Color c1, Color c2) {
    uint32_t s = uint32_t(seed) * 2654435761u + 12345u;
    auto rnd = [&s]() { s = s * 1664525u + 1013904223u; return float((s >> 8) & 0xFFFF) / 65535.0f; };
    r.pushClip(rc);
    r.fillGradientV(rc, c1, c2);
    for (int i = 0; i < 5; ++i) {
        float cx = rc.x + rnd() * rc.w, cy = rc.y + rnd() * rc.h;
        float rad = (0.15f + rnd() * 0.45f) * std::min(rc.w, rc.h);
        r.fillCircle(cx, cy, rad, Color(255, 255, 255, uint8_t(18 + rnd() * 40)));
    }
    for (int i = 0; i < 3; ++i) {
        float x0 = rc.x + rnd() * rc.w;
        float w = rc.w * (0.08f + rnd() * 0.2f);
        Vec2 q[4] = {{x0, rc.y}, {x0 + w, rc.y}, {x0 + w - rc.h * 0.6f, rc.bottom()}, {x0 - rc.h * 0.6f, rc.bottom()}};
        r.fillConvex(q, 4, Color(0, 0, 0, uint8_t(20 + rnd() * 30)));
    }
    // horizonte/forma principal
    float hy = rc.y + rc.h * (0.55f + rnd() * 0.2f);
    Vec2 hill[4] = {{rc.x, hy}, {rc.x + rc.w * 0.45f, hy - rc.h * 0.18f}, {rc.right(), hy + rc.h * 0.05f}, {rc.right(), rc.bottom()}};
    Vec2 hill2[3] = {{rc.x, hy}, {rc.right(), rc.bottom()}, {rc.x, rc.bottom()}};
    Color ground = lerpColor(c2, Color(0, 0, 0), 0.35f);
    r.fillConvex(hill, 4, ground);
    r.fillConvex(hill2, 3, ground);
    r.fillGradientV(Rectf{rc.x, rc.bottom() - rc.h * 0.35f, rc.w, rc.h * 0.35f}, Color(0, 0, 0, 0), Color(0, 0, 0, 110));
    r.popClip();
}

void drawLoadingScreen(IRenderer& r, const DashboardInfo& info, float time) {
    const Rectf full{0, 0, float(kLogicalW), float(kLogicalH)};
    r.fillGradientV(full, Color::hex(0x0A0D11), brighten(info.accent, -0.82f));
    r.fillRadial(640, 360, 520, info.accent.withAlpha(50), info.accent.withAlpha(0), Rectf{120, 0, 1040, 720});
    drawSpinner(r, 640, 300, 34, time, brighten(info.accent, 0.3f));
    TextStyle st;
    st.size = 22;
    st.align = Align::Center;
    st.color = Color(190, 198, 208);
    st.tracking = 3;
    r.drawText("CARREGANDO", 640, 372, st);
    st.size = 44;
    st.tracking = 0;
    st.weight = FontWeight::Bold;
    st.color = Color(255, 255, 255);
    r.drawText(info.name, 640, 404, st);
    st.size = 18;
    st.weight = FontWeight::Regular;
    st.color = Color(150, 158, 170);
    std::string sub = std::string("Inspirado na versão ") + info.version + " (" + info.year + ")";
    r.drawText(sub.c_str(), 640, 464, st);
}

}  // namespace nx
