// Neutro X - fonte vetorial propria. Ver font.h.
#include "graphics/font.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>

namespace nx {

uint32_t utf8Next(const char*& p) {
    const unsigned char* s = reinterpret_cast<const unsigned char*>(p);
    if (!s || !*s) return 0;
    uint32_t c = s[0];
    int extra = 0;
    if (c < 0x80) { p += 1; return c; }
    if ((c & 0xE0) == 0xC0) { c &= 0x1F; extra = 1; }
    else if ((c & 0xF0) == 0xE0) { c &= 0x0F; extra = 2; }
    else if ((c & 0xF8) == 0xF0) { c &= 0x07; extra = 3; }
    else { p += 1; return 0xFFFD; }
    for (int i = 1; i <= extra; ++i) {
        if ((s[i] & 0xC0) != 0x80) { p += i; return 0xFFFD; }
        c = (c << 6) | (s[i] & 0x3F);
    }
    p += extra + 1;
    return c;
}

namespace {

struct PathDef {
    uint32_t cp;
    float width;
    const char* path;
};

// Grade: topo das maiusculas y=2, base y=12, altura-x y=5, descendente 15.5.
// Comandos: M x y (novo traco), L x y (linha), A cx cy rx ry a0 a1 (arco
// eliptico em graus; 0 = direita, 90 = baixo; continua o traco atual),
// Z (fecha o traco).
const PathDef kGlyphs[] = {
    // Maiusculas
    {'A', 8, "M0 12 L4 2 L8 12 M1.4 8.6 L6.6 8.6"},
    {'B', 6.8f, "M0 2 L3.8 2 A3.8 4.5 2.5 2.5 -90 90 L0 7 M0 7 L4.2 7 A4.2 9.5 2.6 2.5 -90 90 L0 12 L0 2"},
    {'C', 9, "A5 7 5 5 -40 -320"},
    {'D', 7.5f, "M0 2 L2.5 2 A2.5 7 5 5 -90 90 L0 12 Z"},
    {'E', 6, "M6 2 L0 2 L0 12 L6 12 M0 7 L5 7"},
    {'F', 6, "M6 2 L0 2 L0 12 M0 7 L5 7"},
    {'G', 10, "A5 7 5 5 -45 -360 L5.5 7"},
    {'H', 7, "M0 2 L0 12 M7 2 L7 12 M0 7 L7 7"},
    {'I', 0, "M0 2 L0 12"},
    {'J', 5, "M5 2 L5 9 A2.5 9 2.5 3 0 180"},
    {'K', 7, "M0 2 L0 12 M6.5 2 L0 8 M2.2 6 L7 12"},
    {'L', 5.5f, "M0 2 L0 12 L5.5 12"},
    {'M', 9, "M0 12 L0 2 L4.5 9 L9 2 L9 12"},
    {'N', 7.5f, "M0 12 L0 2 L7.5 12 L7.5 2"},
    {'O', 10, "A5 7 5 5 0 360"},
    {'P', 6.3f, "M0 12 L0 2 L3.5 2 A3.5 4.75 2.75 2.75 -90 90 L0 7.5"},
    {'Q', 10, "A5 7 5 5 0 360 M6 9.5 L10 13"},
    {'R', 6.5f, "M0 12 L0 2 L3.5 2 A3.5 4.75 2.75 2.75 -90 90 L0 7.5 M3 7.5 L6.5 12"},
    {'S', 7, "A3.5 4.6 3.3 2.6 -30 -270 A3.5 9.6 3.5 2.4 -90 150"},
    {'T', 8, "M0 2 L8 2 M4 2 L4 12"},
    {'U', 7, "M0 2 L0 8.5 A3.5 8.5 3.5 3.5 180 0 L7 2"},
    {'V', 8, "M0 2 L4 12 L8 2"},
    {'W', 11, "M0 2 L2.75 12 L5.5 4 L8.25 12 L11 2"},
    {'X', 7, "M0 2 L7 12 M7 2 L0 12"},
    {'Y', 7.5f, "M0 2 L3.75 7 L7.5 2 M3.75 7 L3.75 12"},
    {'Z', 7, "M0 2 L7 2 L0 12 L7 12"},
    // Minusculas
    {'a', 7, "A3.5 8.5 3.5 3.5 0 360 M7 5 L7 12"},
    {'b', 7, "M0 1.5 L0 12 M7 8.5 A3.5 8.5 3.5 3.5 0 360"},
    {'c', 6.5f, "A3.5 8.5 3.5 3.5 -40 -320"},
    {'d', 7, "A3.5 8.5 3.5 3.5 0 360 M7 1.5 L7 12"},
    {'e', 7, "M0 8.5 L7 8.5 A3.5 8.5 3.5 3.5 0 -315"},
    {'f', 4.5f, "M4.5 2.1 A3.3 3.3 1.5 1.5 -60 -180 L1.8 12 M0 5.2 L4 5.2"},
    {'g', 7, "A3.5 8.5 3.5 3.5 0 360 M7 5 L7 12.5 A3.5 12.5 3.5 2.6 0 160"},
    {'h', 6.5f, "M0 1.5 L0 12 M0 8 A3.25 8 3.25 3 -180 0 L6.5 12"},
    {'i', 0, "M0 5 L0 12 M0 2.7 L0 2.72"},
    {'j', 2, "M2 5 L2 13 A0.5 13 1.5 2.3 0 120 M2 2.7 L2 2.72"},
    {'k', 6, "M0 1.5 L0 12 M5.5 5 L0 9.5 M2 8 L6 12"},
    {'l', 0, "M0 1.5 L0 12"},
    {'m', 10.4f, "M0 5 L0 12 M0 8 A2.6 8 2.6 3 -180 0 L5.2 12 M5.2 8 A7.8 8 2.6 3 -180 0 L10.4 12"},
    {'n', 6.5f, "M0 5 L0 12 M0 8 A3.25 8 3.25 3 -180 0 L6.5 12"},
    {'o', 7, "A3.5 8.5 3.5 3.5 0 360"},
    {'p', 7, "M0 5 L0 15.5 M7 8.5 A3.5 8.5 3.5 3.5 0 360"},
    {'q', 7, "A3.5 8.5 3.5 3.5 0 360 M7 5 L7 15.5"},
    {'r', 4.5f, "M0 5 L0 12 M0 8.5 A3.5 8.5 3.5 3.3 -180 -60"},
    {'s', 5.8f, "A2.9 6.75 2.7 1.8 -20 -270 A2.9 10.3 2.9 1.8 -90 160"},
    {'t', 4.2f, "M1.8 2.5 L1.8 10.5 A3.3 10.5 1.5 1.5 180 80 M0 5.2 L4 5.2"},
    {'u', 6.5f, "M0 5 L0 9 A3.25 9 3.25 3 180 0 M6.5 5 L6.5 12"},
    {'v', 6.5f, "M0 5 L3.25 12 L6.5 5"},
    {'w', 9.6f, "M0 5 L2.4 12 L4.8 6.2 L7.2 12 L9.6 5"},
    {'x', 6, "M0 5 L6 12 M6 5 L0 12"},
    {'y', 6.5f, "M0 5 L3.25 12 M6.5 5 L2.2 15.5"},
    {'z', 6, "M0 5 L6 5 L0 12 L6 12"},
    // Algarismos (largura fixa = 7, para relogios e contadores)
    {'0', 7, "A3.5 7 3.5 5 0 360"},
    {'1', 7, "M1.8 3.8 L4 2 L4 12"},
    {'2', 7, "A3.4 4.9 3.1 2.9 -165 25 L0 12 L7 12"},
    {'3', 7, "A3.4 4.6 2.9 2.6 -160 90 L2.2 7.2 M3.4 7.2 A3.4 9.6 3.5 2.4 -90 165"},
    {'4', 7, "M5 12 L5 2 L0 9 L7 9"},
    {'5', 7, "M6.3 2 L1 2 L0.6 6.6 A3.4 8.9 3.3 3.1 -135 160"},
    {'6', 7, "A3.5 8.7 3.4 3.3 0 360 M5.2 2 L0.6 7.2"},
    {'7', 7, "M0 2 L7 2 L2.5 12"},
    {'8', 7, "A3.5 4.6 2.8 2.6 0 360 M6.9 9.5 A3.5 9.5 3.4 2.5 0 360"},
    {'9', 7, "A3.5 5.3 3.4 3.3 0 360 M6.4 6.8 L1.8 12"},
    // Pontuacao
    {' ', 2.2f, ""},
    {'.', 0, "M0 11.9 L0 12"},
    {',', 0.5f, "M0.5 11.5 L-0.3 13.5"},
    {':', 0, "M0 5.5 L0 5.6 M0 11.9 L0 12"},
    {';', 0.5f, "M0.5 5.5 L0.5 5.6 M0.5 11.5 L-0.3 13.5"},
    {'!', 0, "M0 2 L0 9 M0 11.9 L0 12"},
    {'?', 6, "A3 4.6 2.9 2.6 -160 80 L3 8.5 M3 11.9 L3 12"},
    {'-', 4, "M0 8 L4 8"},
    {'+', 6, "M0 7.5 L6 7.5 M3 4.5 L3 10.5"},
    {'=', 6, "M0 6 L6 6 M0 9.5 L6 9.5"},
    {'/', 5, "M0 13 L5 1"},
    {'\\', 5, "M0 1 L5 13"},
    {'(', 3, "A4 7 4 6.5 -120 -240"},
    {')', 3, "A-1 7 4 6.5 -60 60"},
    {'[', 3, "M3 1 L0 1 L0 13 L3 13"},
    {']', 3, "M0 1 L3 1 L3 13 L0 13"},
    {'\'', 0, "M0 2 L0 4.5"},
    {'"', 2, "M0 2 L0 4.5 M2 2 L2 4.5"},
    {'%', 8, "A1.8 4 1.6 2 0 360 M7.6 10 A6 10 1.6 2 0 360 M7 2 L1 12"},
    {'#', 7, "M2.2 2 L1.2 12 M5.7 2 L4.7 12 M0 5 L7 5 M0 9 L6.5 9"},
    {'<', 6, "M6 4 L0 7.5 L6 11"},
    {'>', 6, "M0 4 L6 7.5 L0 11"},
    {'_', 7, "M0 13.5 L7 13.5"},
    {'*', 5, "M2.5 2 L2.5 7 M0.3 3.3 L4.7 5.7 M4.7 3.3 L0.3 5.7"},
    {'|', 0, "M0 1 L0 15"},
    {'$', 7, "A3.5 4.6 3.3 2.6 -30 -270 A3.5 9.6 3.5 2.4 -90 150 M3.5 0.6 L3.5 13.4"},
    {'&', 8, "M8 12 L1.6 5.4 A3.2 4 1.8 2 -180 30 L0.8 9 A3.3 9.6 2.6 2.4 180 30 L7.5 7"},
    {'@', 11, "A5.5 7.5 5.5 5.5 20 340 M7.5 5 L7.5 9 A5.5 7.5 2 2.2 0 360"},
    {0x00B0, 3.2f, "A1.6 3.6 1.4 1.4 0 360"},                                // grau
    {0x00D7, 5.6f, "M0.3 5.4 L5.3 10.6 M5.3 5.4 L0.3 10.6"},                  // multiplicacao
    {0x2026, 7, "M0 11.9 L0 12 M3.5 11.9 L3.5 12 M7 11.9 L7 12"},            // reticencias
    {0x2022, 3, "A1.5 8 1.0 1.0 0 360 M1.5 8 L1.5 8.01"},                   // marcador
    {0x00B7, 0, "M0 8 L0 8.01"},                                              // ponto medio
    {0x2013, 7, "M0 8 L7 8"},                                                 // traco medio
    {0x2014, 11, "M0 8 L11 8"},                                               // travessao
    {0x00A9, 11, "A5.5 7 5.5 5.5 0 360 A5.5 7 2.6 2.6 -40 -320"},            // copyright
    {0x2190, 9, "M9 7 L0 7 M3.5 3.5 L0 7 L3.5 10.5"},                         // seta esquerda
    {0x2192, 9, "M0 7 L9 7 M5.5 3.5 L9 7 L5.5 10.5"},                         // seta direita
    {0x2191, 7, "M3.5 12 L3.5 2 M0 5.5 L3.5 2 L7 5.5"},                       // seta cima
    {0x2193, 7, "M3.5 2 L3.5 12 M0 8.5 L3.5 12 L7 8.5"},                      // seta baixo
    {0x0131, 0, "M0 5 L0 12"},                                                 // i sem ponto
};

enum Accent { kAcute = 1, kGrave, kCircumflex, kTilde, kDiaeresis, kCedilla };

struct Composed {
    uint32_t cp, base;
    int accent;
};

const Composed kComposed[] = {
    {0xE1, 'a', kAcute}, {0xE0, 'a', kGrave}, {0xE2, 'a', kCircumflex}, {0xE3, 'a', kTilde}, {0xE4, 'a', kDiaeresis},
    {0xE9, 'e', kAcute}, {0xE8, 'e', kGrave}, {0xEA, 'e', kCircumflex}, {0xEB, 'e', kDiaeresis},
    {0xED, 0x131, kAcute}, {0xEC, 0x131, kGrave}, {0xEE, 0x131, kCircumflex}, {0xEF, 0x131, kDiaeresis},
    {0xF3, 'o', kAcute}, {0xF2, 'o', kGrave}, {0xF4, 'o', kCircumflex}, {0xF5, 'o', kTilde}, {0xF6, 'o', kDiaeresis},
    {0xFA, 'u', kAcute}, {0xF9, 'u', kGrave}, {0xFB, 'u', kCircumflex}, {0xFC, 'u', kDiaeresis},
    {0xE7, 'c', kCedilla}, {0xF1, 'n', kTilde},
    {0xC1, 'A', kAcute}, {0xC0, 'A', kGrave}, {0xC2, 'A', kCircumflex}, {0xC3, 'A', kTilde}, {0xC4, 'A', kDiaeresis},
    {0xC9, 'E', kAcute}, {0xC8, 'E', kGrave}, {0xCA, 'E', kCircumflex},
    {0xCD, 'I', kAcute}, {0xCC, 'I', kGrave}, {0xCE, 'I', kCircumflex},
    {0xD3, 'O', kAcute}, {0xD2, 'O', kGrave}, {0xD4, 'O', kCircumflex}, {0xD5, 'O', kTilde}, {0xD6, 'O', kDiaeresis},
    {0xDA, 'U', kAcute}, {0xD9, 'U', kGrave}, {0xDB, 'U', kCircumflex}, {0xDC, 'U', kDiaeresis},
    {0xC7, 'C', kCedilla}, {0xD1, 'N', kTilde},
};

float strokeUnits(FontWeight w) {
    switch (w) {
        case FontWeight::Light: return 0.95f;
        case FontWeight::Bold: return 1.9f;
        default: return 1.35f;
    }
}

constexpr float kGap = 1.25f;  // espaco entre glifos (unidades)

float segDist2(float px, float py, const Vec2& a, const Vec2& b) {
    float dx = b.x - a.x, dy = b.y - a.y;
    float len2 = dx * dx + dy * dy;
    float t = len2 > 1e-9f ? ((px - a.x) * dx + (py - a.y) * dy) / len2 : 0.0f;
    t = clampf(t, 0, 1);
    float qx = a.x + dx * t - px, qy = a.y + dy * t - py;
    return qx * qx + qy * qy;
}

}  // namespace

StrokeFont& StrokeFont::instance() {
    static StrokeFont font;
    return font;
}

void StrokeFont::addPath(uint32_t cp, float width, const char* path) {
    Def d;
    d.width = width;
    std::vector<Vec2>* cur = nullptr;
    const char* p = path;
    auto num = [&p]() {
        while (*p == ' ' || *p == ',') ++p;
        char* end = nullptr;
        float v = std::strtof(p, &end);
        p = end;
        return v;
    };
    while (*p) {
        while (*p == ' ') ++p;
        char cmd = *p;
        if (!cmd) break;
        ++p;
        if (cmd == 'M') {
            float x = num(), y = num();
            d.lines.emplace_back();
            cur = &d.lines.back();
            cur->push_back({x, y});
        } else if (cmd == 'L') {
            float x = num(), y = num();
            if (!cur) { d.lines.emplace_back(); cur = &d.lines.back(); }
            cur->push_back({x, y});
        } else if (cmd == 'A') {
            float cx = num(), cy = num(), rx = num(), ry = num(), a0 = num(), a1 = num();
            if (!cur) { d.lines.emplace_back(); cur = &d.lines.back(); }
            int steps = std::max(8, int(std::fabs(a1 - a0) / 9.0f));
            for (int i = 0; i <= steps; ++i) {
                float a = (a0 + (a1 - a0) * i / steps) * kPi / 180.0f;
                cur->push_back({cx + rx * std::cos(a), cy + ry * std::sin(a)});
            }
        } else if (cmd == 'Z') {
            if (cur && !cur->empty()) cur->push_back(cur->front());
        }
    }
    defs_[cp] = std::move(d);
}

void StrokeFont::addComposed(uint32_t cp, uint32_t base, int accent) {
    auto it = defs_.find(base);
    if (it == defs_.end()) return;
    Def d = it->second;
    bool upper = (base >= 'A' && base <= 'Z');
    float cx = d.width * 0.5f;
    float ty = upper ? 0.0f : 3.2f;  // centro vertical do acento
    auto line = [&d](std::initializer_list<Vec2> pts) { d.lines.emplace_back(pts); };
    switch (accent) {
        case kAcute: line({{cx - 0.7f, ty + 0.8f}, {cx + 1.0f, ty - 1.0f}}); break;
        case kGrave: line({{cx - 1.0f, ty - 1.0f}, {cx + 0.7f, ty + 0.8f}}); break;
        case kCircumflex: line({{cx - 1.7f, ty + 0.8f}, {cx, ty - 0.9f}, {cx + 1.7f, ty + 0.8f}}); break;
        case kTilde: line({{cx - 2.0f, ty + 0.5f}, {cx - 0.8f, ty - 0.6f}, {cx + 0.6f, ty + 0.4f}, {cx + 2.0f, ty - 0.7f}}); break;
        case kDiaeresis:
            line({{cx - 1.4f, ty}, {cx - 1.4f, ty + 0.02f}});
            line({{cx + 1.4f, ty}, {cx + 1.4f, ty + 0.02f}});
            break;
        case kCedilla: {
            float bx = upper ? 5.0f : 3.5f;
            line({{bx, 12.0f}, {bx + 0.9f, 13.4f}, {bx - 0.9f, 14.9f}});
            break;
        }
    }
    defs_[cp] = std::move(d);
}

StrokeFont::StrokeFont() {
    for (const PathDef& g : kGlyphs) addPath(g.cp, g.width, g.path);
    for (const Composed& c : kComposed) addComposed(c.cp, c.base, c.accent);
    // Glifo substituto: retangulo.
    fallback_.width = 5;
    fallback_.lines.push_back({{0, 2}, {5, 2}, {5, 12}, {0, 12}, {0, 2}});
}

const StrokeFont::Def& StrokeFont::def(uint32_t cp) const {
    auto it = defs_.find(cp);
    if (it != defs_.end()) return it->second;
    if (cp == 0xA0) return defs_.at(' ');
    return fallback_;
}

bool StrokeFont::hasGlyph(uint32_t cp) const { return defs_.count(cp) != 0; }

float StrokeFont::advance(uint32_t cp, float pixelSize, FontWeight weight) {
    const Def& d = def(cp);
    float s = pixelSize / 16.0f;
    return (d.width + strokeUnits(weight) + kGap) * s;
}

void StrokeFont::clearCache() {
    cache_.clear();
    cacheBytes_ = 0;
    clearPending_ = false;
}

uint64_t StrokeFont::key(uint32_t cp, float pixelSize, FontWeight weight, int* sizeKeyOut) {
    int sizeKey = int(pixelSize * 2.0f + 0.5f);  // quantiza em meio pixel
    if (sizeKey < 2) sizeKey = 2;
    if (sizeKeyOut) *sizeKeyOut = sizeKey;
    return (uint64_t(cp) << 24) | (uint64_t(sizeKey & 0xFFFFF) << 2) | uint64_t(int(weight) & 3);
}

const GlyphBitmap* StrokeFont::findGlyph(uint32_t cp, float pixelSize, FontWeight weight) const {
    auto it = cache_.find(key(cp, pixelSize, weight, nullptr));
    return it == cache_.end() ? nullptr : &it->second;
}

const GlyphBitmap& StrokeFont::glyph(uint32_t cp, float pixelSize, FontWeight weight) {
    int sizeKey = 2;
    const uint64_t k = key(cp, pixelSize, weight, &sizeKey);
    auto it = cache_.find(k);
    if (it != cache_.end()) return it->second;

    // Limite simples de memoria do cache de glifos (~8 MB). Esvaziar no meio de um
    // quadro invalidaria glifos ja usados: so marca, e collect() limpa no proximo quadro.
    if (cacheBytes_ > 8u * 1024u * 1024u) {
        if (pins_ == 0) clearCache();
        else clearPending_ = true;
    }

    const float px = sizeKey * 0.5f;
    const float s = px / 16.0f;
    const Def& d = def(cp);
    const float sw = strokeUnits(weight);
    const float r = sw * 0.5f * s;  // raio do traco em pixels
    const float lsb = (kGap * 0.5f + sw * 0.5f) * s;

    GlyphBitmap g;
    g.advance = (d.width + sw + kGap) * s;

    if (!d.lines.empty()) {
        float minx = 1e9f, miny = 1e9f, maxx = -1e9f, maxy = -1e9f;
        for (const auto& l : d.lines)
            for (const Vec2& v : l) {
                minx = std::min(minx, v.x); maxx = std::max(maxx, v.x);
                miny = std::min(miny, v.y); maxy = std::max(maxy, v.y);
            }
        int x0 = int(std::floor(lsb + minx * s - r - 1));
        int x1 = int(std::ceil(lsb + maxx * s + r + 1));
        int y0 = int(std::floor(miny * s - r - 1));
        int y1 = int(std::ceil(maxy * s + r + 1));
        g.w = x1 - x0;
        g.h = y1 - y0;
        g.offsetX = x0;
        g.offsetY = y0;
        g.alpha.assign(size_t(g.w) * g.h, 0);

        // Segmentos em espaco de pixels relativos ao bitmap.
        std::vector<std::pair<Vec2, Vec2>> segs;
        for (const auto& l : d.lines) {
            for (size_t i = 0; i < l.size(); ++i) {
                Vec2 a{lsb + l[i].x * s - x0, l[i].y * s - y0};
                Vec2 b = a;
                if (i + 1 < l.size()) b = Vec2{lsb + l[i + 1].x * s - x0, l[i + 1].y * s - y0};
                else if (l.size() > 1) continue;  // ultimo ponto ja coberto pelo segmento anterior
                segs.push_back({a, b});
            }
        }
        const float reach = r + 1.0f;
        for (const auto& sg : segs) {
            // So percorre o retangulo envolvente do segmento.
            int bx0 = std::max(0, int(std::floor(std::min(sg.first.x, sg.second.x) - reach)));
            int bx1 = std::min(g.w - 1, int(std::ceil(std::max(sg.first.x, sg.second.x) + reach)));
            int by0 = std::max(0, int(std::floor(std::min(sg.first.y, sg.second.y) - reach)));
            int by1 = std::min(g.h - 1, int(std::ceil(std::max(sg.first.y, sg.second.y) + reach)));
            for (int y = by0; y <= by1; ++y) {
                for (int x = bx0; x <= bx1; ++x) {
                    float dist = std::sqrt(segDist2(x + 0.5f, y + 0.5f, sg.first, sg.second));
                    float cov = clampf(r - dist + 0.5f, 0, 1);
                    uint8_t a = uint8_t(cov * 255.0f + 0.5f);
                    uint8_t& dst = g.alpha[size_t(y) * g.w + x];
                    if (a > dst) dst = a;
                }
            }
        }
    }
    cacheBytes_ += g.alpha.size() + sizeof(GlyphBitmap);
    return cache_.emplace(k, std::move(g)).first->second;
}

}  // namespace nx
