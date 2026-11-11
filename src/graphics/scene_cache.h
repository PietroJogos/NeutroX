// Neutro X - cache de cena.
//
// Enquanto nada se move, o quadro de uma interface e identico ao anterior.
// Quando o conteudo muda (versao nova) e a cena esta parada, o SceneCache
// desenha a cena normalmente no quadro atual e, em seguida, copia o quadro
// para uma textura (captureTexture). Nos quadros seguintes so a textura e
// desenhada (memcpy no renderizador em software). Ao animar, desenha ao vivo.
//
// Usa uma unica textura na resolucao real: nao ha tela fora da tela nem copias
// duplicadas de recursos (fase 9: metade da memoria da versao anterior).
// Requisito: a cena deve ser a primeira coisa desenhada no quadro (as telas
// de cima e os avisos do aplicativo sao desenhados depois da captura).
#pragma once
#include <functional>
#include <vector>
#include "core/screen.h"

namespace nx {

class SceneCache {
public:
    bool init(IRenderer& r) {
        release(r);
        std::vector<uint32_t> blank(size_t(r.pixelWidth()) * r.pixelHeight(), 0xFF000000u);
        tex_ = r.createTexture(r.pixelWidth(), r.pixelHeight(), blank.data());
        valid_ = false;
        return tex_ != kInvalidTexture;
    }
    void release(IRenderer& r) {
        if (tex_ != kInvalidTexture) r.destroyTexture(tex_);
        tex_ = kInvalidTexture;
        valid_ = false;
    }
    void invalidate() { valid_ = false; }

    // animating: true enquanto houver movimento (desenha ao vivo).
    // version: muda sempre que o conteudo estatico mudar.
    void draw(IRenderer& r, bool animating, unsigned version, const std::function<void(IRenderer&)>& paint) {
        if (animating || tex_ == kInvalidTexture) {
            paint(r);
            valid_ = false;
            return;
        }
        if (!valid_ || version != version_) {
            paint(r);
            valid_ = r.captureTexture(tex_);
            version_ = version;
            ++rebuilds_;
            return;
        }
        r.drawTexture(tex_, Rectf{0, 0, float(kLogicalW), float(kLogicalH)});
        ++hits_;
    }
    int rebuilds() const { return rebuilds_; }
    long hits() const { return hits_; }

private:
    TextureId tex_ = kInvalidTexture;
    bool valid_ = false;
    unsigned version_ = 0;
    int rebuilds_ = 0;
    long hits_ = 0;
};

}  // namespace nx
