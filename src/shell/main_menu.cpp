// Neutro X - menu inicial: selecao das cinco interfaces, configuracoes, sobre e sair.
#include <string>
#include "core/anim.h"
#include "shell/screens.h"
#include "shell/shell_common.h"

namespace nx {

namespace {

enum class ItemKind { Dashboard, Settings, About, Exit };

struct MenuItem {
    ItemKind kind;
    DashboardId dash;
};

const MenuItem kItems[] = {
    {ItemKind::Dashboard, DashboardId::Blades}, {ItemKind::Dashboard, DashboardId::Nxe},
    {ItemKind::Dashboard, DashboardId::Kinect}, {ItemKind::Dashboard, DashboardId::MetroV1},
    {ItemKind::Dashboard, DashboardId::MetroV2}, {ItemKind::Settings, DashboardId::Count},
    {ItemKind::About, DashboardId::Count},       {ItemKind::Exit, DashboardId::Count},
};
constexpr int kItemCount = int(sizeof(kItems) / sizeof(kItems[0]));

constexpr float kListX = 52, kListY = 150, kListW = 500;
constexpr float kDashH = 66, kOtherH = 46, kGap = 6;

float itemY(int i) {
    float y = kListY;
    for (int k = 0; k < i; ++k) y += (kItems[k].kind == ItemKind::Dashboard ? kDashH : kOtherH) + kGap;
    return y;
}
float itemH(int i) { return kItems[i].kind == ItemKind::Dashboard ? kDashH : kOtherH; }

class MainMenuScreen : public Screen {
public:
    const char* name() const override { return "MainMenu"; }

    void onEnter(Context& ctx) override {
        enter_.start(0.7f);
        // Restaura a ultima selecao (ou o dashboard ativo/padrao).
        const DashboardInfo* def = findDashboard(ctx.settings->defaultDashboard);
        if (def) sel_ = int(def->id);
        hlY_.snap(itemY(sel_));
        hlH_.snap(itemH(sel_));
        prevSel_ = sel_;
        swap_.t = 1;
        backdrop_.ensure(ctx);
    }

    void onExit(Context& ctx) override { backdrop_.release(ctx); }

    void update(Context& ctx, const InputFrame& in, float dt) override {
        enter_.update(dt);
        swap_.update(dt);
        hlY_.update(dt);
        hlH_.update(dt);
        int before = sel_;
        if (in.press(Button::Down)) sel_ = (sel_ + 1) % kItemCount;
        if (in.press(Button::Up)) sel_ = (sel_ + kItemCount - 1) % kItemCount;
        if (in.press(Button::RB)) sel_ = sel_ < 5 ? 5 : 0;  // salta entre grupos
        if (in.press(Button::LB)) sel_ = sel_ >= 5 ? 0 : 5;
        if (sel_ != before) {
            ctx.sfx(Sfx::Navigate);
            prevSel_ = before;
            swap_.start(0.28f);
            hlY_.set(itemY(sel_));
            hlH_.set(itemH(sel_));
        }
        if (in.press(Button::A)) activate(ctx, sel_);
        if (in.press(Button::Y)) { ctx.sfx(Sfx::Open); ctx.app->openSettings(); }
        if (in.press(Button::B)) activate(ctx, kItemCount - 1);  // B = sair (com confirmacao)
        if (in.press(Button::X) && kItems[sel_].kind == ItemKind::Dashboard) {
            // Acao contextual: definir como dashboard padrao.
            const DashboardInfo& info = dashboardInfo(kItems[sel_].dash);
            Settings& s = *ctx.settings;
            if (s.defaultDashboard == info.key) {
                s.defaultDashboard.clear();
                ctx.app->toast("O Neutro X abrirá no menu principal");
            } else {
                s.defaultDashboard = info.key;
                ctx.app->toast(std::string(info.name) + " definido como dashboard padrão");
            }
            ctx.sfx(Sfx::Confirm, 0.8f);
            ctx.app->saveSettings();
        }
    }

