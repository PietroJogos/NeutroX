// Neutro X - utilidades compartilhadas pelas telas do shell.
#pragma once
#include <cstdio>
#include "core/app.h"
#include "graphics/soft_renderer.h"
#include "graphics/ui.h"
#include "platform/platform.h"

namespace nx {

// Fundo do Neutro X em cache (textura na resolucao nativa). Reconstruido se a
// cor de destaque mudar.
class NeutroBackdrop {
public:
    void ensure(Context& ctx) {
        Color a = ctx.accent();
        if (tex_ != kInvalidTexture && a.argb() == accent_.argb()) return;
        release(ctx);
        IRenderer& r = *ctx.renderer;
        SoftRenderer canvas(r.pixelWidth(), r.pixelHeight());
        canvas.beginFrame();
        drawNeutroBackground(canvas, a);
        tex_ = r.createTexture(canvas.pixelWidth(), canvas.pixelHeight(), canvas.pixels());
        accent_ = a;
    }
    void release(Context& ctx) {
        if (tex_ != kInvalidTexture) ctx.renderer->destroyTexture(tex_);
        tex_ = kInvalidTexture;
    }
    void draw(Context& ctx, IRenderer& r) {
        ensure(ctx);
        r.drawTexture(tex_, Rectf{0, 0, float(kLogicalW), float(kLogicalH)});
        drawNeutroBands(r, float(ctx.time), ctx.accent());
    }

private:
    TextureId tex_ = kInvalidTexture;
    Color accent_;
};

// Relogio no canto superior direito.
inline void drawClock(Context& ctx, IRenderer& r, float rightX, float y, Color color) {
    int h = 0, m = 0, d = 1, mo = 1, yr = 2026;
    ctx.platform->localTime(h, m, d, mo, yr);
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%02d:%02d", h, m);
    TextStyle st;
    st.size = 30;
    st.align = Align::Right;
    st.color = color;
    r.drawText(buf, rightX, y, st);
    std::snprintf(buf, sizeof(buf), "%02d/%02d/%04d", d, mo, yr);
    st.size = 16;
    st.color = color.fade(0.6f);
    r.drawText(buf, rightX, y + 36, st);
}

}  // namespace nx
