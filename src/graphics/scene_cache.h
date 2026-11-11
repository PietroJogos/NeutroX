// Neutro X - cache de cena.
//
// Enquanto nada se move, o quadro de uma interface e identico ao anterior.
// O SceneCache desenha a cena uma vez numa tela fora da tela (na resolucao
// nativa), guarda como textura opaca e, nos quadros seguintes, so a copia
// (memcpy no renderizador em software). Ao animar, desenha ao vivo.
#pragma once
#include <functional>
#include <memory>
#include "core/screen.h"
#include "graphics/soft_renderer.h"

namespace nx {

class SceneCache {
public:
    // Aloca a textura e a tela fora da tela (chamar em load()).
    bool init(IRenderer& r) {
        release(r);
        canvas_.reset(new SoftRenderer(r.pixelWidth(), r.pixelHeight()));
        tex_ = r.createTexture(canvas_->pixelWidth(), canvas_->pixelHeight(), canvas_->pixels());
        valid_ = false;
        return tex_ != kInvalidTexture;
    }
    void release(IRenderer& r) {
        if (tex_ != kInvalidTexture) r.destroyTexture(tex_);
        tex_ = kInvalidTexture;
        canvas_.reset();
        valid_ = false;
    }
    void invalidate() { valid_ = false; }

    // animating: true enquanto houver movimento (desenha ao vivo).
    // version: muda sempre que o conteudo estatico mudar.
    void draw(IRenderer& r, bool animating, unsigned version, const std::function<void(IRenderer&)>& paint) {
        if (animating || tex_ == kInvalidTexture || !canvas_) {
            paint(r);
            valid_ = false;
            return;
        }
        if (!valid_ || version != version_) {
            canvas_->beginFrame();
            paint(*canvas_);
            canvas_->endFrame();
            valid_ = r.updateTexture(tex_, canvas_->pixelWidth(), canvas_->pixelHeight(), canvas_->pixels());
            version_ = version;
            if (!valid_) { paint(r); return; }
            ++rebuilds_;
        }
        r.drawTexture(tex_, Rectf{0, 0, float(kLogicalW), float(kLogicalH)});
    }
    int rebuilds() const { return rebuilds_; }
    // Renderizador fora da tela (texturas usadas na cena em cache precisam existir nele tambem).
    SoftRenderer* canvas() { return canvas_.get(); }

private:
    std::unique_ptr<SoftRenderer> canvas_;
    TextureId tex_ = kInvalidTexture;
    bool valid_ = false;
    unsigned version_ = 0;
    int rebuilds_ = 0;
};

}  // namespace nx
