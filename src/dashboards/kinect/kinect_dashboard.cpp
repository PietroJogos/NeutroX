// Neutro X - Dashboard "Kinect" (inspirado na geracao 2010-2011, referencia 2.0.13604.0).
//
// Recriacao original: grade 4x2 de cartoes grandes sobre ondas roxas e verdes.
// As secoes (Inicio, Jogos, Aplicativos, Musica, Videos, Configuracoes) sao paginas
// lado a lado: LB/RB ou passar da borda com Esquerda/Direita desliza para a pagina
// vizinha. O cartao selecionado "salta" com moldura branca. A abre uma pagina
// simplificada de botoes grandes; X marca favorito; Y abre opcoes.
// Projetado para o CONTROLE: nenhum sensor Kinect e usado ou necessario.
// Toda a arte e desenhada por codigo. Nenhum recurso dos executaveis originais e usado.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <set>
#include <string>
#include <vector>
#include "core/anim.h"
#include "core/app.h"
#include "dashboards/common/demo_content.h"
#include "dashboards/dashboard.h"
#include "graphics/scene_cache.h"
#include "graphics/soft_renderer.h"
#include "graphics/ui.h"
#include "platform/platform.h"

namespace nx {

namespace {

// ------------------------------------------------------------ dados

enum class Kind { Generic, Game, Profile, OpenSettings, OpenAbout, Sensor };

struct KCard {
    Icon icon;
    std::string label;
    Color a, b;
    Kind kind;
    int index;
    bool art;
    bool lightCard;
};

struct KSection {
    const char* name;
    const char* title;  // titulo grande (maiusculas)
    std::vector<KCard> cards;
};

std::vector<KSection> buildSections() {
    std::vector<KSection> s(6);
    int n = 0;
    const DemoGame* g = demoGames(n);
    const Color green = Color::hex(0x8CC63A), greenD = Color::hex(0x5E9A1E);
    s[0] = {"Início", "INÍCIO", {}};
    s[0].cards = {
        {Icon::Controller, "Jogos", g[0].c1, g[0].c2, Kind::Generic, 1, true, false},
        {Icon::Music, "Música", Color::hex(0x111111), Color::hex(0x2A2A2A), Kind::Generic, 3, false, false},
        {Icon::Video, "Vídeos", Color::hex(0xDDEEF8), Color::hex(0x8FC0E0), Kind::Generic, 4, false, true},
        {Icon::Apps, "Aplicativos", Color::hex(0x3A9AD8), Color::hex(0x1B4F86), Kind::Generic, 2, true, false},
        {Icon::Person, "Perfil", green, greenD, Kind::Profile, 0, false, false},
        {Icon::Friends, "Amigos", Color::hex(0xA8D040), Color::hex(0x6EA020), Kind::Generic, 10, false, false},
        {Icon::Palette, "Avatar", Color::hex(0x7BBE3A), Color::hex(0x4F8E1C), Kind::Generic, 11, false, false},
        {Icon::Gear, "Configurações", Color::hex(0x2F64B0), Color::hex(0x1F4A8A), Kind::Generic, 5, false, false},
    };
    s[1] = {"Jogos", "JOGOS", {}};
    for (int i = 0; i < n && i < 8; ++i)
        s[1].cards.push_back({g[i].arcade ? Icon::Star : Icon::Controller, g[i].title, g[i].c1, g[i].c2, Kind::Game, i, true, false});
    s[2] = {"Aplicativos", "APLICATIVOS", {}};
    const char* apps[] = {"Relógio", "Clima", "Fotos", "Notas", "Calculadora", "Mapas"};
    const Icon appIcons[] = {Icon::Clock, Icon::Globe, Icon::Picture, Icon::List, Icon::Plus, Icon::Home};
    const unsigned appCol[] = {0x2E9BE0, 0xF0A020, 0xD9363E, 0x8A4FC8, 0x3A3F46, 0x2E9466};
    for (int i = 0; i < 6; ++i)
        s[2].cards.push_back({appIcons[i], apps[i], Color::hex(appCol[i]), brighten(Color::hex(appCol[i]), -0.35f), Kind::Generic, 20 + i, false, false});
    s[3] = {"Música", "MÚSICA", {}};
    int mn = 0;
    const DemoMedia* m = demoMusic(mn);
    for (int i = 0; i < mn; ++i)
        s[3].cards.push_back({Icon::Music, m[i].title, Color::hex(0xE0407A), Color::hex(0x5A1030), Kind::Generic, 30 + i, true, false});
    s[3].cards.push_back({Icon::Speaker, "Rádio", Color::hex(0x111111), Color::hex(0x2A2A2A), Kind::Generic, 36, false, false});
    s[3].cards.push_back({Icon::List, "Listas de reprodução", Color::hex(0xE0407A), Color::hex(0x8A1A48), Kind::Generic, 37, false, false});
    s[4] = {"Vídeos", "VÍDEOS", {}};
    int vn = 0;
    const DemoMedia* v = demoVideos(vn);
    for (int i = 0; i < vn; ++i)
        s[4].cards.push_back({Icon::Video, v[i].title, Color::hex(0x58B4E8), Color::hex(0x10305A), Kind::Generic, 40 + i, true, false});
    s[4].cards.push_back({Icon::Tv, "Filmes", Color::hex(0xDDEEF8), Color::hex(0x8FC0E0), Kind::Generic, 45, false, true});
    s[4].cards.push_back({Icon::Tv, "Séries", Color::hex(0x2F64B0), Color::hex(0x1F4A8A), Kind::Generic, 46, false, false});
    s[4].cards.push_back({Icon::Video, "Trailers", Color::hex(0x111111), Color::hex(0x2A2A2A), Kind::Generic, 47, false, false});
    s[5] = {"Configurações", "CONFIGURAÇÕES", {}};
    s[5].cards = {
        {Icon::Gear, "Sistema", Color::hex(0x2F64B0), Color::hex(0x1F4A8A), Kind::Generic, 50, false, false},
        {Icon::Person, "Perfil", green, greenD, Kind::Generic, 51, false, false},
        {Icon::Family, "Família", Color::hex(0xA8D040), Color::hex(0x6EA020), Kind::Generic, 52, false, false},
        {Icon::Sensor, "Sensor Kinect", Color::hex(0x3A3F46), Color::hex(0x1C1E22), Kind::Sensor, 53, false, false},
        {Icon::Palette, "Neutro X", Color::hex(0x7A4FB0), Color::hex(0x3A1F66), Kind::OpenSettings, 0, false, false},
        {Icon::Info, "Sobre", Color::hex(0x111111), Color::hex(0x2A2A2A), Kind::OpenAbout, 0, false, false},
    };
    return s;
}

const std::vector<KSection>& sections() {
    static const std::vector<KSection> s = buildSections();
    return s;
}

struct KTheme {
    const char* name;
    Color top, mid, floorTop, floorBottom, wave1, wave2, wave3, ink, accent;
};
const KTheme kThemes[] = {
    {"Roxo", Color::hex(0x2A1546), Color::hex(0x6A4C8E), Color::hex(0xDCD9E2), Color::hex(0xF4F3F6),
     Color::hex(0x3E8E2E), Color::hex(0x7CC242), Color::hex(0x512F7A), Color::hex(0x5A5466), Color::hex(0x7A4FB0)},
    {"Azul", Color::hex(0x0E2A4A), Color::hex(0x3A78B0), Color::hex(0xD8E0E8), Color::hex(0xF2F5F8),
     Color::hex(0x1E8AA0), Color::hex(0x6CD0E0), Color::hex(0x1F4A7A), Color::hex(0x4A5664), Color::hex(0x2E7BD0)},
    {"Grafite", Color::hex(0x15171A), Color::hex(0x3A3F46), Color::hex(0x2A2E33), Color::hex(0x1A1C1F),
     Color::hex(0x2E7A2E), Color::hex(0x6CB83A), Color::hex(0x2A2E36), Color::hex(0xB0B6BE), Color::hex(0x6CB83A)},
};
constexpr int kThemeCount = int(sizeof(kThemes) / sizeof(kThemes[0]));

constexpr int kCols = 4;
constexpr float kGridX = 148, kGridY = 156, kCardW = 236, kCardH = 180, kGapX = 14, kGapY = 16;

Rectf cardRect(int idx) {
    int col = idx % kCols, row = idx / kCols;
    return Rectf{kGridX + col * (kCardW + kGapX), kGridY + row * (kCardH + kGapY), kCardW, kCardH};
}

// ------------------------------------------------------------ paginas de detalhe

enum BtnAction { kBtnInfo = 0, kBtnClose = 1, kBtnPlay = 2 };

struct KDetail {
    std::string title, subtitle, desc;
    Icon icon = Icon::Info;
    int artSeed = 0;
    Color c1, c2;
    std::vector<InfoRow> buttons;
};

InfoRow btn(Icon ic, std::string t, std::string s = "", int action = kBtnInfo, float prog = -1) {
    InfoRow r;
    r.icon = int(ic);
    r.title = std::move(t);
    r.sub = std::move(s);
    r.action = action;
    r.progress = prog;
    return r;
}

KDetail buildDetail(const KCard& c, Context* ctx) {
    KDetail d;
    d.title = c.label;
    d.icon = c.icon;
    d.c1 = c.a;
    d.c2 = c.b;
    d.artSeed = c.art ? 13604 + c.index : 0;
    int n = 0;
    const DemoGame* g = demoGames(n);
    const DemoProfile& p = demoProfile();
    switch (c.kind) {
        case Kind::Game: {
            const DemoGame& gm = g[c.index];
            d.subtitle = std::string(gm.genre) + " · " + gm.year;
            d.desc = gm.description;
            d.artSeed = gm.artSeed;
            d.buttons = {btn(Icon::Controller, "Jogar", "Demonstração (não executável)", kBtnPlay),
                         btn(Icon::Trophy, "Conquistas", std::to_string(gm.points) + " de " + std::to_string(gm.maxPoints) + " pontos",
                             kBtnInfo, float(gm.points) / gm.maxPoints),
                         btn(Icon::Info, "Detalhes", d.subtitle)};
            break;
        }
        case Kind::Profile:
            d.subtitle = std::string(p.gamertag) + " · " + std::to_string(p.points) + " pontos";
            d.desc = "Perfil local de demonstração. Nenhuma conta online é usada.";
            d.buttons = {btn(Icon::Person, p.gamertag, "Nome do jogador"), btn(Icon::Mail, p.motto, "Lema"),
                         btn(Icon::Globe, p.zone, "Zona de jogo")};
            break;
        case Kind::Sensor:
            d.subtitle = "Não detectado";
            d.desc = "O Neutro X não usa o sensor Kinect. Toda a interface foi adaptada para o controle do Xbox 360.";
            d.buttons = {btn(Icon::Sensor, "Estado do sensor", "Não detectado / não utilizado"),
                         btn(Icon::Controller, "Navegação", "Controle do Xbox 360")};
            break;
        default:
            break;
    }
    if (d.buttons.empty()) {
        switch (c.index) {
            case 10: {
                int fn = 0;
                const DemoFriend* f = demoFriends(fn);
                d.desc = "Modo offline: amigos de demonstração.";
                for (int i = 0; i < fn; ++i) d.buttons.push_back(btn(Icon::Person, f[i].name, f[i].status));
                break;
            }
            case 11:
                d.desc = "Avatar genérico próprio do Neutro X (sem serviço online de avatares).";
                d.buttons = {btn(Icon::Palette, "Trocar roupa", "Demonstração"), btn(Icon::Person, "Ver avatar", "Demonstração")};
                break;
            case 20: {
                char b[32] = "12:00";
                if (ctx && ctx->platform) {
                    int h = 0, mi = 0, dd = 1, mo = 1, y = 2026;
                    ctx->platform->localTime(h, mi, dd, mo, y);
                    std::snprintf(b, sizeof(b), "%02d:%02d · %02d/%02d/%04d", h, mi, dd, mo, y);
                }
                d.subtitle = b;
                d.desc = "Relógio local do aparelho.";
                d.buttons = {btn(Icon::Clock, "Hora local", b)};
                break;
            }
            case 50: {
                char res[48] = "1280 × 720";
                if (ctx && ctx->renderer)
                    std::snprintf(res, sizeof(res), "%d × %d", ctx->renderer->pixelWidth(), ctx->renderer->pixelHeight());
                d.desc = "Somente visualização. As configurações do console não são lidas nem alteradas.";
                d.buttons = {btn(Icon::Tv, "Resolução do Neutro X", res),
                             btn(Icon::Speaker, "Volume dos efeitos",
                                 ctx && ctx->settings ? std::to_string(ctx->settings->sfxVolume) + "%" : "80%"),
                             btn(Icon::Globe, "Idioma", "Português (Brasil)"), btn(Icon::Network, "Rede", "Modo offline")};
                break;
            }
            case 51:
                d.desc = "Perfil local.";
                d.buttons = {btn(Icon::Person, p.gamertag, "Perfil local")};
                break;
            case 52:
                d.desc = "Representação visual.";
                d.buttons = {btn(Icon::Family, "Controle dos pais", "Não configurado (visual)"),
                             btn(Icon::Clock, "Temporizador de família", "Desativado (visual)")};
                break;
            default:
                if (c.index >= 30 && c.index < 40) {
                    int mn = 0;
                    const DemoMedia* m = demoMusic(mn);
                    d.desc = "Música de demonstração. Nenhum arquivo do sistema é lido.";
                    if (c.index - 30 < mn) d.subtitle = m[c.index - 30].detail;
                    d.buttons = {btn(Icon::Music, "Reproduzir", "Demonstração (sem áudio)"), btn(Icon::List, "Adicionar à lista", "Demonstração")};
                } else if (c.index >= 40 && c.index < 50) {
                    int vn = 0;
                    const DemoMedia* v = demoVideos(vn);
                    d.desc = "Vídeo de demonstração. Nenhum arquivo do sistema é lido.";
                    if (c.index - 40 < vn) d.subtitle = v[c.index - 40].detail;
                    d.buttons = {btn(Icon::Video, "Assistir", "Demonstração (sem vídeo)"), btn(Icon::Info, "Detalhes", "Demonstração")};
                } else {
                    d.desc = "Item de demonstração do Neutro X.";
                    d.buttons = {btn(c.icon, "Abrir", "Demonstração")};
                }
                break;
        }
    }
    d.buttons.push_back(btn(Icon::ArrowLeft, "Voltar", "", kBtnClose));
    return d;
}

// ------------------------------------------------------------ estado/visual

struct KState {
    int section = 0;
    float pagePos = 0;
    int sel[6] = {};
    float hlX = kGridX, hlY = kGridY;
    float pop = 1;
    float enter = 1;
    float time = 0;
    bool detailOpen = false;
    float detailAnim = 0;
    KDetail detail;
    int detailSel = 0;
    float detailSelAnim = 0;
    float detailScroll = 0;
    int theme = 0;
    bool showAvatar = true;
    std::set<int> favorites;  // secao*100 + indice
    bool optionsOpen = false;
    float optionsAnim = 0;
    int optionsSel = 0;
};

void waveBand(IRenderer& r, float yTop, float yBot, float amp, float freq, float phase, Color c) {
    const int n = 48;
    for (int i = 0; i < n; ++i) {
        float x0 = 1280.0f * i / n, x1 = 1280.0f * (i + 1) / n + 0.5f;
        auto fy = [&](float x, float base) { return base + amp * std::sin(x * freq + phase); };
        Vec2 q[4] = {{x0, fy(x0, yTop)}, {x1, fy(x1, yTop)}, {x1, fy(x1, yBot)}, {x0, fy(x0, yBot)}};
        r.fillConvex(q, 4, c);
    }
}

void drawBackground(IRenderer& r, int themeIdx) {
    const KTheme& t = kThemes[themeIdx];
    r.fillGradientV(Rectf{0, 0, 1280, 470}, t.top, t.mid);
    r.fillGradientV(Rectf{0, 470, 1280, 250}, t.floorTop, t.floorBottom);
    r.fillGradientV(Rectf{0, 380, 1280, 120}, t.mid.withAlpha(0), t.floorTop);
    waveBand(r, -40, 40, 26, 0.004f, 0.0f, t.wave1);
    waveBand(r, 10, 64, 30, 0.0035f, 1.3f, t.wave2.withAlpha(220));
    waveBand(r, 52, 92, 22, 0.005f, 2.4f, t.wave3.withAlpha(200));
}

void drawCard(IRenderer& r, const KCard& c, const Rectf& rc, bool fav, int sectionIdx) {
    r.fillRect(rc.offset(3, 5), Color(0, 0, 0, 50));
    if (c.art) drawAbstractArt(r, rc, 13604 + c.index + sectionIdx * 17, c.a, c.b);
    else r.fillGradientV(rc, c.a, c.b);
    Color fg = c.lightCard ? Color::hex(0x2B5F8A) : Color(255, 255, 255);
    float k = rc.w / kCardW;
    if (c.kind == Kind::Profile) {
        const DemoProfile& p = demoProfile();
        TextStyle ts;
        ts.size = 18 * k;
        ts.color = Color(255, 255, 255, 230);
        r.drawText(p.gamertag, rc.x + 12 * k, rc.y + 44 * k, ts);
        std::string pts = std::to_string(p.points) + " P";
        r.drawText(pts.c_str(), rc.x + 12 * k, rc.y + 66 * k, ts);
        r.drawText("Offline", rc.x + 12 * k, rc.bottom() - 30 * k, ts);
        drawIcon(r, Icon::Person, rc.x + rc.w * 0.74f, rc.y + rc.h * 0.6f, 70 * k, Color(255, 255, 255, 200), c.b);
    } else {
        Color bg = c.art ? Color(0, 0, 0, 0) : lerpColor(c.a, c.b, 0.5f);
        drawIcon(r, c.icon, rc.cx(), rc.cy() + 8 * k, 62 * k, fg.fade(0.95f), bg);
    }
    if (c.art) r.fillGradientV(Rectf{rc.x, rc.y, rc.w, 52 * k}, Color(0, 0, 0, 120), Color(0, 0, 0, 0));
    TextStyle ls;
    ls.size = 26 * k;
    ls.color = fg;
    r.pushClip(rc);
    r.drawText(c.label.c_str(), rc.x + 12 * k, rc.y + 10 * k, ls);
    r.popClip();
    if (fav) {
        r.fillCircle(rc.right() - 22 * k, rc.bottom() - 22 * k, 16 * k, Color(0, 0, 0, 110));
        drawIcon(r, Icon::Star, rc.right() - 22 * k, rc.bottom() - 23 * k, 22 * k, Color::hex(0xFFD23A));
    }
}

void drawDetail(IRenderer& r, const KState& st, const KTheme& t) {
    float a = ease::outCubic(st.detailAnim);
    if (a <= 0.01f) return;
    const KDetail& d = st.detail;
    r.pushAlpha(a);
    r.pushOffset(0, (1 - a) * 160);
    r.fillGradientV(Rectf{0, 0, 1280, 720}, t.top, brighten(t.mid, -0.2f));
    r.fillRect(Rectf{0, 0, 1280, 720}, Color(0, 0, 0, 40));
    waveBand(r, -40, 30, 20, 0.004f, 0.6f, t.wave2.withAlpha(120));
    // botao circular de voltar
    r.strokeCircle(86, 84, 44, 4, Color(255, 255, 255, 200));
    drawIcon(r, Icon::ArrowLeft, 86, 84, 44, Color(255, 255, 255, 220));
    TextStyle tt;
    tt.size = 56;
    tt.weight = FontWeight::Bold;
    tt.color = Color(255, 255, 255);
    r.drawText(d.title.c_str(), 160, 52, tt);
    if (!d.subtitle.empty()) {
        TextStyle ss;
        ss.size = 22;
        ss.color = Color(255, 255, 255, 200);
        r.drawText(d.subtitle.c_str(), 164, 116, ss);
    }
    Rectf art{100, 168, 520, 330};
    r.fillRect(art.offset(4, 6), Color(0, 0, 0, 70));
    if (d.artSeed) drawAbstractArt(r, art, d.artSeed, d.c1, d.c2);
    else r.fillGradientV(art, d.c1, d.c2);
    drawIcon(r, d.icon, art.cx(), art.cy(), 120, Color(255, 255, 255, 210), d.artSeed ? Color(0, 0, 0, 0) : lerpColor(d.c1, d.c2, 0.5f));
    TextStyle ds;
    ds.size = 20;
    ds.color = Color(255, 255, 255, 220);
    drawWrapped(r, d.desc.c_str(), 100, 516, 520, ds, 28, 3);

    // Botoes grandes (Kinect-era: poucos, grandes e claros)
    const float bx = 680, bw = 500, by = 168, bh = 72, gap = 12;
    const int visible = 5;
    r.pushClip(Rectf{bx - 20, by - 10, bw + 40, visible * (bh + gap) + 10});
    for (int i = 0; i < int(d.buttons.size()); ++i) {
        float y = by + (i - st.detailScroll) * (bh + gap);
        if (y < by - bh - gap || y > by + visible * (bh + gap)) continue;
        const InfoRow& b = d.buttons[size_t(i)];
        bool sel = i == st.detailSel;
        Rectf rc{bx, y, bw, bh};
        if (sel) {
            float s = 1.0f + 0.03f * ease::outBack(st.pop);
            rc = rc.scaledAboutCenter(s);
            r.fillRoundRect(rc.offset(3, 6), 10, Color(0, 0, 0, 70));
            r.fillRoundRect(rc, 10, Color(255, 255, 255));
        } else {
            r.fillRoundRect(rc, 10, Color(255, 255, 255, 38));
        }
        Color fg = sel ? Color::hex(0x2B2236) : Color(255, 255, 255);
        drawIcon(r, Icon(b.icon), rc.x + 40, rc.cy(), 34, fg, sel ? Color(255, 255, 255) : Color(0, 0, 0, 0));
        TextStyle bt;
        bt.size = 26;
        bt.weight = sel ? FontWeight::Bold : FontWeight::Regular;
        bt.color = fg;
        r.drawText(b.title.c_str(), rc.x + 78, b.sub.empty() ? textTopForCenter(rc.cy(), 26) : rc.y + 10, bt);
        if (!b.sub.empty()) {
            TextStyle sb;
            sb.size = 16;
            sb.color = sel ? Color::hex(0x6A6276) : Color(255, 255, 255, 190);
            r.drawText(b.sub.c_str(), rc.x + 79, rc.y + 44, sb);
        }
        if (b.progress >= 0) {
            Rectf bar{rc.right() - 140, rc.cy() - 4, 120, 8};
            r.fillRoundRect(bar, 4, sel ? Color(0, 0, 0, 40) : Color(255, 255, 255, 60));
            r.fillRoundRect(Rectf{bar.x, bar.y, bar.w * clampf(b.progress, 0, 1), bar.h}, 4, sel ? t.accent : Color(255, 255, 255));
        }
    }
    r.popClip();
    drawHints(r, {{Button::B, "Voltar"}, {Button::A, "Selecionar"}}, 1180, 670, 32, Color(255, 255, 255), Align::Right);
    r.popOffset();
    r.popAlpha();
}

// Camada estatica (cache): fundo, titulo, paginas de cartoes, indicador, detalhe.
void renderKinectStatic(IRenderer& r, const KState& st, TextureId bg) {
    const KTheme& t = kThemes[st.theme];
    const auto& secs = sections();
    if (bg != kInvalidTexture) r.drawTexture(bg, Rectf{0, 0, float(kLogicalW), float(kLogicalH)});
    else drawBackground(r, st.theme);
    float e = ease::outCubic(st.enter);

    // Titulo grande translucido da secao (desliza com a pagina)
    for (int i = 0; i < int(secs.size()); ++i) {
        float d = i - st.pagePos;
        if (std::fabs(d) >= 1.0f) continue;
        TextStyle big;
        big.size = 112;
        big.weight = FontWeight::Bold;
        big.align = Align::Right;
        big.tracking = 2;
        big.color = Color(255, 255, 255, uint8_t(55 * (1 - std::fabs(d)) * e));
        r.drawText(secs[size_t(i)].title, 1130 + d * 500, 46, big);
    }
    // botao circular de voltar
    r.pushAlpha(e);
    r.strokeCircle(86, 84, 44, 4, Color(255, 255, 255, 200));
    drawIcon(r, Icon::ArrowLeft, 86, 84, 44, Color(255, 255, 255, 220));
    r.popAlpha();

    // Paginas: a atual e as vizinhas durante o deslize
    for (int p = 0; p < int(secs.size()); ++p) {
        float d = p - st.pagePos;
        if (std::fabs(d) >= 1.2f) continue;
        const auto& cards = secs[size_t(p)].cards;
        r.pushOffset(d * 1280, 0);
        for (int i = 0; i < int(cards.size()); ++i) {
            if (p == st.section && i == st.sel[p]) continue;  // selecionado por ultimo
            float local = ease::outCubic(clampf((st.enter - 0.05f * i) / 0.6f, 0, 1));
            Rectf rc = cardRect(i).offset(0, (1 - local) * 60);
            r.pushAlpha(local);
            drawCard(r, cards[size_t(i)], rc, st.favorites.count(p * 100 + i) != 0, p);
            r.popAlpha();
        }
        r.popOffset();
    }
    // Cartao selecionado: salta com moldura branca e acompanha a pagina
    {
        int p = st.section;
        const auto& cards = secs[size_t(p)].cards;
        int i = clampi(st.sel[p], 0, int(cards.size()) - 1);
        float d = p - st.pagePos;
        float s = 1.0f + 0.06f * ease::outBack(st.pop);
        Rectf rc = Rectf{st.hlX, st.hlY, kCardW, kCardH}.scaledAboutCenter(s).offset(d * 1280, 0);
        r.pushAlpha(ease::outCubic(clampf(st.enter / 0.6f, 0, 1)));
        r.fillRect(rc.offset(6, 10), Color(0, 0, 0, 70));
        drawCard(r, cards[size_t(i)], rc, st.favorites.count(p * 100 + i) != 0, p);
        r.strokeRect(rc.inset(-5), 5, Color(255, 255, 255));
        r.popAlpha();
    }

    // Indicador de secoes
    float total = 0;
    TextStyle ind;
    ind.size = 20;
    for (const auto& s : secs) total += r.measureText(s.name, ind) + 34;
    float x = 640 - total * 0.5f;
    for (int i = 0; i < int(secs.size()); ++i) {
        bool cur = i == st.section;
        ind.weight = cur ? FontWeight::Bold : FontWeight::Regular;
        ind.color = cur ? t.accent : t.ink.fade(0.7f);
        r.drawText(secs[size_t(i)].name, x, 560, ind);
        if (cur) r.fillRect(Rectf{x, 588, r.measureText(secs[size_t(i)].name, ind), 3}, t.accent);
        x += r.measureText(secs[size_t(i)].name, ind) + 34;
    }
    // Estado do sensor (o Neutro X nao usa Kinect) e dicas
    drawIcon(r, Icon::Sensor, 120, 640, 60, t.ink, t.floorBottom);
    TextStyle sn;
    sn.size = 16;
    sn.color = t.ink;
    r.drawText("Sensor Kinect não detectado · usando o controle", 160, 664, sn);
    drawHints(r, {{Button::X, "Favorito"}, {Button::Y, "Opções"}, {Button::A, "Selecionar"}}, 1180, 672, 30, t.ink, Align::Right);
    drawDetail(r, st, t);
}

// Camada ao vivo: avatar (pagina Inicio) e opcoes.
void renderKinectLive(IRenderer& r, const KState& st) {
    const KTheme& t = kThemes[st.theme];
    float d = 0 - st.pagePos;
    float a = (1 - clampf(std::fabs(d), 0, 1)) * ease::outCubic(clampf((st.enter - 0.25f) / 0.6f, 0, 1)) *
              (1 - ease::outCubic(st.detailAnim));
    if (st.showAvatar && a > 0.01f) {
        AvatarLook look;
        look.shirt = Color::hex(0xB03A7A);
        look.hair = Color::hex(0x1E1410);
        look.skin = Color::hex(0x9A6A4A);
        r.pushAlpha(a);
        r.pushOffset(d * 1280, 0);
        // na margem esquerda, ao lado da grade (nao cobre cartoes nem o indicador)
        drawAvatar(r, 74, 556, 250, look, st.time);
        r.popOffset();
        r.popAlpha();
    }
    float oa = ease::outCubic(st.optionsAnim);
    if (oa > 0.01f) {
        r.pushAlpha(oa);
        r.fillRect(Rectf{0, 0, 1280, 720}, Color(0, 0, 0, 110));
        Rectf box = Rectf{390, 210, 500, 290}.scaledAboutCenter(0.92f + 0.08f * oa);
        r.fillRect(box.offset(5, 8), Color(0, 0, 0, 80));
        r.fillGradientV(box, t.mid, t.top);
        TextStyle tt;
        tt.size = 32;
        tt.weight = FontWeight::Bold;
        tt.color = Color(255, 255, 255);
        r.drawText("Opções", box.x + 28, box.y + 22, tt);
        const char* labels[3] = {"Fundo", "Avatar", "Fechar"};
        std::string vals[3] = {kThemes[st.theme].name, st.showAvatar ? "Mostrar" : "Ocultar", ""};
        for (int i = 0; i < 3; ++i) {
            Rectf row{box.x + 18, box.y + 82 + i * 64.0f, box.w - 36, 56};
            bool sel = i == st.optionsSel;
            r.fillRoundRect(row, 10, sel ? Color(255, 255, 255) : Color(255, 255, 255, 38));
            TextStyle ls;
            ls.size = 24;
            ls.color = sel ? Color::hex(0x2B2236) : Color(255, 255, 255);
            ls.weight = sel ? FontWeight::Bold : FontWeight::Regular;
            r.drawText(labels[i], row.x + 18, row.y + 16, ls);
            ls.align = Align::Right;
            r.drawText(vals[i].c_str(), row.right() - 18, row.y + 16, ls);
        }
        r.popAlpha();
    }
}

// ------------------------------------------------------------ dashboard

class KinectDashboard : public Dashboard {
public:
    KinectDashboard() : Dashboard(DashboardId::Kinect) {}

