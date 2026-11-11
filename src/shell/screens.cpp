// Neutro X - telas do shell: abertura, configuracoes, sobre, menu global,
// dialogo de confirmacao e tela de erro.
#include "shell/screens.h"

#include <string>
#include <vector>
#include "core/anim.h"
#include "shell/shell_common.h"

namespace nx {

namespace {

// ------------------------------------------------------------- Abertura

class BootScreen : public Screen {
public:
    const char* name() const override { return "Boot"; }
    void onEnter(Context& ctx) override {
        t_ = 0;
        ctx.sfx(Sfx::Startup);
    }
    void update(Context& ctx, const InputFrame& in, float dt) override {
        t_ += dt;
        bool skip = in.press(Button::A) || in.press(Button::Start);
        if (!done_ && (t_ >= 2.4f || skip)) {
            done_ = true;
            const DashboardInfo* def = findDashboard(ctx.settings->defaultDashboard);
            if (def) ctx.app->openDashboard(def->id);
            else ctx.app->openMainMenu();
        }
    }
    void render(Context& ctx, IRenderer& r) override {
        r.clear(Color::hex(0x07090C));
        const Color accent = ctx.accent();
        float p = ease::outCubic(clampf(t_ / 1.4f, 0, 1));
        r.fillRadial(640, 360, 300 + 260 * p, accent.withAlpha(uint8_t(60 * p)), accent.withAlpha(0),
                     Rectf{0, 0, 1280, 720});
        TextStyle st;
        st.size = 84;
        st.weight = FontWeight::Bold;
        st.tracking = 2.5f;
        float w = r.measureText("NEUTRO", st);
        float total = w + 84 * 0.18f + 84 * 10.0f / 16.0f * 1.05f;
        drawNeutroLogo(r, 640 - total * 0.5f, 290, 84, accent, p);
        TextStyle sub;
        sub.size = 22;
        sub.align = Align::Center;
        sub.tracking = 2.0f;
        sub.color = Color(150, 160, 172);
        r.pushAlpha(clampf((t_ - 0.8f) / 0.6f, 0, 1));
        r.drawText("XBOX 360 DASHBOARD EXPERIENCE", 640, 400, sub);
        r.popAlpha();
        sub.size = 15;
        sub.tracking = 0;
        sub.color = Color(90, 98, 110);
        r.pushAlpha(clampf((t_ - 1.2f) / 0.6f, 0, 1));
        r.drawText("Aplicativo independente · não modifica o sistema do console", 640, 650, sub);
        r.popAlpha();
    }

private:
    float t_ = 0;
    bool done_ = false;
};

// -------------------------------------------------------- Configuracoes

enum class Row { DefaultDash, Volume, SoundSource, SoundStatus, Anim, AnimIntensity, Accent, Language, ShowFps, Restore, Version, Count };

class SettingsScreen : public Screen {
public:
    const char* name() const override { return "Settings"; }
    void onEnter(Context& ctx) override {
        enter_.start(0.45f);
        hl_.snap(rowY(0));
        backdrop_.ensure(ctx);
    }
    void onExit(Context& ctx) override { backdrop_.release(ctx); }

    static float rowY(int i) { return 132.0f + i * 45.0f; }

    void update(Context& ctx, const InputFrame& in, float dt) override {
        enter_.update(dt);
        hl_.update(dt);
        {
            int total = ctx.audio->originalsReport().totalLoaded();
            soundSummary_ = std::to_string(total) + " de " + std::to_string((kSoundThemeCount - 1) * kSfxCount) + " carregados";
        }
        const int n = int(Row::Count);
        int before = sel_;
        if (in.press(Button::Down)) sel_ = (sel_ + 1) % n;
        if (in.press(Button::Up)) sel_ = (sel_ + n - 1) % n;
        if (sel_ != before) { ctx.sfx(Sfx::Navigate); hl_.set(rowY(sel_)); }
        int delta = (in.press(Button::Right) ? 1 : 0) - (in.press(Button::Left) ? 1 : 0);
        Settings& s = *ctx.settings;
        Row row = Row(sel_);
        if (delta != 0) change(ctx, row, delta);
        if (in.press(Button::A)) {
            if (row == Row::Restore) {
                ctx.sfx(Sfx::Open);
                ctx.app->push(createConfirmDialog("Restaurar configurações?",
                                                  "Todas as preferências do Neutro X voltarão aos valores padrão.",
                                                  [](Context& c) {
                                                      *c.settings = Settings::defaults();
                                                      c.app->applySettings();
                                                      c.app->saveSettings();
                                                      c.app->toast("Configurações restauradas");
                                                  }));
            } else if (row == Row::SoundStatus) {
                ctx.sfx(Sfx::Open);
                ctx.app->openSoundStatus();
            } else if (row == Row::Anim || row == Row::ShowFps) {
                change(ctx, row, 1);
            } else if (row == Row::Version || row == Row::Language) {
                ctx.sfx(Sfx::Error, 0.5f);
            } else {
                change(ctx, row, 1);
            }
        }
        if (in.press(Button::B)) {
            ctx.sfx(Sfx::Back);
            s.sanitize();
            if (ctx.app->saveSettings()) ctx.app->toast("Configurações salvas");
            ctx.app->pop();
        }
    }

