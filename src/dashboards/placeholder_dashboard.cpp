// Neutro X - dashboard provisorio.
//
// Usado ate que a recriacao completa de cada interface seja entregue na sua
// fase (4 a 8). Ja exercita o ciclo completo: load() aloca uma textura de
// fundo em resolucao nativa, a navegacao entre secoes funciona e unload()
// libera tudo - permitindo testar a troca repetida de dashboards.
#include <string>
#include "core/anim.h"
#include "core/app.h"
#include "dashboards/dashboard.h"
#include "graphics/soft_renderer.h"
#include "graphics/ui.h"

namespace nx {

namespace {

class PlaceholderDashboard : public Dashboard {
public:
    explicit PlaceholderDashboard(DashboardId id) : Dashboard(id) {}
    ~PlaceholderDashboard() override = default;

    bool load(Context& ctx, std::string& error) override {
        IRenderer& r = *ctx.renderer;
        int w = r.pixelWidth(), h = r.pixelHeight();
        SoftRenderer canvas(w, h);
        canvas.beginFrame();
        info().drawPreview(canvas, 0.0f);
        bg_ = r.createTexture(w, h, canvas.pixels());
        if (bg_ == kInvalidTexture) {
            error = "falha ao criar a textura de fundo";
            return false;
        }
        return true;
    }

    void unload(Context& ctx) override {
        if (bg_ != kInvalidTexture) ctx.renderer->destroyTexture(bg_);
        bg_ = kInvalidTexture;
    }

    void onEnter(Context&) override {
        enter_.start(0.45f);
        sel_.snap(0);
    }

    void update(Context& ctx, const InputFrame& in, float dt) override {
        enter_.update(dt);
        sel_.update(dt);
        const DashboardInfo& inf = info();
        int n = inf.sectionCount;
        if (in.press(Button::RB) || in.press(Button::Right)) {
            if (section_ + 1 < n) { ++section_; ctx.sfx(Sfx::Tab); } else ctx.sfx(Sfx::Error, 0.5f);
        }
        if (in.press(Button::LB) || in.press(Button::Left)) {
            if (section_ > 0) { --section_; ctx.sfx(Sfx::Tab); } else ctx.sfx(Sfx::Error, 0.5f);
        }
        sel_.set(float(section_));
        if (in.press(Button::A)) {
            ctx.sfx(Sfx::Confirm);
            ctx.app->toast(std::string(inf.sections[section_]) + ": tela demonstrativa (recriação completa na Fase " +
                           std::to_string(inf.phase) + ")");
        }
        if (in.press(Button::B)) {
            ctx.sfx(Sfx::Back, 0.6f);
            ctx.app->toast("Pressione START para abrir o menu do Neutro X");
        }
        if (in.press(Button::Y)) {
            ctx.sfx(Sfx::Open);
            ctx.app->openGlobalMenu();
        }
    }

    void render(Context& ctx, IRenderer& r) override {
        const DashboardInfo& inf = info();
        float e = ease::outCubic(enter_.progress());
        r.drawTexture(bg_, Rectf{0, 0, float(kLogicalW), float(kLogicalH)}, 1.0f);
        if (e < 1) r.fillRect(Rectf{0, 0, 1280, 720}, Color(0, 0, 0, uint8_t((1 - e) * 255)));

        // Barra provisoria inferior com as secoes navegaveis.
        r.pushOffset(0, (1 - e) * 60);
        Rectf bar{40, 560, 1200, 120};
        r.fillRoundRect(bar, 14, Color(10, 12, 16, 215));
        r.strokeRoundRect(bar, 14, 1.5f, inf.accent.withAlpha(180));
        TextStyle st;
        st.size = 18;
        st.color = Color(170, 178, 188);
        std::string head = std::string(inf.name) + "  ·  pré-visualização provisória  ·  recriação completa na Fase " +
                           std::to_string(inf.phase);
        r.drawText(head.c_str(), 64, 574, st);
        // Abas de secao
        float x = 64;
        TextStyle ts;
        ts.size = 24;
        float selX = 0, selW = 0;
        float xs[16], ws[16];
        for (int i = 0; i < inf.sectionCount && i < 16; ++i) {
            ws[i] = r.measureText(inf.sections[i], ts) + 28;
            xs[i] = x;
            x += ws[i] + 8;
        }
        // destaque deslizante
        int a = int(sel_.value), b = a + 1 < inf.sectionCount ? a + 1 : a;
        float f = sel_.value - a;
        selX = lerpf(xs[a], xs[b], f);
        selW = lerpf(ws[a], ws[b], f);
        r.fillRoundRect(Rectf{selX, 606, selW, 40}, 8, inf.accent);
        for (int i = 0; i < inf.sectionCount && i < 16; ++i) {
            ts.color = i == section_ ? Color(255, 255, 255) : Color(200, 205, 212);
            r.drawText(inf.sections[i], xs[i] + 14, 613, ts);
        }
        drawHints(r, {{Button::LB, "/"}, {Button::RB, "Seção"}, {Button::Start, "Menu Neutro X"}}, 1216, 590, 28,
                  Color(200, 205, 212), Align::Right);
        r.popOffset();
        (void)ctx;
    }

private:
    TextureId bg_ = kInvalidTexture;
    int section_ = 0;
    Smooth sel_{0, 16};
    Tween enter_;
};

}  // namespace

std::unique_ptr<Dashboard> createPlaceholderDashboard(DashboardId id) {
    return std::unique_ptr<Dashboard>(new PlaceholderDashboard(id));
}

}  // namespace nx