    bool load(Context& ctx, std::string& error) override {
        loadOptions(ctx);  // opcoes salvas desta interface
        if (!cache_.init(*ctx.renderer) || !rebuildBackground(ctx)) {
            error = "falha ao criar as texturas do Kinect";
            return false;
        }
        return true;
    }

    void unload(Context& ctx) override {
        if (bg_ != kInvalidTexture) ctx.renderer->destroyTexture(bg_);
        bg_ = kInvalidTexture;
        cache_.release(*ctx.renderer);
    }

    void onEnter(Context&) override {
        enter_.start(0.8f);
        st_.enter = 0;
        pop_.start(0.3f);
        moveHighlight(true);
    }

    std::string debugState() const override {
        char b[160];
        std::snprintf(b, sizeof(b), "section=%d sel=%d detail=%d detailSel=%d options=%d theme=%d avatar=%d favs=%d",
                      st_.section, st_.sel[st_.section], st_.detailOpen ? 1 : 0, st_.detailSel, st_.optionsOpen ? 1 : 0,
                      st_.theme, st_.showAvatar ? 1 : 0, int(st_.favorites.size()));
        return b;
    }

    void update(Context& ctx, const InputFrame& in, float dt) override {
        enter_.update(dt);
        st_.enter = enter_.progress();
        st_.time += dt;
        page_.update(dt);
        st_.pagePos = page_.value;
        hlX_.update(dt);
        hlY_.update(dt);
        st_.hlX = hlX_.value;
        st_.hlY = hlY_.value;
        pop_.update(dt);
        st_.pop = pop_.progress();
        detailAnim_.update(dt);
        st_.detailAnim = detailAnim_.value;
        detailSel_.update(dt);
        st_.detailSelAnim = detailSel_.value;
        detailScroll_.update(dt);
        st_.detailScroll = detailScroll_.value;
        optionsAnim_.update(dt);
        st_.optionsAnim = optionsAnim_.value;

        for (int b = 0; b < kButtonCount; ++b)
            if (in.pressed[b]) { ++version_; break; }
        if (st_.optionsOpen) { updateOptions(ctx, in); return; }
        if (st_.detailOpen) { updateDetail(ctx, in); return; }
        updateGrid(ctx, in);
    }

