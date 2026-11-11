// Neutro X - tipos basicos portaveis (sem dependencias de plataforma).
#pragma once
#include <cstdint>
#include <cmath>

namespace nx {

// Resolucao logica fixa da interface. Todo o layout e escrito nessas
// coordenadas; o renderizador converte para a resolucao real.
constexpr int kLogicalW = 1280;
constexpr int kLogicalH = 720;

struct Color {
    uint8_t r = 0, g = 0, b = 0, a = 255;
    constexpr Color() = default;
    constexpr Color(uint8_t r_, uint8_t g_, uint8_t b_, uint8_t a_ = 255) : r(r_), g(g_), b(b_), a(a_) {}
    // 0xRRGGBB
    static constexpr Color hex(uint32_t rgb, uint8_t a = 255) {
        return Color(uint8_t((rgb >> 16) & 0xFF), uint8_t((rgb >> 8) & 0xFF), uint8_t(rgb & 0xFF), a);
    }
    constexpr Color withAlpha(uint8_t na) const { return Color(r, g, b, na); }
    Color fade(float f) const {
        float v = a * (f < 0 ? 0 : (f > 1 ? 1 : f));
        return Color(r, g, b, uint8_t(v + 0.5f));
    }
    // 0xAARRGGBB empacotado (formato do framebuffer)
    constexpr uint32_t argb() const { return (uint32_t(a) << 24) | (uint32_t(r) << 16) | (uint32_t(g) << 8) | b; }
};

inline Color lerpColor(Color a, Color b, float t) {
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    auto l = [t](uint8_t x, uint8_t y) { return uint8_t(x + (y - x) * t + 0.5f); };
    return Color(l(a.r, b.r), l(a.g, b.g), l(a.b, b.b), l(a.a, b.a));
}

inline Color brighten(Color c, float amount) {  // amount -1..1
    if (amount >= 0) return lerpColor(c, Color(255, 255, 255, c.a), amount);
    return lerpColor(c, Color(0, 0, 0, c.a), -amount);
}

struct Vec2 {
    float x = 0, y = 0;
};

struct Rectf {
    float x = 0, y = 0, w = 0, h = 0;
    float right() const { return x + w; }
    float bottom() const { return y + h; }
    float cx() const { return x + w * 0.5f; }
    float cy() const { return y + h * 0.5f; }
    Rectf inset(float d) const { return Rectf{x + d, y + d, w - 2 * d, h - 2 * d}; }
    Rectf offset(float dx, float dy) const { return Rectf{x + dx, y + dy, w, h}; }
    Rectf scaledAboutCenter(float s) const {
        float nw = w * s, nh = h * s;
        return Rectf{cx() - nw * 0.5f, cy() - nh * 0.5f, nw, nh};
    }
};

inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }

constexpr float kPi = 3.14159265358979f;

}  // namespace nx
