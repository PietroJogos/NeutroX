// Neutro X - fonte vetorial propria (tracos), rasterizada sob demanda com antialiasing.
//
// Cada glifo e descrito por polilinhas numa grade de 16 unidades de altura
// (topo das maiusculas y=2, linha de base y=12, altura-x y=5, descendente ate 15.5).
// Os tracos sao rasterizados por distancia (capsulas), produzindo um bitmap
// alfa por (glifo, tamanho, peso) mantido em cache.
// A fonte foi desenhada especificamente para o Neutro X (estilo geometrico).
#pragma once
#include <cstdint>
#include <unordered_map>
#include <vector>
#include "graphics/renderer.h"

namespace nx {

struct GlyphBitmap {
    int w = 0, h = 0;
    int offsetX = 0;  // deslocamento do bitmap em relacao a caneta (pixels)
    int offsetY = 0;  // deslocamento em relacao ao topo do em (pixels)
    float advance = 0;  // avanco horizontal em pixels (sem tracking)
    std::vector<uint8_t> alpha;
};

class StrokeFont {
public:
    static StrokeFont& instance();

    // pixelSize = altura do em em pixels reais.
    const GlyphBitmap& glyph(uint32_t codepoint, float pixelSize, FontWeight weight);
    float advance(uint32_t codepoint, float pixelSize, FontWeight weight);
    bool hasGlyph(uint32_t codepoint) const;

    void clearCache();
    size_t cacheBytes() const { return cacheBytes_; }
    size_t cacheEntries() const { return cache_.size(); }

private:
    StrokeFont();
    struct Def {
        float width = 0;
        std::vector<std::vector<Vec2>> lines;
    };
    const Def& def(uint32_t cp) const;
    void addPath(uint32_t cp, float width, const char* path);
    void addComposed(uint32_t cp, uint32_t base, int accent);

    std::unordered_map<uint32_t, Def> defs_;
    Def fallback_;
    std::unordered_map<uint64_t, GlyphBitmap> cache_;
    size_t cacheBytes_ = 0;
};

// Decodifica o proximo codepoint UTF-8 (avanca p). Retorna 0 no fim da string.
uint32_t utf8Next(const char*& p);

}  // namespace nx