    void render(Context& ctx, IRenderer& r) override {
        (void)ctx;
        cache_.draw(r, animating(), version_, [this](IRenderer& rr) { renderKinectStatic(rr, st_, bg_); });
        renderKinectLive(r, st_);
    }

private:
    void loadOptions(Context& ctx) {
        st_.theme = ctx.settings->opt("kinect.theme", 0, 0, kThemeCount - 1);
        st_.showAvatar = ctx.settings->opt("kinect.avatar", 1, 0, 1) != 0;
    }
    void saveOptions(Context& ctx) {
        ctx.settings->setOpt("kinect.theme", st_.theme);
        ctx.settings->setOpt("kinect.avatar", st_.showAvatar ? 1 : 0);
        ctx.app->saveSettings();
    }

    bool animating() const {
        return !enter_.done() || !page_.settled() || !hlX_.settled() || !hlY_.settled() || !pop_.done() ||
               !detailAnim_.settled() || !detailScroll_.settled();
    }

    bool rebuildBackground(Context& ctx) {
        IRenderer& r = *ctx.renderer;
        if (bg_ != kInvalidTexture) r.destroyTexture(bg_);
        SoftRenderer canvas(r.pixelWidth(), r.pixelHeight());
        canvas.beginFrame();
        drawBackground(canvas, st_.theme);
        bg_ = r.createTexture(canvas.pixelWidth(), canvas.pixelHeight(), canvas.pixels());
        ++version_;
        return bg_ != kInvalidTexture;
    }

