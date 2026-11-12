// Neutro X - renderizador em software (CPU), portavel.
//
// Desenha num framebuffer 32 bits (0xAARRGGBB por uint32). As coordenadas
// logicas 1280x720 sao escaladas uniformemente para a resolucao real, com
// barras (letterbox) quando a proporcao nao for 16:9.
//
// Modo paralelo (setThreads(n > 1)): as chamadas de desenho do quadro sao
// GRAVADAS e, no fim do quadro (ou antes de capturar/alterar uma textura),
// reexecutadas em faixas horizontais da tela, uma por thread. Cada primitiva
// so escreve dentro do recorte, entao cada faixa produz exatamente os mesmos
// pixels do modo imediato (verificado nos testes, pixel a pixel). No Xbox 360
// isso usa os 6 threads de hardware do Xenon em vez de um so.
#pragma once
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "graphics/renderer.h"

namespace nx {

class SoftRenderer : public IRenderer {
public:
    SoftRenderer(int width, int height);
    ~SoftRenderer() override;
    SoftRenderer(const SoftRenderer&) = delete;
    SoftRenderer& operator=(const SoftRenderer&) = delete;

    // Acesso ao framebuffer (para apresentacao pela plataforma ou capturas).
    const uint32_t* pixels() const { return fbp_; }
    uint32_t* pixels() { return fbp_; }
    void resize(int width, int height);
    float scale() const { return scale_; }

    // 1 = imediato (padrao). n > 1: grava e rasteriza em n threads (a thread que
    // chama endFrame e uma delas). workerInit(i) roda uma vez em cada thread
    // auxiliar i (1..n-1), p.ex. para fixa-la num thread de hardware.
    void setThreads(int n, std::function<void(int)> workerInit = {});
    int threads() const { return threads_; }
    // Executa agora os comandos gravados (no modo imediato nao faz nada).
    void flush();
    // Diagnostico: comandos gravados no ultimo quadro e faixas usadas.
    int lastCommandCount() const { return lastCmds_; }

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
    int textureCount() const override { return int(texs_->size()); }
    size_t textureBytes() const override;

private:
    struct ClipI { int x0, y0, x1, y1; };
    struct Texture {
        int w = 0, h = 0;
        bool opaque = true;
        std::vector<uint32_t> px;
    };
    struct State {
        ClipI clip{};
        std::vector<ClipI> clipStack;
        float alpha = 1.0f;
        std::vector<float> alphaStack;
        float offX = 0, offY = 0;
        std::vector<Vec2> offsetStack;
    };
    enum class Op : uint8_t {
        Clear, FillRect, FillRoundRect, GradV, GradH, Radial, FillCircle, StrokeCircle, StrokeRect,
        StrokeRoundRect, Line, Convex, Text, PushClip, PopClip, PushAlpha, PopAlpha, PushOffset, PopOffset, DrawTex
    };
    struct Cmd {
        Op op;
        int yb0, yb1;  // linhas do dispositivo que o comando pode tocar (rejeicao rapida por faixa)
        Color c0, c1;
        float f[7];
        int i0, i1;
    };
    struct Pool;

    // Faixa: renderizador auxiliar que compartilha framebuffer e texturas do dono.
    struct LaneTag {};
    SoftRenderer(const SoftRenderer& owner, LaneTag);

    float tx(float x) const { return ox_ + (x + offX_) * scale_; }
    float ty(float y) const { return oy_ + (y + offY_) * scale_; }
    uint32_t effAlpha(uint8_t a) const;
    void blendSpan(int y, int x0, int x1, Color c, uint32_t alpha);
    void blendPixel(int x, int y, Color c, uint32_t alpha);
    void convexDevice(const Vec2* pts, int count, Color c);
    std::map<TextureId, Texture>& tex() { return *texs_; }
    const std::map<TextureId, Texture>& tex() const { return *texs_; }

    bool recording() const { return threads_ > 1; }
    Cmd& rec(Op op, float yLogical0, float yLogical1);  // limites em coordenadas logicas (com offset atual)
    Cmd& recFull(Op op);
    State saveState() const;
    void loadState(const State& s, int band0, int band1);
    void replayFrom(const SoftRenderer& src, int band0, int band1);
    void ensureLanes();

    int w_, h_;
    std::vector<uint32_t> fb_;        // dono: o framebuffer
    uint32_t* fbp_ = nullptr;         // framebuffer em uso (do dono, nas faixas)
    std::map<TextureId, Texture> ownTex_;
    std::map<TextureId, Texture>* texs_ = &ownTex_;
    float scale_ = 1, ox_ = 0, oy_ = 0;
    float offX_ = 0, offY_ = 0;
    ClipI clip_{};
    std::vector<ClipI> clipStack_;
    float alpha_ = 1.0f;
    std::vector<float> alphaStack_;
    std::vector<Vec2> offsetStack_;
    TextureId nextTex_ = 1;
    int bandY0_ = 0, bandY1_ = 0;     // linhas que este renderizador pode escrever
    bool lane_ = false;

    // modo paralelo
    int threads_ = 1;
    std::function<void(int)> workerInit_;
    std::vector<Cmd> cmds_;
    std::vector<Vec2> pts_;
    std::string text_;
    std::vector<TextStyle> styles_;
    State batch_;                      // estado no inicio dos comandos pendentes
    std::vector<std::unique_ptr<SoftRenderer>> lanes_;
    std::unique_ptr<Pool> pool_;
    bool fontPinned_ = false;
    int lastCmds_ = 0;
};

}  // namespace nx
