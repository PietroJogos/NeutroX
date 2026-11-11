// Neutro X - telas e contexto compartilhado.
#pragma once
#include <string>
#include "audio/audio.h"
#include "graphics/renderer.h"
#include "input/input.h"
#include "settings/settings.h"

namespace nx {

class App;
class IPlatform;

struct Context {
    App* app = nullptr;
    IPlatform* platform = nullptr;
    IRenderer* renderer = nullptr;
    IAudio* audio = nullptr;
    Settings* settings = nullptr;
    double time = 0;  // segundos desde o inicio
    Color accent() const;
    void sfx(Sfx s, float vol = 1.0f) const;
};

class Screen {
public:
    virtual ~Screen() = default;
    virtual const char* name() const = 0;
    virtual void onEnter(Context&) {}
    virtual void onExit(Context&) {}
    virtual void update(Context& ctx, const InputFrame& in, float dt) = 0;
    virtual void render(Context& ctx, IRenderer& r) = 0;
    // Overlays desenham a tela de baixo antes de si mesmos.
    virtual bool isOverlay() const { return false; }
    // Se true, START abre o menu do Neutro X sobre esta tela.
    virtual bool wantsGlobalMenu() const { return false; }
};

}  // namespace nx