    void change(Context& ctx, Row row, int delta) {
        Settings& s = *ctx.settings;
        bool ok = true;
        switch (row) {
            case Row::DefaultDash: {
                // "", blades, nxe, kinect, metro_v1, metro_v2
                int cur = 0;
                const DashboardInfo* d = findDashboard(s.defaultDashboard);
                if (d) cur = int(d->id) + 1;
                cur = (cur + delta + kDashboardCount + 1) % (kDashboardCount + 1);
                s.defaultDashboard = cur == 0 ? "" : dashboardInfo(DashboardId(cur - 1)).key;
                break;
            }
            case Row::Volume:
                s.sfxVolume = clampi(s.sfxVolume + delta * 10, 0, 100);
                break;
            case Row::SoundSource: s.dashboardSounds = s.dashboardSounds == 0 ? 1 : 0; break;
            case Row::Anim: s.animations = !s.animations; break;
            case Row::AnimIntensity: s.animIntensity = AnimIntensity(clampi(int(s.animIntensity) + delta, 0, 2)); break;
            case Row::Accent: s.accentTheme = (s.accentTheme + delta + kAccentThemeCount) % kAccentThemeCount; break;
            case Row::ShowFps: s.showFps = !s.showFps; break;
            default: ok = false; break;
        }
        ctx.app->applySettings();
        ctx.sfx(ok ? Sfx::Tab : Sfx::Error, ok ? 1.0f : 0.5f);
    }

    std::string valueText(const Settings& s, Row row) const {
        switch (row) {
            case Row::DefaultDash: {
                const DashboardInfo* d = findDashboard(s.defaultDashboard);
                return d ? d->name : "Menu do Neutro X";
            }
            case Row::Volume: return std::to_string(s.sfxVolume) + "%";
            case Row::SoundSource: return s.dashboardSounds == 0 ? "Originais (pasta local)" : "Recriados pelo Neutro X";
            case Row::SoundStatus: return soundSummary_;
            case Row::Anim: return s.animations ? "Ativadas" : "Desativadas";
            case Row::AnimIntensity:
                return s.animIntensity == AnimIntensity::Low ? "Baixa" : s.animIntensity == AnimIntensity::High ? "Alta" : "Média";
            case Row::Accent: return kAccentThemes[s.accentTheme].name;
            case Row::Language: return "Português (Brasil)";
            case Row::ShowFps: return s.showFps ? "Sim" : "Não";
            case Row::Restore: return "";
            case Row::Version: return std::string("Neutro X ") + kNeutroXVersion;
            default: return "";
        }
    }