    void activate(Context& ctx, int i) {
        switch (kItems[i].kind) {
            case ItemKind::Dashboard:
                ctx.sfx(Sfx::Confirm);
                ctx.app->openDashboard(kItems[i].dash);
                break;
            case ItemKind::Settings:
                ctx.sfx(Sfx::Open);
                ctx.app->openSettings();
                break;
            case ItemKind::About:
                ctx.sfx(Sfx::Open);
                ctx.app->openAbout();
                break;
            case ItemKind::Exit:
                ctx.sfx(Sfx::Open);
                ctx.app->push(createConfirmDialog("Sair do Neutro X?",
                                                  "O aplicativo será encerrado. Nenhum arquivo do sistema foi alterado.",
                                                  [](Context& c) { c.app->requestQuit(); }));
                break;
        }
    }

    void render(Context& ctx, IRenderer& r) override {
        backdrop_.draw(ctx, r);
        const Color accent = ctx.accent();
        const float e = enter_.progress();

        drawNeutroLogo(r, 56, 36, 50, accent, clampf(e * 1.4f, 0, 1));
        TextStyle sub;
        sub.size = 19;
        sub.tracking = 1.2f;
        sub.color = Color(150, 160, 172);
        r.pushAlpha(clampf(e * 2 - 0.4f, 0, 1));
        r.drawText("Xbox 360 Dashboard Experience", 60, 98, sub);
        r.popAlpha();
        drawClock(ctx, r, 1224, 40, Color(220, 226, 234));

        renderList(ctx, r, e);
        renderPanel(ctx, r, e);

        r.fillRect(Rectf{0, 668, 1280, 52}, Color(0, 0, 0, 90));
        bool isDash = kItems[sel_].kind == ItemKind::Dashboard;
        if (isDash)
            drawHints(r, {{Button::A, "Abrir"}, {Button::X, "Definir padrão"}, {Button::Y, "Configurações"}, {Button::B, "Sair"}},
                      56, 694, 30, Color(205, 212, 220));
        else
            drawHints(r, {{Button::A, "Selecionar"}, {Button::Y, "Configurações"}, {Button::B, "Sair"}}, 56, 694, 30,
                      Color(205, 212, 220));
        TextStyle ver;
        ver.size = 16;
        ver.align = Align::Right;
        ver.color = Color(120, 128, 140);
        std::string v = std::string("versão ") + kNeutroXVersion;
        r.drawText(v.c_str(), 1224, 688, ver);
    }

private:
    void renderList(Context& ctx, IRenderer& r, float e) {
        const Color accent = ctx.accent();
        // Destaque deslizante
        float hy = hlY_.value, hh = hlH_.value;
        Rectf hl{kListX - 6, hy - 3, kListW + 12, hh + 6};
        r.pushAlpha(clampf(e * 2, 0, 1));
        r.fillRoundRect(hl.offset(0, 4), 10, Color(0, 0, 0, 70));
        r.fillRoundRect(hl, 10, brighten(accent, -0.35f));
        r.pushClip(hl.inset(1));
        r.fillGradientH(hl, accent.withAlpha(230), accent.withAlpha(60));
        float shine = std::fmod(float(ctx.time) * 0.35f, 1.6f);
        Vec2 sh[4] = {{hl.x + (shine - 0.3f) * hl.w, hl.y}, {hl.x + (shine - 0.15f) * hl.w, hl.y},
                      {hl.x + (shine - 0.25f) * hl.w, hl.bottom()}, {hl.x + (shine - 0.4f) * hl.w, hl.bottom()}};
        r.fillConvex(sh, 4, Color(255, 255, 255, 28));
        r.popClip();
        r.fillRoundRect(Rectf{hl.x, hl.y + 8, 5, hl.h - 16}, 2.5f, Color(255, 255, 255, 230));
        r.popAlpha();

        for (int i = 0; i < kItemCount; ++i) {
            float local = clampf((e - 0.05f * i) / 0.5f, 0, 1);
            float k = ease::outCubic(local);
            float y = itemY(i), h = itemH(i);
            bool sel = i == sel_;
            r.pushAlpha(k);
            r.pushOffset((1 - k) * -50, 0);
            Color tc = sel ? Color(255, 255, 255) : Color(196, 204, 214);
            if (kItems[i].kind == ItemKind::Dashboard) {
                const DashboardInfo& info = dashboardInfo(kItems[i].dash);
                Rectf th{kListX + 8, y + 7, 92, 52};
                r.fillRect(th.offset(2, 3), Color(0, 0, 0, 80));
                r.drawTexture(ctx.app->previewTexture(info.id, true), th);
                r.strokeRect(th, sel ? 2.0f : 1.0f, sel ? Color(255, 255, 255) : Color(255, 255, 255, 60));
                TextStyle ns;
                ns.size = 27;
                ns.weight = sel ? FontWeight::Bold : FontWeight::Regular;
                ns.color = tc;
                r.drawText(info.name, kListX + 116, y + 8, ns);
                TextStyle vs;
                vs.size = 16;
                vs.color = sel ? Color(235, 240, 245) : Color(130, 140, 152);
                std::string line = std::string(info.version) + "  ·  " + info.year;
                r.drawText(line.c_str(), kListX + 117, y + 41, vs);
                if (ctx.settings->defaultDashboard == info.key) {
                    TextStyle bs;
                    bs.size = 13;
                    bs.weight = FontWeight::Bold;
                    bs.tracking = 1.5f;
                    bs.color = Color(20, 24, 28);
                    float bw = r.measureText("PADRÃO", bs) + 16;
                    Rectf badge{kListX + kListW - bw - 10, y + 10, bw, 22};
                    r.fillRoundRect(badge, 11, Color(255, 255, 255, 220));
                    r.drawText("PADRÃO", badge.x + 8, badge.y + 5, bs);
                }
            } else {
                Icon icon = kItems[i].kind == ItemKind::Settings ? Icon::Gear
                            : kItems[i].kind == ItemKind::About ? Icon::Info : Icon::Power;
                const char* label = kItems[i].kind == ItemKind::Settings ? "Configurações"
                                    : kItems[i].kind == ItemKind::About ? "Sobre o Neutro X" : "Sair";
                drawIcon(r, icon, kListX + 54, y + h * 0.5f, 26, tc, sel ? brighten(accent, -0.1f) : Color::hex(0x121921));
                TextStyle ns;
                ns.size = 25;
                ns.weight = sel ? FontWeight::Bold : FontWeight::Regular;
                ns.color = tc;
                r.drawText(label, kListX + 116, textTopForCenter(y + h * 0.5f, 25), ns);
            }
            r.popOffset();
            r.popAlpha();
        }
    }

