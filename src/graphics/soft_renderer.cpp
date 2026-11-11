// Neutro X - renderizador em software. Ver soft_renderer.h.
#include "graphics/soft_renderer.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include "graphics/font.h"

namespace nx {

namespace {

inline uint32_t div255(uint32_t x) { return (x + 1 + (x >> 8)) >> 8; }

inline void blendRaw(uint32_t& d, uint32_t r, uint32_t g, uint32_t b, uint32_t a) {
    if (a >= 255) { d = 0xFF000000u | (r << 16) | (g << 8) | b; return; }
    if (a == 0) return;
    uint32_t ia = 255 - a;
    uint32_t dr = (d >> 16) & 0xFF, dg = (d >> 8) & 0xFF, db = d & 0xFF;
    dr = div255(r * a + dr * ia);
    dg = div255(g * a + dg * ia);
    db = div255(b * a + db * ia);
    d = 0xFF000000u | (dr << 16) | (dg << 8) | db;
}

// Distancia assinada a um retangulo arredondado (negativa dentro).
inline float roundRectSdf(float px, float py, float x0, float y0, float x1, float y1, float r) {
    float cx = clampf(px, x0 + r, x1 - r);
    float cy = clampf(py, y0 + r, y1 - r);
    float dx = px - cx, dy = py - cy;
    float outside = std::sqrt(dx * dx + dy * dy) - r;
    // Dentro da regiao reta: distancia negativa ate a borda mais proxima.
    if (px >= x0 + r && px <= x1 - r && py >= y0 && py <= y1) {
        return -std::min(py - y0, y1 - py);
    }
    if (py >= y0 + r && py <= y1 - r && px >= x0 && px <= x1) {
        return -std::min(px - x0, x1 - px);
    }
    return outside;
}

}  // namespace

SoftRenderer::SoftRenderer(int width, int height) : w_(0), h_(0) { resize(width, height); }

void SoftRenderer::resize(int width, int height) {
    w_ = std::max(1, width);
    h_ = std::max(1, height);
    fb_.assign(size_t(w_) * h_, 0xFF000000u);
    scale_ = std::min(float(w_) / kLogicalW, float(h_) / kLogicalH);
    ox_ = std::floor((w_ - kLogicalW * scale_) * 0.5f);
    oy_ = std::floor((h_ - kLogicalH * scale_) * 0.5f);
    beginFrame();
}

void SoftRenderer::beginFrame() {
    clipStack_.clear();
    alphaStack_.clear();
    offsetStack_.clear();
    offX_ = offY_ = 0;
    alpha_ = 1.0f;
    clip_ = ClipI{int(ox_), int(oy_), int(std::ceil(ox_ + kLogicalW * scale_)), int(std::ceil(oy_ + kLogicalH * scale_))};
    clip_.x1 = std::min(clip_.x1, w_);
    clip_.y1 = std::min(clip_.y1, h_);
}

void SoftRenderer::endFrame() {}

uint32_t SoftRenderer::effAlpha(uint8_t a) const { return uint32_t(a * alpha_ + 0.5f); }

void SoftRenderer::blendSpan(int y, int x0, int x1, Color c, uint32_t alpha) {
    if (y < clip_.y0 || y >= clip_.y1) return;
    x0 = std::max(x0, clip_.x0);
    x1 = std::min(x1, clip_.x1);
    if (x0 >= x1 || alpha == 0) return;
    uint32_t* row = fb_.data() + size_t(y) * w_;
    if (alpha >= 255) {
        std::fill(row + x0, row + x1, 0xFF000000u | (uint32_t(c.r) << 16) | (uint32_t(c.g) << 8) | c.b);
        return;
    }
    for (int x = x0; x < x1; ++x) blendRaw(row[x], c.r, c.g, c.b, alpha);
}

void SoftRenderer::blendPixel(int x, int y, Color c, uint32_t alpha) {
    if (x < clip_.x0 || x >= clip_.x1 || y < clip_.y0 || y >= clip_.y1 || alpha == 0) return;
    blendRaw(fb_[size_t(y) * w_ + x], c.r, c.g, c.b, alpha);
}

void SoftRenderer::clear(Color c) {
    // Limpa a tela inteira (inclusive letterbox) com preto e a area util com a cor.
    std::fill(fb_.begin(), fb_.end(), 0xFF000000u);
    for (int y = clip_.y0; y < clip_.y1; ++y) blendSpan(y, clip_.x0, clip_.x1, c, 255);
}

void SoftRenderer::fillRect(const Rectf& r, Color c) {
    uint32_t a = effAlpha(c.a);
    if (!a || r.w <= 0 || r.h <= 0) return;
    int x0 = int(std::lround(tx(r.x))), x1 = int(std::lround(tx(r.right())));
    int y0 = int(std::lround(ty(r.y))), y1 = int(std::lround(ty(r.bottom())));
    y0 = std::max(y0, clip_.y0);
    y1 = std::min(y1, clip_.y1);
    for (int y = y0; y < y1; ++y) blendSpan(y, x0, x1, c, a);
}

void SoftRenderer::fillRoundRect(const Rectf& r, float radius, Color c) {
    uint32_t a = effAlpha(c.a);
    if (!a || r.w <= 0 || r.h <= 0) return;
    float fx0 = tx(r.x), fy0 = ty(r.y), fx1 = tx(r.right()), fy1 = ty(r.bottom());
    float rr = std::min(radius * scale_, std::min(fx1 - fx0, fy1 - fy0) * 0.5f);
    if (rr < 0) rr = 0;
    int iy0 = std::max(int(std::floor(fy0)), clip_.y0), iy1 = std::min(int(std::ceil(fy1)), clip_.y1);
    int ix0 = std::max(int(std::floor(fx0)), clip_.x0), ix1 = std::min(int(std::ceil(fx1)), clip_.x1);
    for (int y = iy0; y < iy1; ++y) {
        float yc = y + 0.5f;
        float dy = std::max(0.0f, std::max((fy0 + rr) - yc, yc - (fy1 - rr)));
        // Faixa central (fora das colunas dos cantos): cobertura so depende de y.
        int mx0 = int(std::ceil(fx0 + rr)), mx1 = int(std::floor(fx1 - rr));
        float midCov = clampf(rr - dy + 0.5f, 0, 1);
        if (rr < 0.5f) midCov = clampf(std::min(yc - fy0, fy1 - yc) + 0.5f, 0, 1);
        if (mx1 > mx0) blendSpan(y, mx0, mx1, c, uint32_t(a * midCov + 0.5f));
        else { mx0 = ix1; mx1 = ix1; }
        auto edge = [&](int x) {
            float xc = x + 0.5f;
            float sd = roundRectSdf(xc, yc, fx0, fy0, fx1, fy1, rr);
            float cov = clampf(0.5f - sd, 0, 1);
            blendPixel(x, y, c, uint32_t(a * cov + 0.5f));
        };
        for (int x = ix0; x < std::min(mx0, ix1); ++x) edge(x);
        for (int x = std::max(mx1, ix0); x < ix1; ++x) if (x >= mx0) edge(x);
    }
}

void SoftRenderer::fillGradientV(const Rectf& r, Color top, Color bottom) {
    if (r.w <= 0 || r.h <= 0) return;
    int x0 = int(std::lround(tx(r.x))), x1 = int(std::lround(tx(r.right())));
    float fy0 = ty(r.y), fy1 = ty(r.bottom());
    int y0 = std::max(int(std::lround(fy0)), clip_.y0), y1 = std::min(int(std::lround(fy1)), clip_.y1);
    float span = std::max(1.0f, fy1 - fy0);
    for (int y = y0; y < y1; ++y) {
        Color c = lerpColor(top, bottom, (y + 0.5f - fy0) / span);
        blendSpan(y, x0, x1, c, effAlpha(c.a));
    }
}

void SoftRenderer::fillGradientH(const Rectf& r, Color left, Color right) {
    if (r.w <= 0 || r.h <= 0) return;
    float fx0 = tx(r.x), fx1 = tx(r.right());
    int x0 = std::max(int(std::lround(fx0)), clip_.x0), x1 = std::min(int(std::lround(fx1)), clip_.x1);
    int y0 = std::max(int(std::lround(ty(r.y))), clip_.y0), y1 = std::min(int(std::lround(ty(r.bottom()))), clip_.y1);
    if (x0 >= x1) return;
    float span = std::max(1.0f, fx1 - fx0);
    std::vector<Color> cols(size_t(x1 - x0));
    for (int x = x0; x < x1; ++x) cols[size_t(x - x0)] = lerpColor(left, right, (x + 0.5f - fx0) / span);
    for (int y = y0; y < y1; ++y) {
        uint32_t* row = fb_.data() + size_t(y) * w_;
        for (int x = x0; x < x1; ++x) {
            const Color& c = cols[size_t(x - x0)];
            blendRaw(row[x], c.r, c.g, c.b, effAlpha(c.a));
        }
    }
}

void SoftRenderer::fillRadial(float cx, float cy, float radius, Color inner, Color outer, const Rectf& area) {
    float pcx = tx(cx), pcy = ty(cy), pr = std::max(1.0f, radius * scale_);
    int x0 = std::max(int(std::floor(tx(area.x))), clip_.x0), x1 = std::min(int(std::ceil(tx(area.right()))), clip_.x1);
    int y0 = std::max(int(std::floor(ty(area.y))), clip_.y0), y1 = std::min(int(std::ceil(ty(area.bottom()))), clip_.y1);
    if (x0 >= x1 || y0 >= y1) return;
    // Tabela indexada pela distancia ao quadrado normalizada (evita sqrt e
    // interpolacao em ponto flutuante por pixel).
    constexpr int kLut = 1024;
    struct Entry { uint8_t r, g, b; uint32_t a; };
    Entry lut[kLut + 1];
    for (int i = 0; i <= kLut; ++i) {
        Color c = lerpColor(inner, outer, std::sqrt(float(i) / kLut));
        lut[i] = Entry{c.r, c.g, c.b, effAlpha(c.a)};
    }
    const Entry& out = lut[kLut];
    const float r2 = pr * pr, inv = kLut / r2;
    for (int y = y0; y < y1; ++y) {
        uint32_t* row = fb_.data() + size_t(y) * w_;
        float dy = (y + 0.5f - pcy);
        float dy2 = dy * dy;
        // Faixa da linha dentro do raio; fora dela a cor e constante (outer).
        int sx0 = x1, sx1 = x1;
        if (dy2 < r2) {
            float hw = std::sqrt(r2 - dy2);
            sx0 = std::max(x0, int(std::floor(pcx - hw)));
            sx1 = std::min(x1, int(std::ceil(pcx + hw)));
        }
        if (out.a) {
            for (int x = x0; x < std::min(sx0, x1); ++x) blendRaw(row[x], out.r, out.g, out.b, out.a);
            for (int x = std::max(sx1, x0); x < x1; ++x) blendRaw(row[x], out.r, out.g, out.b, out.a);
        }
        float dx = (sx0 + 0.5f - pcx);
        for (int x = sx0; x < sx1; ++x, dx += 1.0f) {
            int idx = int((dx * dx + dy2) * inv);
            const Entry& e = lut[idx < kLut ? idx : kLut];
            if (e.a) blendRaw(row[x], e.r, e.g, e.b, e.a);
        }
    }
}

void SoftRenderer::fillCircle(float cx, float cy, float radius, Color c) {
    uint32_t a = effAlpha(c.a);
    if (!a || radius <= 0) return;
    float pcx = tx(cx), pcy = ty(cy), pr = radius * scale_;
    int y0 = std::max(int(std::floor(pcy - pr - 1)), clip_.y0), y1 = std::min(int(std::ceil(pcy + pr + 1)), clip_.y1);
    for (int y = y0; y < y1; ++y) {
        float dy = y + 0.5f - pcy;
        float half = pr * pr - dy * dy;
        float inner = half > 0 ? std::sqrt(half) : 0;
        // Interior solido
        int sx0 = int(std::ceil(pcx - inner + 1.0f)), sx1 = int(std::floor(pcx + inner - 1.0f));
        if (sx1 > sx0 && std::fabs(dy) < pr - 1.0f) blendSpan(y, sx0, sx1, c, a);
        else { sx0 = sx1 = int(std::floor(pcx)); }
        int ex0 = int(std::floor(pcx - pr - 1)), ex1 = int(std::ceil(pcx + pr + 1));
        for (int x = ex0; x < ex1; ++x) {
            if (x >= sx0 && x < sx1) { x = sx1 - 1; continue; }
            float dx = x + 0.5f - pcx;
            float cov = clampf(pr - std::sqrt(dx * dx + dy * dy) + 0.5f, 0, 1);
            if (cov > 0) blendPixel(x, y, c, uint32_t(a * cov + 0.5f));
        }
    }
}

void SoftRenderer::strokeCircle(float cx, float cy, float radius, float thickness, Color c) {
    uint32_t a = effAlpha(c.a);
    if (!a) return;
    float pcx = tx(cx), pcy = ty(cy), pr = radius * scale_, ht = thickness * scale_ * 0.5f;
    float outer = pr + ht + 1;
    int y0 = std::max(int(std::floor(pcy - outer)), clip_.y0), y1 = std::min(int(std::ceil(pcy + outer)), clip_.y1);
    int x0 = std::max(int(std::floor(pcx - outer)), clip_.x0), x1 = std::min(int(std::ceil(pcx + outer)), clip_.x1);
    float innerSkip = std::max(0.0f, pr - ht - 1.5f);
    for (int y = y0; y < y1; ++y) {
        float dy = y + 0.5f - pcy;
        // Regiao interna totalmente fora do anel nesta linha: pula.
        int skip0 = x1, skip1 = x1;
        if (std::fabs(dy) < innerSkip) {
            float hw = std::sqrt(innerSkip * innerSkip - dy * dy);
            skip0 = int(std::ceil(pcx - hw));
            skip1 = int(std::floor(pcx + hw));
        }
        for (int x = x0; x < x1; ++x) {
            if (x >= skip0 && x < skip1) { x = skip1 - 1; continue; }
            float dx = x + 0.5f - pcx;
            float d = std::sqrt(dx * dx + dy * dy);
            float cov = clampf(ht - std::fabs(d - pr) + 0.5f, 0, 1);
            if (cov > 0) blendRaw(fb_[size_t(y) * w_ + x], c.r, c.g, c.b, uint32_t(a * cov + 0.5f));
        }
    }
}

void SoftRenderer::strokeRect(const Rectf& r, float t, Color c) {
    fillRect(Rectf{r.x, r.y, r.w, t}, c);
    fillRect(Rectf{r.x, r.bottom() - t, r.w, t}, c);
    fillRect(Rectf{r.x, r.y + t, t, r.h - 2 * t}, c);
    fillRect(Rectf{r.right() - t, r.y + t, t, r.h - 2 * t}, c);
}

void SoftRenderer::strokeRoundRect(const Rectf& r, float radius, float thickness, Color c) {
    uint32_t a = effAlpha(c.a);
    if (!a || r.w <= 0 || r.h <= 0) return;
    float fx0 = tx(r.x), fy0 = ty(r.y), fx1 = tx(r.right()), fy1 = ty(r.bottom());
    float rr = std::max(0.0f, std::min(radius * scale_, std::min(fx1 - fx0, fy1 - fy0) * 0.5f));
    float ht = thickness * scale_ * 0.5f;
    // A linha fica centrada ht para dentro da borda (traco interno).
    float ix0 = fx0 + ht, iy0 = fy0 + ht, ix1 = fx1 - ht, iy1 = fy1 - ht;
    float ir = std::max(0.0f, rr - ht);
    int y0 = std::max(int(std::floor(fy0)) - 1, clip_.y0), y1 = std::min(int(std::ceil(fy1)) + 1, clip_.y1);
    int x0 = std::max(int(std::floor(fx0)) - 1, clip_.x0), x1 = std::min(int(std::ceil(fx1)) + 1, clip_.x1);
    float band = ht + 1.5f;
    for (int y = y0; y < y1; ++y) {
        float yc = y + 0.5f;
        bool straight = yc > iy0 + ir + band && yc < iy1 - ir - band;
        for (int x = x0; x < x1; ++x) {
            float xc = x + 0.5f;
            if (straight && xc > ix0 + band && xc < ix1 - band) { x = std::max(x, int(ix1 - band) - 1); continue; }
            float sd = roundRectSdf(xc, yc, ix0, iy0, ix1, iy1, ir);
            float cov = clampf(ht - std::fabs(sd) + 0.5f, 0, 1);
            if (cov > 0) blendRaw(fb_[size_t(y) * w_ + x], c.r, c.g, c.b, uint32_t(a * cov + 0.5f));
        }
    }
}

void SoftRenderer::drawLine(float x0, float y0, float x1, float y1, float thickness, Color c) {
    float dx = x1 - x0, dy = y1 - y0;
    float len = std::sqrt(dx * dx + dy * dy);
    if (len < 1e-4f) { fillCircle(x0, y0, thickness * 0.5f, c); return; }
    float nx = -dy / len * thickness * 0.5f, ny = dx / len * thickness * 0.5f;
    Vec2 q[4] = {{x0 + nx, y0 + ny}, {x1 + nx, y1 + ny}, {x1 - nx, y1 - ny}, {x0 - nx, y0 - ny}};
    fillConvex(q, 4, c);
}

void SoftRenderer::fillConvex(const Vec2* pts, int count, Color c) {
    if (count < 3) return;
    std::vector<Vec2> dev(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i) dev[size_t(i)] = Vec2{tx(pts[i].x), ty(pts[i].y)};
    convexDevice(dev.data(), count, c);
}

void SoftRenderer::convexDevice(const Vec2* p, int n, Color c) {
    uint32_t a = effAlpha(c.a);
    if (!a) return;
    float area = 0;
    float miny = 1e9f, maxy = -1e9f, minx = 1e9f, maxx = -1e9f;
    for (int i = 0; i < n; ++i) {
        const Vec2& u = p[i];
        const Vec2& v = p[(i + 1) % n];
        area += u.x * v.y - v.x * u.y;
        miny = std::min(miny, u.y); maxy = std::max(maxy, u.y);
        minx = std::min(minx, u.x); maxx = std::max(maxx, u.x);
    }
    if (std::fabs(area) < 1e-3f) return;
    float sign = area > 0 ? 1.0f : -1.0f;
    // Normais das arestas (apontando para dentro) para distancia assinada.
    struct Edge { float nx, ny, d; };
    std::vector<Edge> edges(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) {
        const Vec2& u = p[i];
        const Vec2& v = p[(i + 1) % n];
        float ex = v.x - u.x, ey = v.y - u.y;
        float len = std::sqrt(ex * ex + ey * ey);
        if (len < 1e-6f) { edges[size_t(i)] = Edge{0, 0, 1e9f}; continue; }
        float nx = -ey / len * sign, ny = ex / len * sign;
        edges[size_t(i)] = Edge{nx, ny, -(nx * u.x + ny * u.y)};
    }
    auto sdf = [&](float x, float y) {
        float m = 1e9f;
        for (const Edge& e : edges) m = std::min(m, e.nx * x + e.ny * y + e.d);
        return m;  // positiva dentro
    };
    auto span = [&](float y, float& xl, float& xr) {
        xl = 1e9f; xr = -1e9f;
        for (int i = 0; i < n; ++i) {
            const Vec2& u = p[i];
            const Vec2& v = p[(i + 1) % n];
            if ((y < std::min(u.y, v.y)) || (y > std::max(u.y, v.y))) continue;
            float x;
            if (std::fabs(v.y - u.y) < 1e-6f) { xl = std::min(xl, std::min(u.x, v.x)); xr = std::max(xr, std::max(u.x, v.x)); continue; }
            x = u.x + (y - u.y) * (v.x - u.x) / (v.y - u.y);
            xl = std::min(xl, x); xr = std::max(xr, x);
        }
        return xr >= xl;
    };
    int y0 = std::max(int(std::floor(miny)) - 1, clip_.y0), y1 = std::min(int(std::ceil(maxy)) + 1, clip_.y1);
    int bx0 = std::max(int(std::floor(minx)) - 1, clip_.x0), bx1 = std::min(int(std::ceil(maxx)) + 1, clip_.x1);
    for (int y = y0; y < y1; ++y) {
        float ya = std::max(miny, float(y)), yb = std::min(maxy, float(y + 1));
        float l1, r1, l2, r2;
        bool ok1 = span(ya, l1, r1), ok2 = span(yb, l2, r2);
        if (!ok1 && !ok2) continue;
        if (!ok1) { l1 = l2; r1 = r2; }
        if (!ok2) { l2 = l1; r2 = r1; }
        float outerL = std::min(l1, l2) - 1, outerR = std::max(r1, r2) + 1;
        float innerL = std::max(l1, l2), innerR = std::min(r1, r2);
        bool fullRow = (float(y) >= miny) && (float(y + 1) <= maxy);
        int sx0 = int(std::ceil(innerL)), sx1 = int(std::floor(innerR));
        if (!fullRow || sx1 <= sx0) { sx0 = sx1 = int(outerR) + 2; }
        else blendSpan(y, sx0, sx1, c, a);
        int ex0 = std::max(int(std::floor(outerL)), bx0), ex1 = std::min(int(std::ceil(outerR)), bx1);
        for (int x = ex0; x < ex1; ++x) {
            if (x >= sx0 && x < sx1) { x = sx1 - 1; continue; }
            float cov = clampf(sdf(x + 0.5f, y + 0.5f) + 0.5f, 0, 1);
            if (cov > 0) blendPixel(x, y, c, uint32_t(a * cov + 0.5f));
        }
    }
}

float SoftRenderer::measureText(const char* utf8, const TextStyle& st) {
    if (!utf8) return 0;
    StrokeFont& f = StrokeFont::instance();
    float total = 0;
    int count = 0;
    const char* p = utf8;
    while (uint32_t cp = utf8Next(p)) {
        total += f.advance(cp, st.size, st.weight) + st.tracking * st.size / 16.0f;
        ++count;
    }
    return total;
}

void SoftRenderer::drawText(const char* utf8, float x, float y, const TextStyle& st) {
    if (!utf8 || !*utf8) return;
    uint32_t baseA = effAlpha(st.color.a);
    if (!baseA) return;
    StrokeFont& f = StrokeFont::instance();
    float width = measureText(utf8, st);
    if (st.align == Align::Center) (st.rotated90 ? y : x) -= width * 0.5f;
    else if (st.align == Align::Right) (st.rotated90 ? y : x) -= width;

    const float devSize = st.size * scale_;
    float penX = tx(x), penY = ty(y);
    const char* p = utf8;
    const Color c = st.color;
    while (uint32_t cp = utf8Next(p)) {
        const GlyphBitmap& g = f.glyph(cp, devSize, st.weight);
        int gx = int(std::lround(penX)), gy = int(std::lround(penY));
        if (!g.alpha.empty()) {
            if (!st.rotated90) {
                int bx = gx + g.offsetX, by = gy + g.offsetY;
                for (int j = 0; j < g.h; ++j) {
                    int yy = by + j;
                    if (yy < clip_.y0 || yy >= clip_.y1) continue;
                    uint32_t* row = fb_.data() + size_t(yy) * w_;
                    const uint8_t* src = g.alpha.data() + size_t(j) * g.w;
                    int i0 = std::max(0, clip_.x0 - bx), i1 = std::min(g.w, clip_.x1 - bx);
                    for (int i = i0; i < i1; ++i) {
                        if (!src[i]) continue;
                        blendRaw(row[bx + i], c.r, c.g, c.b, div255(src[i] * baseA));
                    }
                }
            } else {
                // Rotacao 90 graus horaria: (u, v) do glifo -> (x0 + em - v, y0 + u)
                int em = int(std::lround(devSize));
                for (int j = 0; j < g.h; ++j) {
                    int xx = gx + em - (g.offsetY + j);
                    if (xx < clip_.x0 || xx >= clip_.x1) continue;
                    const uint8_t* src = g.alpha.data() + size_t(j) * g.w;
                    for (int i = 0; i < g.w; ++i) {
                        if (!src[i]) continue;
                        int yy = gy + g.offsetX + i;
                        if (yy < clip_.y0 || yy >= clip_.y1) continue;
                        blendRaw(fb_[size_t(yy) * w_ + xx], c.r, c.g, c.b, div255(src[i] * baseA));
                    }
                }
            }
        }
        float adv = g.advance + st.tracking * devSize / 16.0f;
        if (st.rotated90) penY += adv; else penX += adv;
    }
}

void SoftRenderer::pushClip(const Rectf& r) {
    clipStack_.push_back(clip_);
    ClipI n{int(std::floor(tx(r.x))), int(std::floor(ty(r.y))), int(std::ceil(tx(r.right()))), int(std::ceil(ty(r.bottom())))};
    clip_.x0 = std::max(clip_.x0, n.x0);
    clip_.y0 = std::max(clip_.y0, n.y0);
    clip_.x1 = std::max(clip_.x0, std::min(clip_.x1, n.x1));
    clip_.y1 = std::max(clip_.y0, std::min(clip_.y1, n.y1));
}

void SoftRenderer::popClip() {
    if (clipStack_.empty()) return;
    clip_ = clipStack_.back();
    clipStack_.pop_back();
}

void SoftRenderer::pushAlpha(float a) {
    alphaStack_.push_back(alpha_);
    alpha_ *= clampf(a, 0, 1);
}

void SoftRenderer::popAlpha() {
    if (alphaStack_.empty()) return;
    alpha_ = alphaStack_.back();
    alphaStack_.pop_back();
}

void SoftRenderer::pushOffset(float dx, float dy) {
    offsetStack_.push_back(Vec2{offX_, offY_});
    offX_ += dx;
    offY_ += dy;
}

void SoftRenderer::popOffset() {
    if (offsetStack_.empty()) return;
    offX_ = offsetStack_.back().x;
    offY_ = offsetStack_.back().y;
    offsetStack_.pop_back();
}

TextureId SoftRenderer::createTexture(int w, int h, const uint32_t* argb) {
    if (w <= 0 || h <= 0 || !argb) return kInvalidTexture;
    Texture t;
    t.w = w;
    t.h = h;
    t.px.assign(argb, argb + size_t(w) * h);
    for (uint32_t v : t.px)
        if ((v >> 24) != 0xFF) { t.opaque = false; break; }
    TextureId id = nextTex_++;
    textures_.emplace(id, std::move(t));
    return id;
}

void SoftRenderer::destroyTexture(TextureId id) { textures_.erase(id); }

bool SoftRenderer::captureTexture(TextureId id) {
    auto it = textures_.find(id);
    if (it == textures_.end() || it->second.w != w_ || it->second.h != h_) return false;
    std::memcpy(it->second.px.data(), fb_.data(), fb_.size() * sizeof(uint32_t));
    it->second.opaque = true;  // o framebuffer e sempre opaco
    return true;
}

bool SoftRenderer::updateTexture(TextureId id, int w, int h, const uint32_t* argb) {
    auto it = textures_.find(id);
    if (it == textures_.end() || !argb || it->second.w != w || it->second.h != h) return false;
    Texture& t = it->second;
    std::memcpy(t.px.data(), argb, size_t(w) * h * sizeof(uint32_t));
    t.opaque = true;
    for (uint32_t v : t.px)
        if ((v >> 24) != 0xFF) { t.opaque = false; break; }
    return true;
}

size_t SoftRenderer::textureBytes() const {
    size_t total = 0;
    for (const auto& kv : textures_) total += kv.second.px.size() * sizeof(uint32_t);
    return total;
}

void SoftRenderer::drawTexture(TextureId id, const Rectf& dst, float alpha) {
    auto it = textures_.find(id);
    if (it == textures_.end()) return;
    const Texture& t = it->second;
    float fx0 = tx(dst.x), fy0 = ty(dst.y), fx1 = tx(dst.right()), fy1 = ty(dst.bottom());
    int x0 = int(std::lround(fx0)), y0 = int(std::lround(fy0)), x1 = int(std::lround(fx1)), y1 = int(std::lround(fy1));
    int dw = x1 - x0, dh = y1 - y0;
    if (dw <= 0 || dh <= 0) return;
    uint32_t ga = uint32_t(clampf(alpha * alpha_, 0, 1) * 255.0f + 0.5f);
    if (!ga) return;
    int cx0 = std::max(x0, clip_.x0), cx1 = std::min(x1, clip_.x1);
    int cy0 = std::max(y0, clip_.y0), cy1 = std::min(y1, clip_.y1);
    if (cx0 >= cx1 || cy0 >= cy1) return;

    // Caminho rapido: copia 1:1 opaca.
    if (dw == t.w && dh == t.h && t.opaque && ga >= 255) {
        for (int y = cy0; y < cy1; ++y)
            std::memcpy(fb_.data() + size_t(y) * w_ + cx0, t.px.data() + size_t(y - y0) * t.w + (cx0 - x0), size_t(cx1 - cx0) * 4);
        return;
    }
    // Amostragem bilinear em ponto fixo 16.16.
    const int64_t stepX = (int64_t(t.w) << 16) / dw, stepY = (int64_t(t.h) << 16) / dh;
    for (int y = cy0; y < cy1; ++y) {
        int64_t sy = (y - y0) * stepY + stepY / 2 - 32768;
        if (sy < 0) sy = 0;
        int ty0 = int(sy >> 16), ty1 = std::min(ty0 + 1, t.h - 1);
        uint32_t fy = uint32_t((sy >> 8) & 0xFF);
        if (ty0 >= t.h) { ty0 = ty1 = t.h - 1; fy = 0; }
        const uint32_t* r0 = t.px.data() + size_t(ty0) * t.w;
        const uint32_t* r1 = t.px.data() + size_t(ty1) * t.w;
        uint32_t* out = fb_.data() + size_t(y) * w_;
        for (int x = cx0; x < cx1; ++x) {
            int64_t sx = (x - x0) * stepX + stepX / 2 - 32768;
            if (sx < 0) sx = 0;
            int tx0 = int(sx >> 16), tx1 = std::min(tx0 + 1, t.w - 1);
            uint32_t fx = uint32_t((sx >> 8) & 0xFF);
            if (tx0 >= t.w) { tx0 = tx1 = t.w - 1; fx = 0; }
            uint32_t p00 = r0[tx0], p01 = r0[tx1], p10 = r1[tx0], p11 = r1[tx1];
            uint32_t ch[4];
            for (int k = 0; k < 4; ++k) {
                int sh = k * 8;
                uint32_t a0 = ((p00 >> sh) & 0xFF) * (256 - fx) + ((p01 >> sh) & 0xFF) * fx;
                uint32_t a1 = ((p10 >> sh) & 0xFF) * (256 - fx) + ((p11 >> sh) & 0xFF) * fx;
                ch[k] = (a0 * (256 - fy) + a1 * fy) >> 16;
            }
            uint32_t pa = div255(ch[3] * ga);
            blendRaw(out[x], ch[2], ch[1], ch[0], pa);
        }
    }
}

}  // namespace nx