    void render(Context& ctx, IRenderer& r) override {
        backdrop_.draw(ctx, r);
        const Color accent = ctx.accent();
        float e = ease::outCubic(enter_.progress());
        r.pushAlpha(e);
        drawIcon(r, Icon::Gear, 82, 82, 46, accent, Color::hex(0x0E141B));
        TextStyle t;
        t.size = 44;
        t.weight = FontWeight::Bold;
        t.color = Color(255, 255, 255);
        r.drawText("Configurações", 124, 56, t);
        drawClock(ctx, r, 1224, 40, Color(220, 226, 234));

        static const char* const labels[] = {"Dashboard padrão", "Volume dos efeitos", "Sons dos dashboards", "Sons originais: estado", "Animações", "Intensidade das animações",
                                             "Tema de cores", "Idioma", "Mostrar FPS", "Restaurar configurações padrão",
                                             "Versão"};
        Rectf hl{60, hl_.value - 2, 1160, 42};
        r.fillRoundRect(hl, 8, accent.withAlpha(200));
        r.fillGradientH(hl, Color(255, 255, 255, 30), Color(255, 255, 255, 0));
        const Settings& s = *ctx.settings;
        for (int i = 0; i < int(Row::Count); ++i) {
            float y = rowY(i);
            float k = ease::outCubic(clampf((enter_.progress() - i * 0.04f) / 0.5f, 0, 1));
            r.pushOffset((1 - k) * 40, 0);
            bool sel = i == sel_;
            TextStyle ls;
            ls.size = 24;
            ls.color = sel ? Color(255, 255, 255) : Color(196, 204, 214);
            ls.weight = sel ? FontWeight::Bold : FontWeight::Regular;
            r.drawText(labels[i], 84, y + 8, ls);
            std::string v = valueText(s, Row(i));
            TextStyle vs;
            vs.size = 24;
            vs.align = Align::Right;
            vs.color = sel ? Color(255, 255, 255) : Color(160, 170, 182);
            Row row = Row(i);
            bool adjustable = row != Row::Restore && row != Row::Version && row != Row::Language && row != Row::SoundStatus;
            float rx = 1190;
            if (adjustable && sel) {
                TextStyle ar = vs;
                ar.weight = FontWeight::Bold;
                r.drawText("\xE2\x86\x92", rx, y + 8, ar);  // seta
                rx -= 34;
            }
            if (row == Row::Volume) {
                // barra de volume
                Rectf bar{rx - 220, y + 17, 160, 8};
                r.fillRoundRect(bar, 4, Color(255, 255, 255, 50));
                r.fillRoundRect(Rectf{bar.x, bar.y, bar.w * s.sfxVolume / 100.0f, bar.h}, 4, Color(255, 255, 255, 220));
            }
            if (row == Row::Accent) {
                r.fillCircle(rx - r.measureText(v.c_str(), vs) - 22, y + 21, 9, Color::hex(kAccentThemes[s.accentTheme].rgb));
                r.strokeCircle(rx - r.measureText(v.c_str(), vs) - 22, y + 21, 9, 1.5f, Color(255, 255, 255, 180));
            }
            r.drawText(v.c_str(), rx, y + 8, vs);
            if (adjustable && sel) {
                TextStyle ar = vs;
                ar.weight = FontWeight::Bold;
                ar.align = Align::Right;
                r.drawText("\xE2\x86\x90", rx - r.measureText(v.c_str(), vs) - (row == Row::Accent ? 48 : 16) -
                                                (row == Row::Volume ? 240 : 0),
                           y + 8, ar);
            }
            r.popOffset();
        }
        TextStyle note;
        note.size = 16;
        note.color = Color(120, 130, 142);
        std::string p = "Arquivo: " + ctx.platform->dataPath() + "neutrox.ini";
        r.drawText(p.c_str(), 84, 644, note);
        r.fillRect(Rectf{0, 668, 1280, 52}, Color(0, 0, 0, 90));
        drawHints(r, {{Button::Left, "/"}, {Button::Right, "Alterar"}, {Button::A, "Selecionar"}, {Button::B, "Salvar e voltar"}},
                  56, 694, 30, Color(205, 212, 220));
        r.popAlpha();
    }

private:
    NeutroBackdrop backdrop_;
    int sel_ = 0;
    Smooth hl_{0, 18};
    Tween enter_;
    std::string soundSummary_;
};

// ----------------------------------------------------------------- Sobre

class AboutScreen : public Screen {
public:
    const char* name() const override { return "About"; }
    void onEnter(Context& ctx) override { enter_.start(0.5f); backdrop_.ensure(ctx); }
    void onExit(Context& ctx) override { backdrop_.release(ctx); }
    void update(Context& ctx, const InputFrame& in, float dt) override {
        enter_.update(dt);
        if (in.press(Button::B) || in.press(Button::A)) { ctx.sfx(Sfx::Back); ctx.app->pop(); }
    }
    void render(Context& ctx, IRenderer& r) override {
        backdrop_.draw(ctx, r);
        float e = ease::outCubic(enter_.progress());
        r.pushAlpha(e);
        r.pushOffset(0, (1 - e) * 24);
        drawNeutroLogo(r, 84, 70, 64, ctx.accent());
        TextStyle v;
        v.size = 22;
        v.color = Color(160, 170, 182);
        std::string ver = std::string("Versão ") + kNeutroXVersion + "  ·  Xbox 360 Dashboard Experience";
        r.drawText(ver.c_str(), 88, 150, v);
        TextStyle b;
        b.size = 21;
        b.color = Color(214, 220, 228);
        float y = 210;
        y += drawWrapped(r, "O Neutro X é um aplicativo independente que apresenta recriações visuais e interativas de cinco gerações "
                            "de dashboards do Xbox 360: Blades, NXE, Kinect, Metro V1 e Metro V2.", 88, y, 1100, b, 30) + 14;
        y += drawWrapped(r, "Todos os gráficos, ícones, fontes e sons foram criados especificamente para o Neutro X, por código. "
                            "Nenhum recurso, código ou arquivo proprietário dos dashboards originais é usado ou redistribuído.",
                         88, y, 1100, b, 30) + 14;
        y += drawWrapped(r, "Segurança: o Neutro X não grava na NAND, não altera arquivos do sistema nem a configuração do "
                            "console. Ele salva apenas o próprio arquivo de configurações e pode ser removido apagando sua pasta.",
                         88, y, 1100, b, 30) + 14;
        TextStyle small = b;
        small.size = 17;
        small.color = Color(130, 140, 152);
        drawWrapped(r, "Xbox, Xbox 360, Xbox LIVE e Kinect são marcas da Microsoft Corporation. O Neutro X é um projeto "
                       "de fã sem afiliação ou endosso da Microsoft.", 88, y + 10, 1100, small, 24);
        TextStyle h;
        h.size = 20;
        h.weight = FontWeight::Bold;
        h.color = Color(255, 255, 255);
        r.drawText("Controles", 88, 548, h);
        drawHints(r, {{Button::A, "Confirmar"}, {Button::B, "Voltar"}, {Button::X, "Ação contextual"}, {Button::Y, "Opções"},
                      {Button::LB, "/"}, {Button::RB, "Categorias"}, {Button::Start, "Menu Neutro X"}},
                  88, 600, 30, Color(205, 212, 220));
        r.popOffset();
        r.fillRect(Rectf{0, 668, 1280, 52}, Color(0, 0, 0, 90));
        drawHints(r, {{Button::B, "Voltar"}}, 56, 694, 30, Color(205, 212, 220));
        r.popAlpha();
    }

private:
    NeutroBackdrop backdrop_;
    Tween enter_;
};

// ----------------------------------------------------- Menu global (START)

class GlobalMenuScreen : public Screen {
public:
    const char* name() const override { return "GlobalMenu"; }
    bool isOverlay() const override { return true; }
    void onEnter(Context& ctx) override {
        open_.start(0.3f);
        cur_ = ctx.app->hasActiveDashboard() ? int(ctx.app->activeDashboard()) : 0;
    }
    void update(Context& ctx, const InputFrame& in, float dt) override {
        open_.update(dt);
        hl_.update(dt);
        int n = switching_ ? kDashboardCount : 5;
        int before = sel_;
        if (in.press(Button::Down)) sel_ = (sel_ + 1) % n;
        if (in.press(Button::Up)) sel_ = (sel_ + n - 1) % n;
        if (before != sel_) ctx.sfx(Sfx::Navigate);
        hl_.set(float(sel_));
        if (in.press(Button::B) || in.press(Button::Start)) {
            ctx.sfx(Sfx::Back);
            if (switching_) { switching_ = false; sel_ = 1; hl_.snap(1); }
            else ctx.app->pop();
            return;
        }
        if (!in.press(Button::A)) return;
        if (switching_) {
            if (sel_ == cur_ && ctx.app->hasActiveDashboard()) { ctx.sfx(Sfx::Back); ctx.app->pop(); return; }
            ctx.sfx(Sfx::Confirm);
            ctx.app->openDashboard(DashboardId(sel_));  // descarrega a atual e carrega a nova
            return;
        }
        switch (sel_) {
            case 0: ctx.sfx(Sfx::Back); ctx.app->pop(); break;
            case 1: ctx.sfx(Sfx::Open); switching_ = true; sel_ = cur_; hl_.snap(float(sel_)); break;
            case 2: ctx.sfx(Sfx::Open); ctx.app->pop(); ctx.app->openSettings(); break;
            case 3: ctx.sfx(Sfx::Confirm); ctx.app->openMainMenu(); break;
            case 4:
                ctx.sfx(Sfx::Open);
                ctx.app->push(createConfirmDialog("Sair do Neutro X?", "O aplicativo será encerrado.",
                                                  [](Context& c) { c.app->requestQuit(); }));
                break;
        }
    }
    void render(Context& ctx, IRenderer& r) override {
        float e = ease::outCubic(open_.progress());
        const Color accent = ctx.accent();
        r.fillRect(Rectf{0, 0, 1280, 720}, Color(0, 0, 0, uint8_t(150 * e)));
        float w = 460;
        Rectf panel{1280 - w * e, 0, w, 720};
        r.fillRect(panel, Color(14, 18, 24));
        r.fillRect(Rectf{panel.x, 0, 4, 720}, accent);
        r.pushOffset(panel.x - (1280 - w), 0);
        float px = 1280 - w + 40;
        drawNeutroLogo(r, px, 40, 34, accent);
        TextStyle sub;
        sub.size = 17;
        sub.color = Color(140, 150, 162);
        r.drawText(switching_ ? "Trocar de dashboard" : "Menu do Neutro X", px, 88, sub);
        float y0 = 140;
        float itemH = switching_ ? 86 : 62;
        Rectf hl{px - 14, y0 + hl_.value * itemH, w - 52, itemH - 8};
        r.fillRoundRect(hl, 8, accent.withAlpha(210));
        if (!switching_) {
            static const char* const items[] = {"Continuar", "Trocar de dashboard", "Configurações", "Menu principal",
                                                "Sair do Neutro X"};
            static const Icon icons[] = {Icon::ArrowLeft, Icon::Apps, Icon::Gear, Icon::Home, Icon::Power};
            for (int i = 0; i < 5; ++i) {
                float y = y0 + i * itemH;
                Color c = i == sel_ ? Color(255, 255, 255) : Color(196, 204, 214);
                drawIcon(r, icons[i], px + 16, y + 27, 26, c, i == sel_ ? accent : Color(14, 18, 24));
                TextStyle ts;
                ts.size = 25;
                ts.color = c;
                ts.weight = i == sel_ ? FontWeight::Bold : FontWeight::Regular;
                r.drawText(items[i], px + 52, y + 16, ts);
            }
        } else {
            for (int i = 0; i < kDashboardCount; ++i) {
                const DashboardInfo& info = dashboardInfo(DashboardId(i));
                float y = y0 + i * itemH;
                Rectf th{px, y + 6, 116, 65};
                r.drawTexture(ctx.app->previewTexture(info.id, true), th);
                r.strokeRect(th, 1.5f, Color(255, 255, 255, i == sel_ ? 255 : 70));
                TextStyle ts;
                ts.size = 25;
                ts.color = i == sel_ ? Color(255, 255, 255) : Color(196, 204, 214);
                ts.weight = i == sel_ ? FontWeight::Bold : FontWeight::Regular;
                r.drawText(info.name, px + 132, y + 14, ts);
                TextStyle vs;
                vs.size = 15;
                vs.color = ts.color.fade(0.7f);
                std::string sub2 = std::string(info.version) + (i == cur_ && ctx.app->hasActiveDashboard() ? "  ·  em uso" : "");
                r.drawText(sub2.c_str(), px + 133, y + 48, vs);
            }
        }
        drawHints(r, {{Button::A, "Selecionar"}, {Button::B, "Voltar"}}, px, 686, 28, Color(205, 212, 220));
        r.popOffset();
    }

private:
    Tween open_;
    Smooth hl_{0, 20};
    int sel_ = 0, cur_ = 0;
    bool switching_ = false;
};

// ------------------------------------------------------------- Dialogo

class ConfirmDialog : public Screen {
public:
    ConfirmDialog(std::string title, std::string msg, std::function<void(Context&)> onYes)
        : title_(std::move(title)), msg_(std::move(msg)), onYes_(std::move(onYes)) {}
    const char* name() const override { return "Dialog"; }
    bool isOverlay() const override { return true; }
    void onEnter(Context&) override { open_.start(0.22f); }
    void update(Context& ctx, const InputFrame& in, float dt) override {
        open_.update(dt);
        if (in.press(Button::Left) || in.press(Button::Right)) { yes_ = !yes_; ctx.sfx(Sfx::Navigate); }
        if (in.press(Button::B)) { ctx.sfx(Sfx::Back); ctx.app->pop(); return; }
        if (in.press(Button::A)) {
            if (yes_) {
                ctx.sfx(Sfx::Confirm);
                ctx.app->pop();
                if (onYes_) onYes_(ctx);
            } else {
                ctx.sfx(Sfx::Back);
                ctx.app->pop();
            }
        }
    }
    void render(Context& ctx, IRenderer& r) override {
        float e = ease::outBack(open_.progress());
        r.fillRect(Rectf{0, 0, 1280, 720}, Color(0, 0, 0, uint8_t(160 * clampf(open_.progress() * 2, 0, 1))));
        Rectf box = Rectf{340, 230, 600, 260}.scaledAboutCenter(0.9f + 0.1f * e);
        r.pushAlpha(clampf(open_.progress() * 2, 0, 1));
        r.fillRoundRect(box.offset(0, 10), 16, Color(0, 0, 0, 120));
        r.fillRoundRect(box, 16, Color(24, 30, 38));
        r.strokeRoundRect(box, 16, 2, ctx.accent());
        TextStyle t;
        t.size = 32;
        t.weight = FontWeight::Bold;
        t.color = Color(255, 255, 255);
        r.drawText(title_.c_str(), box.x + 36, box.y + 30, t);
        TextStyle m;
        m.size = 20;
        m.color = Color(190, 198, 208);
        drawWrapped(r, msg_.c_str(), box.x + 36, box.y + 84, box.w - 72, m, 28, 3);
        const char* opts[2] = {"Sim", "Não"};
        for (int i = 0; i < 2; ++i) {
            bool sel = (i == 0) == yes_;
            Rectf b{box.x + 36 + i * 180.0f, box.bottom() - 74, 160, 46};
            r.fillRoundRect(b, 10, sel ? ctx.accent() : Color(50, 58, 68));
            TextStyle bs;
            bs.size = 22;
            bs.align = Align::Center;
            bs.weight = sel ? FontWeight::Bold : FontWeight::Regular;
            bs.color = Color(255, 255, 255);
            r.drawText(opts[i], b.cx(), textTopForCenter(b.cy(), 22), bs);
        }
        r.popAlpha();
    }

private:
    std::string title_, msg_;
    std::function<void(Context&)> onYes_;
    Tween open_;
    bool yes_ = false;  // padrao seguro: "Nao"
};

// Corta o texto (por codepoint UTF-8) para caber em maxW, terminando com reticencias.
std::string fitText(IRenderer& r, const std::string& s, const TextStyle& st, float maxW) {
    if (r.measureText(s.c_str(), st) <= maxW) return s;
    std::string cut = s;
    const std::string ell = "\xE2\x80\xA6";  // reticencias
    while (!cut.empty()) {
        size_t n = cut.size() - 1;
        while (n > 0 && (static_cast<unsigned char>(cut[n]) & 0xC0) == 0x80) --n;  // inicio do codepoint
        cut.erase(n);
        if (r.measureText((cut + ell).c_str(), st) <= maxW) return cut + ell;
    }
    return ell;
}
// ------------------------------------------- Estado dos sons originais

class SoundStatusScreen : public Screen {
public:
    const char* name() const override { return "SoundStatus"; }
    void onEnter(Context& ctx) override {
        enter_.start(0.4f);
        backdrop_.ensure(ctx);
        rep_ = ctx.audio->originalsReport();
    }
    void onExit(Context& ctx) override { backdrop_.release(ctx); }
    void update(Context& ctx, const InputFrame& in, float dt) override {
        enter_.update(dt);
        int before = sel_;
        if (in.press(Button::Down)) sel_ = (sel_ + 1) % kDashboardCount;
        if (in.press(Button::Up)) sel_ = (sel_ + kDashboardCount - 1) % kDashboardCount;
        if (before != sel_) ctx.sfx(Sfx::Navigate);
        if (in.press(Button::Y)) {
            ctx.app->reloadOriginalSounds();
            rep_ = ctx.audio->originalsReport();
            ctx.sfx(Sfx::Confirm);
            ctx.app->toast("Pasta de sons originais verificada novamente");
        }
        if (in.press(Button::A)) {
            // Testa um efeito do tema selecionado com a fonte atual.
            SoundTheme t = soundThemeFor(DashboardId(sel_));
            ctx.audio->setTheme(t);
            ctx.audio->play(Sfx(preview_ % kSfxCount), 1.0f, 1.0f);
            ctx.audio->setTheme(SoundTheme::NeutroX);
            preview_++;
        }
        if (in.press(Button::B)) { ctx.sfx(Sfx::Back); ctx.app->pop(); }
    }
    void render(Context& ctx, IRenderer& r) override {
        backdrop_.draw(ctx, r);
        const Color accent = ctx.accent();
        r.pushAlpha(ease::outCubic(enter_.progress()));
        drawIcon(r, Icon::Speaker, 82, 80, 44, accent);
        TextStyle t;
        t.size = 40;
        t.weight = FontWeight::Bold;
        t.color = Color(255, 255, 255);
        r.drawText("Sons originais dos dashboards", 124, 56, t);
        TextStyle note;
        note.size = 16;
        note.color = Color(150, 160, 172);
        std::string p = "Pasta local (fora do repositório): " + rep_.baseDir;
        r.drawText(p.c_str(), 124, 104, note);
        const char* src = ctx.settings->dashboardSounds == 0
                              ? "Fonte atual: ORIGINAIS. Efeitos ausentes ficam mudos (sem substituição automática)."
                              : "Fonte atual: RECRIADOS pelo Neutro X (escolhida em Configurações).";
        r.drawText(src, 124, 126, note);
        int totalFiles = 0;
        for (int ti = 1; ti < kSoundThemeCount; ++ti) totalFiles += rep_.filesInFolder[ti];
        TextStyle tf;
        tf.size = 18;
        tf.weight = FontWeight::Bold;
        tf.color = totalFiles ? Color(140, 230, 120) : Color(240, 160, 120);
        std::string tot = "Arquivos originais encontrados: " + std::to_string(totalFiles) + "  ·  efeitos com função: " +
                          std::to_string(rep_.totalLoaded()) + " de " + std::to_string((kSoundThemeCount - 1) * kSfxCount);
        r.drawText(tot.c_str(), 124, 144, tf);

        // Lista de versoes
        for (int i = 0; i < kDashboardCount; ++i) {
            const DashboardInfo& info = dashboardInfo(DashboardId(i));
            SoundTheme th = soundThemeFor(info.id);
            int n = rep_.loaded(th);
            Rectf row{60, 168.0f + i * 62, 380, 54};
            bool sel = i == sel_;
            if (sel) r.fillRoundRect(row, 8, accent.withAlpha(210));
            TextStyle ns;
            ns.size = 24;
            ns.weight = sel ? FontWeight::Bold : FontWeight::Regular;
            ns.color = Color(255, 255, 255);
            r.drawText(info.name, 80, row.y + 8, ns);
            TextStyle vs;
            vs.size = 15;
            vs.color = sel ? Color(235, 240, 245) : Color(150, 160, 172);
            std::string sub = std::string(soundThemeInfo(th).folder) + "  ·  " + std::to_string(rep_.filesInFolder[int(th)]) +
                              (rep_.filesInFolder[int(th)] == 1 ? " arquivo" : " arquivos");
            r.drawText(sub.c_str(), 81, row.y + 34, vs);
            TextStyle cs;
            cs.size = 22;
            cs.align = Align::Right;
            cs.color = n == kSfxCount ? Color(140, 230, 120) : (n == 0 ? Color(240, 120, 120) : Color(240, 200, 100));
            std::string cnt = std::to_string(n) + "/" + std::to_string(kSfxCount);
            r.drawText(cnt.c_str(), row.right() - 16, row.y + 14, cs);
        }
        // Detalhe por efeito
        Rectf panel{468, 168, 756, 486};
        r.fillRoundRect(panel, 12, Color(14, 18, 24, 220));
        SoundTheme th = soundThemeFor(DashboardId(sel_));
        for (int s = 0; s < kSfxCount; ++s) {
            const OrigEntry& e = rep_.e[int(th)][s];
            float y = panel.y + 16 + s * 58.0f;
            Color dot = e.status == OrigStatus::Loaded ? Color(110, 210, 90)
                        : e.status == OrigStatus::Missing ? Color(200, 90, 90) : Color(230, 170, 60);
            r.fillCircle(panel.x + 26, y + 14, 7, dot);
            TextStyle fs;
            fs.size = 20;
            fs.weight = FontWeight::Bold;
            fs.color = Color(235, 240, 245);
            // efeito -> arquivo original real (pode vir do mapeamento.ini)
            std::string label = sfxFileName(Sfx(s));
            if (e.status == OrigStatus::Loaded) {
                size_t slash = e.file.find_last_of("/\\");
                label += "  \xE2\x86\x92  " + (slash == std::string::npos ? e.file : e.file.substr(slash + 1));
            }
            r.drawText(label.c_str(), panel.x + 44, y + 2, fs);
            TextStyle ds;
            ds.size = 15;
            ds.color = Color(160, 170, 182);
            std::string det;
            if (e.status == OrigStatus::Loaded) {
                char b[48];
                std::snprintf(b, sizeof(b), "carregado (%.2f s)", e.seconds);
                det = b;
            } else {
                det = e.reason;
            }
            det = fitText(r, det, ds, panel.w - 64);
            r.drawText(det.c_str(), panel.x + 44, y + 28, ds);
        }
        r.fillRect(Rectf{0, 668, 1280, 52}, Color(0, 0, 0, 90));
        drawHints(r, {{Button::A, "Testar efeito"}, {Button::Y, "Verificar pasta de novo"}, {Button::B, "Voltar"}}, 56, 694, 30,
                  Color(205, 212, 220));
        r.popAlpha();
    }

private:
    NeutroBackdrop backdrop_;
    OrigReport rep_;
    Tween enter_;
    int sel_ = 0, preview_ = 0;
};

// ----------------------------------------------------------------- Erro

class ErrorScreen : public Screen {
public:
    ErrorScreen(std::string title, std::string msg) : title_(std::move(title)), msg_(std::move(msg)) {}
    const char* name() const override { return "Error"; }
    void update(Context& ctx, const InputFrame& in, float) override {
        if (!leaving_ && (in.press(Button::B) || in.press(Button::A))) {
            leaving_ = true;
            ctx.sfx(Sfx::Back);
            ctx.app->openMainMenu();
        }
    }
    void render(Context&, IRenderer& r) override {
        r.fillGradientV(Rectf{0, 0, 1280, 720}, Color::hex(0x1A0D10), Color::hex(0x0A0A0C));
        r.fillCircle(640, 250, 54, Color::hex(0xD8323C));
        TextStyle ex;
        ex.size = 80;
        ex.weight = FontWeight::Bold;
        ex.align = Align::Center;
        r.drawText("!", 640, 210, ex);
        TextStyle t;
        t.size = 36;
        t.weight = FontWeight::Bold;
        t.align = Align::Center;
        t.color = Color(255, 255, 255);
        r.drawText(title_.c_str(), 640, 340, t);
        TextStyle m;
        m.size = 21;
        m.align = Align::Center;
        m.color = Color(200, 190, 194);
        r.drawText(msg_.c_str(), 640, 396, m);
        m.size = 17;
        m.color = Color(140, 130, 134);
        r.drawText("O Neutro X continua funcionando. Nenhum dado do sistema foi afetado.", 640, 440, m);
        drawHints(r, {{Button::B, "Voltar ao menu"}}, 640, 560, 32, Color(220, 210, 214), Align::Center);
    }

private:
    std::string title_, msg_;
    bool leaving_ = false;
};

}  // namespace

std::unique_ptr<Screen> createBootScreen() { return std::unique_ptr<Screen>(new BootScreen()); }
std::unique_ptr<Screen> createSettingsScreen() { return std::unique_ptr<Screen>(new SettingsScreen()); }
std::unique_ptr<Screen> createAboutScreen() { return std::unique_ptr<Screen>(new AboutScreen()); }
std::unique_ptr<Screen> createGlobalMenuScreen() { return std::unique_ptr<Screen>(new GlobalMenuScreen()); }
std::unique_ptr<Screen> createSoundStatusScreen() { return std::unique_ptr<Screen>(new SoundStatusScreen()); }
std::unique_ptr<Screen> createErrorScreen(const std::string& title, const std::string& message) {
    return std::unique_ptr<Screen>(new ErrorScreen(title, message));
}
std::unique_ptr<Screen> createConfirmDialog(const std::string& title, const std::string& message,
                                            std::function<void(Context&)> onYes) {
    return std::unique_ptr<Screen>(new ConfirmDialog(title, message, std::move(onYes)));
}

}  // namespace nx