    void moveHighlight(bool snap) {
        Rectf rc = cardRect(st_.sel[st_.section]);
        if (snap) { hlX_.snap(rc.x); hlY_.snap(rc.y); }
        else { hlX_.set(rc.x); hlY_.set(rc.y); }
    }

    void goSection(Context& ctx, int ns, int keepRow, bool fromLeft) {
        const auto& secs = sections();
        if (ns < 0 || ns >= int(secs.size())) { ctx.sfx(Sfx::Error, 0.6f); return; }
        st_.section = ns;
        int count = int(secs[size_t(ns)].cards.size());
        int rows = (count + kCols - 1) / kCols;
        int row = clampi(keepRow, 0, rows - 1);
        int col = fromLeft ? 0 : kCols - 1;
        int idx = std::min(row * kCols + col, count - 1);
        st_.sel[ns] = idx;
        page_.set(float(ns));
        moveHighlight(true);  // o destaque acompanha a nova pagina
        pop_.start(0.3f);
        ctx.sfx(Sfx::Tab);
    }

    void updateGrid(Context& ctx, const InputFrame& in) {
        const auto& secs = sections();
        int s = st_.section;
        int count = int(secs[size_t(s)].cards.size());
        int& sel = st_.sel[s];
        int col = sel % kCols, row = sel / kCols;
        int before = sel;
        if (in.press(Button::RB)) { goSection(ctx, s + 1, row, true); return; }
        if (in.press(Button::LB)) { goSection(ctx, s - 1, row, false); return; }
        if (in.press(Button::Right)) {
            if (col + 1 < kCols && sel + 1 < count) sel++;
            else { goSection(ctx, s + 1, row, true); return; }
        }
        if (in.press(Button::Left)) {
            if (col > 0) sel--;
            else { goSection(ctx, s - 1, row, false); return; }
        }
        // Baixo: proxima linha; se a linha de baixo for mais curta, vai ao ultimo cartao dela.
        if (in.press(Button::Down) && (row + 1) * kCols < count) sel = std::min(sel + kCols, count - 1);
        if (in.press(Button::Up) && row > 0) sel -= kCols;
        if (sel != before) {
            moveHighlight(false);
            pop_.start(0.3f);
            ctx.sfx(Sfx::Navigate);
        } else if (in.press(Button::Down) || in.press(Button::Up)) {
            ctx.sfx(Sfx::Error, 0.4f);
        }
        const KCard& card = secs[size_t(s)].cards[size_t(sel)];
        if (in.press(Button::A)) {
            if (card.kind == Kind::OpenSettings) { ctx.sfx(Sfx::Confirm); ctx.app->openSettings(); }
            else if (card.kind == Kind::OpenAbout) { ctx.sfx(Sfx::Confirm); ctx.app->openAbout(); }
            else if (s == 0 && card.kind == Kind::Generic && card.index >= 1 && card.index <= 5) {
                // atalhos do Inicio: o indice do cartao e a secao de destino
                ctx.sfx(Sfx::Confirm);
                goSection(ctx, card.index, 0, true);
            } else {
                st_.detail = buildDetail(card, &ctx);
                st_.detailOpen = true;
                st_.detailSel = 0;
                detailSel_.snap(0);
                detailScroll_.snap(0);
                detailAnim_.set(1);
                pop_.start(0.3f);
                ctx.sfx(Sfx::Confirm);
            }
        }
        if (in.press(Button::X)) {
            int key = s * 100 + sel;
            if (st_.favorites.count(key)) { st_.favorites.erase(key); ctx.app->toast(card.label + ": removido dos favoritos"); }
            else { st_.favorites.insert(key); ctx.app->toast(card.label + ": marcado como favorito"); }
            ctx.sfx(Sfx::Confirm, 0.8f);
        }
        if (in.press(Button::Y)) { st_.optionsOpen = true; st_.optionsSel = 0; optionsAnim_.set(1); ctx.sfx(Sfx::Open); }
        if (in.press(Button::B)) {
            if (sel != 0) { sel = 0; moveHighlight(false); pop_.start(0.3f); ctx.sfx(Sfx::Back); }
            else if (s != 0) goSection(ctx, 0, 0, true);
            else { ctx.sfx(Sfx::Back, 0.6f); ctx.app->toast("Pressione START para o menu do Neutro X"); }
        }
    }

