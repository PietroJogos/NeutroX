// Neutro X - interface abstrata de renderizacao.
//
// Todo o codigo de interface desenha em coordenadas logicas 1280x720.
// Implementacoes:
//   - SoftRenderer (graphics/soft_renderer.*): rasterizador em CPU, portavel,
//     usado no Windows e como base para o console.
//   - (futuro) renderizador acelerado por GPU no Xbox 360 (Xenos), ver docs/TOOLCHAIN.md.
#pragma once
#include <cstddef>
#include <cstdint>
#include "core/types.h"

namespace nx {

enum class Align { Left, Center, Right };

enum class FontWeight { Light = 0, Regular = 1, Bold = 2 };

struct TextStyle {
    float size = 24.0f;  // altura do "em" em pixels logicos
    Color color = Color(255, 255, 255);
    Align align = Align::Left;
    FontWeight weight = FontWeight::Regular;
    float tracking = 0.0f;  // espacamento extra entre letras (unidades do em/16)
    bool rotated90 = false;  // texto girado 90 graus no sentido horario (lido de cima para baixo)
};

using TextureId = int;
constexpr TextureId kInvalidTexture = -1;

class IRenderer {
public:
    virtual ~IRenderer() = default;

    // Resolucao real do alvo de desenho (pixels).
    virtual int pixelWidth() const = 0;
    virtual int pixelHeight() const = 0;

    virtual void beginFrame() = 0;
    virtual void endFrame() = 0;

    virtual void clear(Color c) = 0;
    virtual void fillRect(const Rectf& r, Color c) = 0;
    virtual void fillRoundRect(const Rectf& r, float radius, Color c) = 0;
    virtual void fillGradientV(const Rectf& r, Color top, Color bottom) = 0;
    virtual void fillGradientH(const Rectf& r, Color left, Color right) = 0;
    // Gradiente radial: centro -> borda (raio em pixels logicos).
    virtual void fillRadial(float cx, float cy, float radius, Color inner, Color outer, const Rectf& area) = 0;
    virtual void fillCircle(float cx, float cy, float radius, Color c) = 0;
    virtual void strokeCircle(float cx, float cy, float radius, float thickness, Color c) = 0;
    virtual void strokeRect(const Rectf& r, float thickness, Color c) = 0;
    virtual void strokeRoundRect(const Rectf& r, float radius, float thickness, Color c) = 0;
    virtual void drawLine(float x0, float y0, float x1, float y1, float thickness, Color c) = 0;
    // Poligono convexo (pontos em ordem).
    virtual void fillConvex(const Vec2* pts, int count, Color c) = 0;

    // Texto UTF-8. (x, y) = canto superior da caixa do em.
    virtual void drawText(const char* utf8, float x, float y, const TextStyle& style) = 0;
    virtual float measureText(const char* utf8, const TextStyle& style) = 0;

    // Recorte (pilha).
    virtual void pushClip(const Rectf& r) = 0;
    virtual void popClip() = 0;
    // Opacidade global multiplicativa (pilha).
    virtual void pushAlpha(float a) = 0;
    virtual void popAlpha() = 0;
    // Deslocamento global (pilha) - util para animacoes de entrada.
    virtual void pushOffset(float dx, float dy) = 0;
    virtual void popOffset() = 0;

    // Texturas (pixels 0xAARRGGBB, alfa nao pre-multiplicado).
    virtual TextureId createTexture(int w, int h, const uint32_t* argb) = 0;
    virtual void destroyTexture(TextureId id) = 0;
    virtual void drawTexture(TextureId id, const Rectf& dst, float alpha = 1.0f) = 0;
    virtual int textureCount() const = 0;
    virtual size_t textureBytes() const = 0;
};

}  // namespace nx
