// Neutro X - renderizador em software (CPU), portavel.
//
// Desenha num framebuffer 32 bits (0xAARRGGBB por uint32). As coordenadas
// logicas 1280x720 sao escaladas uniformemente para a resolucao real, com
// barras (letterbox) quando a proporcao nao for 16:9.
#pragma once
#include <map>
#include <vector>
#include "graphics/renderer.h"

namespace nx {

class SoftRenderer : public IRenderer {
public:
    SoftRenderer(int width, int height);

    // Acesso ao framebuffer (para apresentacao pela plataforma ou capturas).
    const uint32_t* pixels() const { return fb_.data(); }
    uint32_t* pixels() { return fb_.data(); }
    void resize(int width, int height);
    float scale() const { return scale_; }

    int pixelWidth() const override { return w_; }
    int pixelHeight() const override { return h_; }
    void beginFrame() override;
    void endFrame() override;

    void clear(Color c) override;
    void fillRect(const Rectf& r, Color c) override;
    void fillRoundRect(const Rectf& r, float radius, Color c) override;
    void fillGradientV(const Rectf& r, Color top, Color bottom) override;
    void fillGradientH(const Rectf& r, Color left, Color right) override;
    void fillRadial(float cx, float cy, float radius, Color inner, Color outer, const Rectf& area) override;
    void fillCircle(float cx, float cy, float radius, Color c) override;
    void strokeCircle(float cx, float cy, float radius, float thickness, Color c) override;
    void strokeRect(const Rectf& r, float thickness, Color c) override;
    void strokeRoundRect(const Rectf& r, float radius, float thickness, Color c) override;
    void drawLine(float x0, float y0, float x1, float y1, float thickness, Color c) override;
    void fillConvex(const Vec2* pts, int count, Color c) override;

    void drawText(const char* utf8, float x, float y, const TextStyle& style) override;
    float measureText(const char* utf8, const TextStyle& style) override;

    void pushClip(const Rectf& r) override;
    void popClip() override;
    void pushAlpha(float a) override;
    void popAlpha() override;
    void pushOffset(float dx, float dy) override;
    void popOffset() override;

    TextureId createTexture(int w, int h, const uint32_t* argb) override;
    void destroyTexture(TextureId id) override;
    bool updateTexture(TextureId id, int w, int h, const uint32_t* argb) override;
    bool captureTexture(TextureId id) override;
    void drawTexture(TextureId id, const Rectf& dst, float alpha = 1.0f) override;
    int textureCount() const override { return int(textures_.size()); }
    size_t textureBytes() const override;

private:
    struct ClipI { int x0, y0, x1, y1; };
    struct Texture {
        int w = 0, h = 0;
        bool opaque = true;
        std::vector<uint32_t> px;
    };

    float tx(float x) const { return ox_ + (x + offX_) * scale_; }
    float ty(float y) const { return oy_ + (y + offY_) * scale_; }
    uint32_t effAlpha(uint8_t a) const;
    void blendSpan(int y, int x0, int x1, Color c, uint32_t alpha);
    void blendPixel(int x, int y, Color c, uint32_t alpha);
    void convexDevice(const Vec2* pts, int count, Color c);

    int w_, h_;
    std::vector<uint32_t> fb_;
    float scale_ = 1, ox_ = 0, oy_ = 0;
    float offX_ = 0, offY_ = 0;
    ClipI clip_{};
    std::vector<ClipI> clipStack_;
    float alpha_ = 1.0f;
    std::vector<float> alphaStack_;
    std::vector<Vec2> offsetStack_;
    std::map<TextureId, Texture> textures_;
    TextureId nextTex_ = 1;
};

}  // namespace nx