    void closeDetail(Context& ctx) {
        st_.detailOpen = false;
        detailAnim_.set(0);
        ctx.sfx(Sfx::Back);
    }

    void updateDetail(Context& ctx, const InputFrame& in) {
        int n = int(st_.detail.buttons.size());
        int before = st_.detailSel;
        if (in.press(Button::Down) && st_.detailSel + 1 < n) st_.detailSel++;
        if (in.press(Button::Up) && st_.detailSel > 0) st_.detailSel--;
        if (before != st_.detailSel) {
            ctx.sfx(Sfx::Navigate);
            pop_.start(0.25f);
            detailSel_.set(float(st_.detailSel));
            const int visible = 5;
            float scroll = detailScroll_.target;
            if (st_.detailSel < scroll) scroll = float(st_.detailSel);
            if (st_.detailSel > scroll + visible - 1) scroll = float(st_.detailSel - visible + 1);
            detailScroll_.set(scroll);
        }
        if (in.press(Button::A) && n > 0) {
            const InfoRow& b = st_.detail.buttons[size_t(st_.detailSel)];
            if (b.action == kBtnClose) { closeDetail(ctx); return; }
            if (b.action == kBtnPlay) {
                ctx.sfx(Sfx::Error, 0.7f);
                ctx.app->toast("Jogos de demonstração não são executados pelo Neutro X");
            } else {
                ctx.sfx(Sfx::Open, 0.8f);
                ctx.app->toast(b.title + ": item de demonstração (somente informativo)");
            }
        }
        if (in.press(Button::B)) closeDetail(ctx);
    }