    void renderPanel(Context& ctx, IRenderer& r, float e) {
        float k = ease::outCubic(clampf((e - 0.15f) / 0.6f, 0, 1));
        r.pushAlpha(k);
        r.pushOffset((1 - k) * 60, 0);
        float s = ease::outCubic(swap_.progress());
        // Conteudo anterior saindo e novo entrando (crossfade com leve deslize).
        if (s < 1 && prevSel_ != sel_) {
            r.pushAlpha(1 - s);
            r.pushOffset(-20 * s, 0);
            panelFor(ctx, r, prevSel_);
            r.popOffset();
            r.popAlpha();
        }
        r.pushAlpha(s);
        r.pushOffset(20 * (1 - s), 0);
        panelFor(ctx, r, sel_);
        r.popOffset();
        r.popAlpha();
        r.popOffset();
        r.popAlpha();
    }

    void panelFor(Context& ctx, IRenderer& r, int i) {
        const Rectf pv{600, 150, 624, 351};
        const Color accent = ctx.accent();
        TextStyle title;
        title.size = 38;
        title.weight = FontWeight::Bold;
        title.color = Color(255, 255, 255);
        TextStyle body;
        body.size = 20;
        body.color = Color(178, 188, 200);

        if (kItems[i].kind == ItemKind::Dashboard) {
            const DashboardInfo& info = dashboardInfo(kItems[i].dash);
            r.fillRoundRect(pv.inset(-6).offset(0, 8), 12, Color(0, 0, 0, 90));
            r.fillRoundRect(pv.inset(-6), 12, Color(255, 255, 255, 30));
            r.drawTexture(ctx.app->previewTexture(info.id, false), pv);
            r.strokeRect(pv, 1.5f, Color(255, 255, 255, 90));
            // reflexo
            r.fillGradientV(Rectf{pv.x, pv.y, pv.w, 60}, Color(255, 255, 255, 34), Color(255, 255, 255, 0));
            r.drawText(info.name, 604, 520, title);
            TextStyle tg;
            tg.size = 20;
            tg.color = accent;
            tg.weight = FontWeight::Bold;
            float tw = r.measureText(info.name, title);
            r.drawText(info.tagline, 604 + tw + 18, 536, tg);
            drawWrapped(r, info.description, 604, 572, 620, body, 28, 2);
            TextStyle meta;
            meta.size = 16;
            meta.color = Color(130, 140, 152);
            std::string m = std::string("Referência histórica: versão ") + info.version + " (" + info.year +
                            ")  ·  recriação original do Neutro X";
            r.drawText(m.c_str(), 604, 634, meta);
            return;
        }
        Rectf card{600, 150, 624, 351};
        r.fillRoundRect(card.offset(0, 8), 14, Color(0, 0, 0, 80));
        r.fillRoundRect(card, 14, Color(22, 30, 40, 235));
        r.strokeRoundRect(card, 14, 1.5f, accent.withAlpha(120));
        if (kItems[i].kind == ItemKind::Settings) {
            drawIcon(r, Icon::Gear, 720, 270, 130, accent, Color(22, 30, 40));
            const Settings& s = *ctx.settings;
            const DashboardInfo* def = findDashboard(s.defaultDashboard);
            std::string lines[4] = {
                std::string("Dashboard padrão: ") + (def ? def->name : "Menu do Neutro X"),
                "Volume dos efeitos: " + std::to_string(s.sfxVolume) + "%",
                std::string("Animações: ") + (s.animations ? "ativadas" : "desativadas") + " · intensidade " +
                    (s.animIntensity == AnimIntensity::Low ? "baixa" : s.animIntensity == AnimIntensity::High ? "alta" : "média"),
                std::string("Tema de cores: ") + kAccentThemes[s.accentTheme].name,
            };
            TextStyle ls = body;
            ls.color = Color(214, 220, 228);
            for (int k = 0; k < 4; ++k) r.drawText(lines[k].c_str(), 820, 196 + k * 40.0f, ls);
            r.drawText("Configurações", 604, 520, title);
            drawWrapped(r, "Ajuste dashboard padrão, volume, animações e cores. As preferências ficam apenas no arquivo próprio do Neutro X.",
                        604, 572, 620, body, 28, 2);
        } else if (kItems[i].kind == ItemKind::About) {
            drawNeutroLogo(r, 680, 250, 64, accent);
            TextStyle v = body;
            v.align = Align::Center;
            std::string ver = std::string("Versão ") + kNeutroXVersion + "  ·  aplicativo independente";
            r.drawText(ver.c_str(), 912, 350, v);
            r.drawText("Sobre o Neutro X", 604, 520, title);
            drawWrapped(r, "Recriações originais inspiradas em cinco gerações de dashboards. Não substitui nem altera o sistema do console.",
                        604, 572, 620, body, 28, 2);
        } else {
            drawIcon(r, Icon::Power, 912, 300, 140, Color::hex(0xD8323C));
            r.drawText("Sair", 604, 520, title);
            drawWrapped(r, "Encerra o Neutro X e volta para o inicializador (Aurora no console). Basta remover a pasta do aplicativo para desinstalar.",
                        604, 572, 620, body, 28, 2);
        }
    }

    NeutroBackdrop backdrop_;
    int sel_ = 0, prevSel_ = 0;
    Smooth hlY_{0, 18}, hlH_{0, 18};
    Tween enter_, swap_;
};

}  // namespace

std::unique_ptr<Screen> createMainMenuScreen() { return std::unique_ptr<Screen>(new MainMenuScreen()); }

}  // namespace nx