    void updateOptions(Context& ctx, const InputFrame& in) {
        if (in.press(Button::Down)) { st_.optionsSel = (st_.optionsSel + 1) % 3; ctx.sfx(Sfx::Navigate); }
        if (in.press(Button::Up)) { st_.optionsSel = (st_.optionsSel + 2) % 3; ctx.sfx(Sfx::Navigate); }
        int delta = (in.press(Button::Right) || in.press(Button::A)) ? 1 : (in.press(Button::Left) ? -1 : 0);
        if (delta != 0 && st_.optionsSel == 0) {
            st_.theme = (st_.theme + delta + kThemeCount) % kThemeCount; saveOptions(ctx);
            rebuildBackground(ctx);
            ctx.sfx(Sfx::Tab);
        } else if (delta != 0 && st_.optionsSel == 1) {
            st_.showAvatar = !st_.showAvatar; saveOptions(ctx);
            ctx.sfx(Sfx::Tab);
        } else if ((in.press(Button::A) && st_.optionsSel == 2) || in.press(Button::B) || in.press(Button::Y)) {
            st_.optionsOpen = false;
            optionsAnim_.set(0);
            ctx.sfx(Sfx::Back);
        }
    }

    KState st_;
    Tween enter_, pop_;
    Smooth page_{0, 9};
    Smooth hlX_{kGridX, 16}, hlY_{kGridY, 16};
    Smooth detailAnim_{0, 12};
    Smooth detailSel_{0, 18};
    Smooth detailScroll_{0, 14};
    Smooth optionsAnim_{0, 16};
    SceneCache cache_;
    TextureId bg_ = kInvalidTexture;
    unsigned version_ = 1;
};

}  // namespace

void drawKinectPreview(IRenderer& r, float time) {
    KState st;
    st.time = time;
    renderKinectStatic(r, st, kInvalidTexture);
    renderKinectLive(r, st);
}

std::unique_ptr<Dashboard> createKinectDashboard() { return std::unique_ptr<Dashboard>(new KinectDashboard()); }

}  // namespace nx
