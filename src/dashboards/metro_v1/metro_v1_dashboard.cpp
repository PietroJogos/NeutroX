// Neutro X - Dashboard "Metro V1" (inspirado na geracao 2011-2012, referencia 2.0.15574.0).
//
// Recriacao original: fundo claro, categorias em tipografia leve no topo (o
// cabecalho rola para manter a categoria atual a esquerda) e um grupo de blocos
// retangulares por categoria, organizado em linhas. Os grupos sao paginas que
// deslizam na horizontal; as setas fazem navegacao espacial entre blocos e,
// na borda, mudam de categoria. O bloco selecionado ganha borda branca destacada.
// Cada bloco tem uma acao (pagina demonstrativa local ou tela do proprio app).
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

enum class Kind { Generic, Game, Profile, OpenSettings, OpenAbout, OpenSounds };
enum class Style { Accent, Art, Dark, Light };

struct MTile {
    int col, row, w, h;
    Icon icon;
    std::string label;
    std::string desc;
    Kind kind;
    int index;
    Style style;
    int seed;
    Color c1, c2;
};

struct MCategory {
    const char* name;
    std::vector<MTile> tiles;
};

MTile tile(int c, int r, int w, int h, Icon ic, std::string label, std::string desc, Kind k, int idx, Style st,
           int seed = 0, Color c1 = Color(), Color c2 = Color()) {
    return MTile{c, r, w, h, ic, std::move(label), std::move(desc), k, idx, st, seed, c1, c2};
}

MTile gameTile(int c, int r, int w, int h, int gi) {
    int n = 0;
    const DemoGame& g = demoGames(n)[gi];
    return tile(c, r, w, h, g.arcade ? Icon::Star : Icon::Controller, g.title, g.description, Kind::Game, gi, Style::Art,
                g.artSeed, g.c1, g.c2);
}

std::vector<MCategory> buildCategories() {
    std::vector<MCategory> c(8);
    int mn = 0, vn = 0;
    const DemoMedia* mu = demoMusic(mn);
    const DemoMedia* vi = demoVideos(vn);
    c[0].name = "início";
    c[0].tiles = {gameTile(0, 0, 2, 2, 0),
                  tile(2, 0, 1, 1, Icon::Controller, "Meus jogos", "Sua coleção de jogos de demonstração.", Kind::Generic, 1, Style::Accent),
                  tile(3, 0, 1, 1, Icon::Person, "Perfil", "Seu perfil local.", Kind::Profile, 0, Style::Accent),
                  tile(2, 1, 2, 1, Icon::Friends, "Amigos", "Amigos de demonstração (modo offline).", Kind::Generic, 2, Style::Art, 1501,
                       Color::hex(0x58B4E8), Color::hex(0x1A4A6A)),
                  tile(4, 0, 1, 1, Icon::Trophy, "Conquistas", "Pontos e conquistas por jogo.", Kind::Generic, 3, Style::Dark),
                  tile(4, 1, 1, 1, Icon::Mail, "Mensagens", "Caixa de mensagens local.", Kind::Generic, 4, Style::Accent)};
    c[1].name = "social";
    c[1].tiles = {tile(0, 0, 2, 2, Icon::Person, "Perfil", "Seu perfil e avatar genérico próprio.", Kind::Profile, 0, Style::Light),
                  tile(2, 0, 1, 1, Icon::Friends, "Amigos", "Amigos de demonstração (modo offline).", Kind::Generic, 2, Style::Accent),
                  tile(3, 0, 1, 1, Icon::Mail, "Mensagens", "Caixa de mensagens local.", Kind::Generic, 4, Style::Dark),
                  tile(2, 1, 2, 1, Icon::Clock, "Atividade recente", "O que você jogou por último (demonstração).", Kind::Generic, 5,
                       Style::Art, 1502, Color::hex(0xE8A040), Color::hex(0x5A3010))};
    c[2].name = "tv";
    c[2].tiles = {tile(0, 0, 2, 2, Icon::Tv, "Canal Neutro", "Canal de demonstração; nada é transmitido.", Kind::Generic, 10, Style::Art, 1503,
                       Color::hex(0x5A6070), Color::hex(0x15181E)),
                  tile(2, 0, 1, 1, Icon::List, "Guia", "Guia de programação de demonstração.", Kind::Generic, 11, Style::Accent),
                  tile(2, 1, 1, 1, Icon::Video, "Gravações", "Nenhuma gravação (demonstração).", Kind::Generic, 12, Style::Dark),
                  tile(3, 0, 1, 1, Icon::Trophy, "Esportes", "Resultados de demonstração.", Kind::Generic, 13, Style::Accent),
                  tile(3, 1, 1, 1, Icon::Globe, "Notícias", "Manchetes de demonstração (offline).", Kind::Generic, 14, Style::Accent)};
    c[3].name = "vídeo";
    c[3].tiles = {tile(0, 0, 1, 1, Icon::Video, "Meus vídeos", "Biblioteca de vídeos local de demonstração.", Kind::Generic, 20, Style::Accent),
                  tile(0, 1, 1, 1, Icon::Video, vi[0].title, vi[0].detail, Kind::Generic, 21, Style::Art, 1504, Color::hex(0x58B4E8),
                       Color::hex(0x10305A)),
                  tile(1, 0, 2, 2, Icon::Video, vn > 1 ? vi[1].title : "Vídeo", vn > 1 ? vi[1].detail : "", Kind::Generic, 22, Style::Art,
                       15574, Color::hex(0x58B4E8), Color::hex(0x1A5A2A)),
                  tile(3, 0, 1, 1, Icon::Tv, "Filmes", "Catálogo de demonstração.", Kind::Generic, 23, Style::Art, 1505,
                       Color::hex(0x5A6070), Color::hex(0x20242C)),
                  tile(3, 1, 1, 1, Icon::Video, "Trailers", "Trailers de demonstração.", Kind::Generic, 24, Style::Dark)};
    c[4].name = "jogos";
    c[4].tiles = {tile(0, 0, 1, 1, Icon::Controller, "Meus jogos", "Sua coleção de jogos de demonstração.", Kind::Generic, 1, Style::Accent),
                  tile(0, 1, 1, 1, Icon::Trophy, "Conquistas", "Pontos e conquistas por jogo.", Kind::Generic, 3, Style::Art, 1506,
                       Color::hex(0xC89A5A), Color::hex(0x5A3A1A)),
                  gameTile(1, 0, 2, 2, 1),
                  gameTile(3, 0, 1, 1, 2),
                  gameTile(3, 1, 1, 1, 3),
                  gameTile(4, 0, 2, 1, 4),
                  tile(4, 1, 1, 1, Icon::Download, "Demos", "Demonstrações locais.", Kind::Generic, 6, Style::Dark),
                  tile(5, 1, 1, 1, Icon::List, "A - Z", "Todos os jogos de demonstração em ordem.", Kind::Generic, 7, Style::Accent)};
    c[5].name = "música";
    c[5].tiles = {tile(0, 0, 2, 2, Icon::Music, mu[0].title, mu[0].detail, Kind::Generic, 30, Style::Art, 1507, Color::hex(0xE0407A),
                       Color::hex(0x3A0A20)),
                  tile(2, 0, 1, 1, Icon::Music, "Minha música", "Biblioteca de música de demonstração.", Kind::Generic, 31, Style::Accent),
                  tile(2, 1, 1, 1, Icon::Speaker, "Rádio", "Rádio de demonstração (sem áudio).", Kind::Generic, 32, Style::Dark),
                  tile(3, 0, 2, 1, Icon::List, "Listas de reprodução", "Listas de demonstração.", Kind::Generic, 33, Style::Accent),
                  tile(3, 1, 1, 1, Icon::Music, mn > 1 ? mu[1].title : "Faixa", mn > 1 ? mu[1].detail : "", Kind::Generic, 34, Style::Art,
                       1508, Color::hex(0x9A5CD6), Color::hex(0x24103A)),
                  tile(4, 1, 1, 1, Icon::Music, mn > 2 ? mu[2].title : "Faixa", mn > 2 ? mu[2].detail : "", Kind::Generic, 35, Style::Art,
                       1509, Color::hex(0xF0A020), Color::hex(0x5A3A08))};
    c[6].name = "aplicativos";
    c[6].tiles = {tile(0, 0, 1, 1, Icon::Clock, "Relógio", "Hora local do aparelho.", Kind::Generic, 40, Style::Accent),
                  tile(1, 0, 1, 1, Icon::Globe, "Clima", "Previsão de demonstração (offline).", Kind::Generic, 41, Style::Accent),
                  tile(0, 1, 1, 1, Icon::Picture, "Fotos", "Imagens de demonstração.", Kind::Generic, 42, Style::Dark),
                  tile(1, 1, 1, 1, Icon::List, "Notas", "Notas de demonstração.", Kind::Generic, 43, Style::Accent),
                  tile(2, 0, 2, 1, Icon::Plus, "Calculadora", "Aplicativo de demonstração.", Kind::Generic, 44, Style::Dark),
                  tile(2, 1, 2, 1, Icon::Apps, "Todos os aplicativos", "Lista de aplicativos de demonstração.", Kind::Generic, 45,
                       Style::Accent)};
    c[7].name = "configurações";
    c[7].tiles = {tile(0, 0, 1, 1, Icon::Gear, "Sistema", "Resumo somente-leitura: nada do console é alterado.", Kind::Generic, 50, Style::Accent),
                  tile(1, 0, 1, 1, Icon::Person, "Perfil", "Seu perfil local.", Kind::Profile, 0, Style::Accent),
                  tile(0, 1, 1, 1, Icon::Family, "Família", "Representação visual.", Kind::Generic, 52, Style::Dark),
                  tile(1, 1, 1, 1, Icon::Speaker, "Sons originais", "Estado dos sons originais na pasta local.", Kind::OpenSounds, 0,
                       Style::Accent),
                  tile(2, 0, 2, 1, Icon::Palette, "Neutro X", "Configurações do próprio Neutro X.", Kind::OpenSettings, 0, Style::Dark),
                  tile(2, 1, 2, 1, Icon::Info, "Sobre o Neutro X", "Versão, créditos e segurança.", Kind::OpenAbout, 0, Style::Accent)};
    return c;
}

const std::vector<MCategory>& categories() {
    static const std::vector<MCategory> c = buildCategories();
    return c;
}

struct MColor {
    const char* name;
    unsigned rgb;
};
const MColor kTileColors[] = {{"verde", 0x5DAE2A}, {"azul", 0x1F8AD8}, {"laranja", 0xE8742A}, {"roxo", 0x8A4FC8}};
constexpr int kTileColorCount = int(sizeof(kTileColors) / sizeof(kTileColors[0]));
const char* const kBgNames[] = {"claro", "cinza"};

constexpr float kCell = 140, kGap = 4, kGroupX = 248, kGroupY = 200, kPageSpacing = 1000;

Rectf tileRect(const MTile& t) {
    return Rectf{kGroupX + t.col * (kCell + kGap), kGroupY + t.row * (kCell + kGap), t.w * kCell + (t.w - 1) * kGap,
                 t.h * kCell + (t.h - 1) * kGap};
}

// ------------------------------------------------------------ paginas de detalhe

enum RowAct { kActInfo = 0, kActPlay = 1 };

struct MDetail {
    std::string category, title, desc;
    MTile tile;
    std::vector<InfoRow> rows;
};

InfoRow row(Icon ic, std::string t, std::string s, int act = kActInfo, float prog = -1) {
    InfoRow r;
    r.icon = int(ic);
    r.title = std::move(t);
    r.sub = std::move(s);
    r.action = act;
    r.progress = prog;
    return r;
}

MDetail buildDetail(const MCategory& cat, const MTile& t, Context* ctx) {
    MDetail d;
    d.category = cat.name;
    d.title = t.label;
    d.desc = t.desc;
    d.tile = t;
    int n = 0;
    const DemoGame* g = demoGames(n);
    const DemoProfile& p = demoProfile();
    if (t.kind == Kind::Game) {
        const DemoGame& gm = g[t.index];
        d.rows = {row(Icon::Controller, "jogar", "demonstração (não executável)", kActPlay),
                  row(Icon::Trophy, "conquistas", std::to_string(gm.points) + " de " + std::to_string(gm.maxPoints) + " pontos",
                      kActInfo, float(gm.points) / gm.maxPoints),
                  row(Icon::Info, "detalhes", std::string(gm.genre) + " · " + gm.year)};
        return d;
    }
    if (t.kind == Kind::Profile) {
        d.rows = {row(Icon::Person, p.gamertag, "nome do jogador (local)"),
                  row(Icon::Trophy, std::to_string(p.points) + " pontos", "pontuação de demonstração"),
                  row(Icon::Mail, p.motto, "lema"), row(Icon::Globe, p.zone, "zona de jogo")};
        return d;
    }
    switch (t.index) {
        case 1:
        case 7:
            for (int i = 0; i < n; ++i) d.rows.push_back(row(g[i].arcade ? Icon::Star : Icon::Controller, g[i].title, std::string(g[i].genre) + " · " + g[i].year));
            if (t.index == 7) std::sort(d.rows.begin(), d.rows.end(), [](const InfoRow& a, const InfoRow& b) { return a.title < b.title; });
            break;
        case 2: {
            int fn = 0;
            const DemoFriend* f = demoFriends(fn);
            for (int i = 0; i < fn; ++i) d.rows.push_back(row(Icon::Person, f[i].name, f[i].status));
            break;
        }
        case 3:
            for (int i = 0; i < n; ++i)
                d.rows.push_back(row(Icon::Trophy, g[i].title, std::to_string(g[i].points) + " / " + std::to_string(g[i].maxPoints),
                                     kActInfo, float(g[i].points) / g[i].maxPoints));
            break;
        case 4: d.rows = {row(Icon::Mail, "Bem-vindo ao Neutro X", "mensagem do sistema · lida")}; break;
        case 5:
            for (int i = 0; i < 4 && i < n; ++i) d.rows.push_back(row(Icon::Clock, g[i].title, i == 0 ? "hoje" : "esta semana"));
            break;
        case 40: {
            char b[40] = "12:00";
            if (ctx && ctx->platform) {
                int h = 0, mi = 0, dd = 1, mo = 1, y = 2026;
                ctx->platform->localTime(h, mi, dd, mo, y);
                std::snprintf(b, sizeof(b), "%02d:%02d · %02d/%02d/%04d", h, mi, dd, mo, y);
            }
            d.rows = {row(Icon::Clock, b, "hora local")};
            break;
        }
        case 50: {
            char res[48] = "1280 × 720";
            if (ctx && ctx->renderer) std::snprintf(res, sizeof(res), "%d × %d", ctx->renderer->pixelWidth(), ctx->renderer->pixelHeight());
            d.rows = {row(Icon::Tv, "resolução do Neutro X", res),
                      row(Icon::Speaker, "volume dos efeitos", ctx && ctx->settings ? std::to_string(ctx->settings->sfxVolume) + "%" : "80%"),
                      row(Icon::Globe, "idioma", "português (Brasil)"), row(Icon::Network, "rede", "modo offline")};
            break;
        }
        case 52:
            d.rows = {row(Icon::Family, "controle dos pais", "não configurado (visual)"),
                      row(Icon::Clock, "temporizador de família", "desativado (visual)")};
            break;
        default:
            if (t.index >= 30 && t.index < 40)
                d.rows = {row(Icon::Music, "reproduzir", "demonstração (sem áudio)"), row(Icon::List, "adicionar à lista", "demonstração")};
            else if (t.index >= 20 && t.index < 30)
                d.rows = {row(Icon::Video, "assistir", "demonstração (sem vídeo)"), row(Icon::Info, "detalhes", "demonstração")};
            else
                d.rows = {row(t.icon, "abrir", "demonstração local")};
            break;
    }
    return d;
}

// ------------------------------------------------------------ estado/visual

struct MState {
    int category = 0;
    float pagePos = 0;
    int sel[8] = {};
    Rectf hl{kGroupX, kGroupY, 284, 284};
    float pageEnter = 1;  // animacao de entrada dos blocos da categoria atual
    float infoT = 1;
    float enter = 1;
    bool detailOpen = false;
    float detailAnim = 0;
    MDetail detail;
    int detailSel = 0;
    float detailSelAnim = 0;
    int bg = 0;
    int color = 0;
    std::set<int> pins;
    bool optionsOpen = false;
    float optionsAnim = 0;
    int optionsSel = 0;
    std::string clock = "12:00";
};

Color accentOf(const MState& st) { return Color::hex(kTileColors[st.color].rgb); }

void drawBackground(IRenderer& r, int bg) {
    const Rectf full{0, 0, float(kLogicalW), float(kLogicalH)};
    if (bg == 0) {
        r.fillGradientV(full, Color::hex(0xD7D9DC), Color::hex(0xC4C7CB));
        r.fillRadial(640, 330, 760, Color::hex(0xFCFCFC), Color::hex(0xFCFCFC, 0), full);
    } else {
        r.fillGradientV(full, Color::hex(0x9EA2A8), Color::hex(0x7C8086));
        r.fillRadial(640, 330, 760, Color::hex(0xC8CBCF), Color::hex(0xC8CBCF, 0), full);
    }
    r.fillRadial(640, 520, 520, Color(0, 0, 0, 24), Color(0, 0, 0, 0), Rectf{100, 480, 1080, 80});
}

void drawTile(IRenderer& r, const MTile& t, const Rectf& rc, Color accent, bool pinned) {
    float k = rc.h / kCell;  // escala aproximada
    float ks = std::min(rc.w, rc.h) / kCell;
    TextStyle ls;
    ls.size = 19 * std::min(1.2f, ks);
    ls.color = Color(255, 255, 255);
    switch (t.style) {
        case Style::Accent:
            r.fillRect(rc, accent);
            drawIcon(r, t.icon, rc.cx(), rc.y + rc.h * 0.4f, 52 * ks, Color(255, 255, 255), accent);
            break;
        case Style::Dark:
            r.fillRect(rc, Color::hex(0x1A1A1A));
            drawIcon(r, t.icon, rc.cx(), rc.y + rc.h * 0.4f, 52 * ks, accent, Color::hex(0x1A1A1A));
            break;
        case Style::Art:
            drawAbstractArt(r, rc, t.seed, t.c1, t.c2);
            r.fillGradientV(Rectf{rc.x, rc.bottom() - 54 * std::min(k, 1.5f), rc.w, 54 * std::min(k, 1.5f)}, Color(0, 0, 0, 0),
                            Color(0, 0, 0, 150));
            if (rc.h >= kCell * 1.5f) drawIcon(r, t.icon, rc.cx(), rc.cy() - 10, 70 * ks, Color(255, 255, 255, 170));
            break;
        case Style::Light: {
            r.fillGradientV(rc, Color::hex(0xC9DDF0), Color::hex(0x8DB4D8));
            AvatarLook look;
            r.pushClip(rc);
            drawAvatar(r, rc.x + rc.w * 0.68f, rc.bottom() + 6, rc.h * 0.95f, look, 0);
            r.popClip();
            const DemoProfile& p = demoProfile();
            TextStyle ts;
            ts.size = 26;
            ts.weight = FontWeight::Light;
            ts.color = Color::hex(0x24384A);
            r.drawText(p.gamertag, rc.x + 14, rc.y + 12, ts);
            ts.size = 18;
            std::string pts = std::to_string(p.points) + " pontos";
            r.drawText(pts.c_str(), rc.x + 14, rc.y + 46, ts);
            ls.color = Color::hex(0x24384A);
            break;
        }
    }
    r.pushClip(rc);
    r.drawText(t.label.c_str(), rc.x + 10, rc.bottom() - ls.size - 12, ls);
    r.popClip();
    if (pinned) {
        r.fillRect(Rectf{rc.right() - 30, rc.y, 30, 30}, Color(0, 0, 0, 90));
        drawIcon(r, Icon::Star, rc.right() - 15, rc.y + 15, 18, Color::hex(0xFFD23A));
    }
}

void drawHeader(IRenderer& r, const MState& st, float e) {
    const auto& cats = categories();
    TextStyle cs;
    cs.size = 38;
    cs.weight = FontWeight::Light;
    std::vector<float> xs(cats.size());
    float x = 0;
    for (size_t i = 0; i < cats.size(); ++i) {
        xs[i] = x;
        x += r.measureText(cats[i].name, cs) + 34;
    }
    int a = clampi(int(std::floor(st.pagePos)), 0, int(cats.size()) - 1);
    int b = clampi(a + 1, 0, int(cats.size()) - 1);
    float f = st.pagePos - a;
    float off = 70 - lerpf(xs[size_t(a)], xs[size_t(b)], f);
    r.pushAlpha(e);
    r.pushOffset((1 - e) * 80, 0);
    for (size_t i = 0; i < cats.size(); ++i) {
        float d = std::fabs(float(i) - st.pagePos);
        // inativas mais escuras no fundo cinza para manter o contraste
        cs.color = lerpColor(Color::hex(0x2E3134), st.bg == 0 ? Color::hex(0xA2A6AC) : Color::hex(0x55595F), clampf(d, 0, 1));
        r.drawText(cats[i].name, off + xs[i], 106, cs);
    }
    r.popOffset();
    r.popAlpha();
}

void drawDetail(IRenderer& r, const MState& st) {
    float a = ease::outCubic(st.detailAnim);
    if (a <= 0.01f) return;
    const MDetail& d = st.detail;
    const Color accent = accentOf(st);
    r.pushAlpha(a);
    r.fillRect(Rectf{0, 0, 1280, 720}, st.bg == 0 ? Color::hex(0xF2F3F4) : Color::hex(0xB4B8BC));
    r.pushOffset((1 - a) * 260, 0);
    TextStyle bc;
    bc.size = 22;
    bc.weight = FontWeight::Light;
    bc.color = Color::hex(0x8A8E94);
    r.drawText(d.category.c_str(), 72, 46, bc);
    TextStyle tt;
    tt.size = 60;
    tt.weight = FontWeight::Light;
    tt.color = Color::hex(0x34373C);
    r.drawText(d.title.c_str(), 68, 76, tt);
    Rectf art{72, 182, 520, 292};
    MTile big = d.tile;
    drawTile(r, big, art, accent, false);
    TextStyle ds;
    ds.size = 20;
    ds.weight = FontWeight::Light;
    ds.color = Color::hex(0x5A5E64);
    drawWrapped(r, d.desc.c_str(), 72, 492, 520, ds, 28, 3);
    const float rx = 650, ry = 182, rh = 64;
    for (int i = 0; i < int(d.rows.size()) && i < 7; ++i) {
        const InfoRow& rw = d.rows[size_t(i)];
        float y = ry + i * rh;
        bool sel = i == st.detailSel;
        if (sel) {
            r.fillRect(Rectf{rx - 16, y + 4, 6, rh - 12}, accent);
            r.fillRect(Rectf{rx - 10, y + 4, 560, rh - 12}, Color(0, 0, 0, 14));
        }
        TextStyle rt;
        rt.size = 28;
        rt.weight = sel ? FontWeight::Regular : FontWeight::Light;
        rt.color = sel ? Color::hex(0x24272C) : Color::hex(0x5A5E64);
        r.drawText(rw.title.c_str(), rx, y + 6, rt);
        TextStyle rs;
        rs.size = 15;
        rs.color = Color::hex(0x8A8E94);
        r.drawText(rw.sub.c_str(), rx + 1, y + 40, rs);
        if (rw.progress >= 0) {
            Rectf bar{rx + 380, y + 24, 150, 6};
            r.fillRect(bar, Color(0, 0, 0, 30));
            r.fillRect(Rectf{bar.x, bar.y, bar.w * clampf(rw.progress, 0, 1), bar.h}, accent);
        }
    }
    drawHints(r, {{Button::B, "voltar"}, {Button::A, "selecionar"}}, 72, 662, 30, Color::hex(0x5A5E64));
    r.popOffset();
    r.popAlpha();
}

void drawOptions(IRenderer& r, const MState& st) {
    float a = ease::outCubic(st.optionsAnim);
    if (a <= 0.01f) return;
    const Color accent = accentOf(st);
    r.pushAlpha(a);
    r.fillRect(Rectf{0, 0, 1280, 720}, Color(0, 0, 0, 120));
    // painel claro de largura total (estilo da geracao)
    Rectf box{0, 200 + (1 - a) * 30, 1280, 300};
    r.fillRect(box, Color::hex(0xF2F3F4));
    TextStyle tt;
    tt.size = 40;
    tt.weight = FontWeight::Light;
    tt.color = Color::hex(0x34373C);
    r.drawText("opções", 248, box.y + 26, tt);
    const char* labels[3] = {"fundo", "cor dos blocos", "fechar"};
    std::string vals[3] = {kBgNames[st.bg], kTileColors[st.color].name, ""};
    for (int i = 0; i < 3; ++i) {
        float y = box.y + 92 + i * 62.0f;
        bool sel = i == st.optionsSel;
        if (sel) r.fillRect(Rectf{232, y + 6, 6, 44}, accent);
        TextStyle ls;
        ls.size = 28;
        ls.weight = sel ? FontWeight::Regular : FontWeight::Light;
        ls.color = sel ? Color::hex(0x24272C) : Color::hex(0x6A6E74);
        r.drawText(labels[i], 248, y + 10, ls);
        if (i == 1) r.fillRect(Rectf{620, y + 14, 28, 28}, accent);
        ls.color = Color::hex(0x6A6E74);
        r.drawText(vals[i].c_str(), i == 1 ? 662 : 620, y + 10, ls);
    }
    r.popAlpha();
}

void renderMetro(IRenderer& r, const MState& st, TextureId bg) {
    const auto& cats = categories();
    const Color accent = accentOf(st);
    if (bg != kInvalidTexture) r.drawTexture(bg, Rectf{0, 0, float(kLogicalW), float(kLogicalH)});
    else drawBackground(r, st.bg);
    float e = ease::outCubic(st.enter);

    // Perfil e relogio
    TextStyle tag;
    tag.size = 22;
    tag.align = Align::Right;
    tag.color = Color::hex(0x5A5E64);
    std::string who = std::string(demoProfile().gamertag) + "   " + st.clock;
    r.drawText(who.c_str(), 1110, 50, tag);
    r.fillRect(Rectf{1122, 42, 40, 40}, Color::hex(0x6B8FB8));
    drawIcon(r, Icon::Person, 1142, 64, 30, Color(255, 255, 255, 230));

    drawHeader(r, st, e);

    // Grupos de blocos (pagina atual e vizinhas espiando nas bordas)
    for (int p = 0; p < int(cats.size()); ++p) {
        float d = p - st.pagePos;
        if (d < -1.6f || d > 1.6f) continue;
        bool current = p == st.category;
        float pageA = current ? 1.0f : 0.5f;
        if (!current) pageA *= clampf(1.6f - std::fabs(d), 0, 1);
        r.pushOffset(d * kPageSpacing, 0);
        const auto& tiles = cats[size_t(p)].tiles;
        for (int i = 0; i < int(tiles.size()); ++i) {
            if (current && i == st.sel[p]) continue;
            float local = 1;
            if (current) local = ease::outCubic(clampf((st.pageEnter - 0.05f * tiles[size_t(i)].col) / 0.55f, 0, 1));
            local *= ease::outCubic(clampf((st.enter - 0.04f * i) / 0.6f, 0, 1));
            Rectf rc = tileRect(tiles[size_t(i)]).offset((1 - local) * 120, 0);
            r.pushAlpha(pageA * local);
            drawTile(r, tiles[size_t(i)], rc, accent, st.pins.count(p * 100 + i) != 0);
            r.popAlpha();
        }
        r.popOffset();
    }
    // Bloco selecionado: borda branca destacada (desliza entre blocos)
    {
        int p = st.category;
        const auto& tiles = cats[size_t(p)].tiles;
        int i = clampi(st.sel[p], 0, int(tiles.size()) - 1);
        float d = p - st.pagePos;
        float local = ease::outCubic(clampf(st.pageEnter / 0.55f, 0, 1)) * ease::outCubic(clampf(st.enter / 0.6f, 0, 1));
        Rectf rc = st.hl.scaledAboutCenter(1.03f).offset(d * kPageSpacing + (1 - local) * 120, 0);
        r.pushAlpha(local);
        r.fillRect(rc.offset(5, 8), Color(0, 0, 0, 50));
        drawTile(r, tiles[size_t(i)], rc, accent, st.pins.count(p * 100 + i) != 0);
        r.strokeRect(rc.inset(-4), 4, Color(255, 255, 255));
        r.popAlpha();

        // Cartao informativo do bloco selecionado
        float it = ease::outCubic(st.infoT) * local;
        r.pushAlpha(it);
        TextStyle t;
        t.size = 26;
        t.color = Color::hex(0x34373C);
        r.drawText(tiles[size_t(i)].label.c_str(), kGroupX + (1 - it) * 30, 506, t);
        TextStyle ds;
        ds.size = 19;
        ds.weight = FontWeight::Light;
        ds.color = Color::hex(0x5A5E64);
        r.drawText(tiles[size_t(i)].desc.c_str(), kGroupX + (1 - it) * 30, 542, ds);
        r.popAlpha();
    }
    // indicador de pagina
    for (int i = 0; i < int(cats.size()); ++i)
        r.fillCircle(kGroupX + 6 + i * 12.0f, 590, i == st.category ? 4.0f : 2.5f,
                     i == st.category ? Color::hex(0x2E3134) : (st.bg == 0 ? Color::hex(0x9A9EA4) : Color::hex(0x55595F)));
    drawHints(r, {{Button::A, "selecionar"}, {Button::X, "fixar"}, {Button::Y, "opções"}, {Button::LB, "/"}, {Button::RB, "categorias"}},
              70, 662, 30, Color::hex(0x5A5E64));
    drawDetail(r, st);
    drawOptions(r, st);
}

// Navegacao espacial: bloco mais proximo na direcao (dx, dy).
int spatialNext(const std::vector<MTile>& tiles, int cur, int dx, int dy) {
    Rectf a = tileRect(tiles[size_t(cur)]);
    float best = 1e9f;
    int bestI = -1;
    for (int i = 0; i < int(tiles.size()); ++i) {
        if (i == cur) continue;
        Rectf b = tileRect(tiles[size_t(i)]);
        float px, perp;
        if (dx > 0) { if (b.x < a.right() - 1) continue; px = b.x - a.right(); perp = std::fabs(b.cy() - a.cy()); }
        else if (dx < 0) { if (b.right() > a.x + 1) continue; px = a.x - b.right(); perp = std::fabs(b.cy() - a.cy()); }
        else if (dy > 0) { if (b.y < a.bottom() - 1) continue; px = b.y - a.bottom(); perp = std::fabs(b.cx() - a.cx()); }
        else { if (b.bottom() > a.y + 1) continue; px = a.y - b.bottom(); perp = std::fabs(b.cx() - a.cx()); }
        // sobreposicao no eixo perpendicular conta como alinhado
        bool overlap = (dx != 0) ? (b.y < a.bottom() && b.bottom() > a.y) : (b.x < a.right() && b.right() > a.x);
        float score = px + (overlap ? 0 : 1000) + perp * 0.5f;
        if (score < best) { best = score; bestI = i; }
    }
    return bestI;
}

// ------------------------------------------------------------ dashboard

class MetroV1Dashboard : public Dashboard {
public:
    MetroV1Dashboard() : Dashboard(DashboardId::MetroV1) {}

    bool load(Context& ctx, std::string& error) override {
        if (!cache_.init(*ctx.renderer) || !rebuildBackground(ctx)) {
            error = "falha ao criar as texturas do Metro V1";
            return false;
        }
        return true;
    }

    void unload(Context& ctx) override {
        if (bg_ != kInvalidTexture) ctx.renderer->destroyTexture(bg_);
        bg_ = kInvalidTexture;
        cache_.release(*ctx.renderer);
        cacheBg_ = kInvalidTexture;
    }

    void onEnter(Context& ctx) override {
        enter_.start(0.8f);
        st_.enter = 0;
        pageEnter_.t = 1;
        moveHighlight(true);
        updateClock(ctx);
    }

    bool debugSelect(int group, int item) override {
        const auto& cats = categories();
        if (group < 0 || group >= int(cats.size()) || item < 0 || item >= int(cats[size_t(group)].tiles.size())) return false;
        st_.category = group;
        st_.sel[group] = item;
        page_.snap(float(group));
        moveHighlight(true);
        ++version_;
        return true;
    }

    std::string debugState() const override {
        char b[160];
        std::snprintf(b, sizeof(b), "category=%d sel=%d detail=%d detailSel=%d options=%d bg=%d color=%d pins=%d", st_.category,
                      st_.sel[st_.category], st_.detailOpen ? 1 : 0, st_.detailSel, st_.optionsOpen ? 1 : 0, st_.bg, st_.color,
                      int(st_.pins.size()));
        return b;
    }

    void update(Context& ctx, const InputFrame& in, float dt) override {
        enter_.update(dt);
        st_.enter = enter_.progress();
        page_.update(dt);
        st_.pagePos = page_.value;
        pageEnter_.update(dt);
        st_.pageEnter = pageEnter_.progress();
        info_.update(dt);
        st_.infoT = info_.progress();
        hx_.update(dt); hy_.update(dt); hw_.update(dt); hh_.update(dt);
        st_.hl = Rectf{hx_.value, hy_.value, hw_.value, hh_.value};
        detailAnim_.update(dt);
        st_.detailAnim = detailAnim_.value;
        optionsAnim_.update(dt);
        st_.optionsAnim = optionsAnim_.value;
        clockTimer_ -= dt;
        if (clockTimer_ <= 0) updateClock(ctx);

        for (int b = 0; b < kButtonCount; ++b)
            if (in.pressed[b]) { ++version_; break; }
        if (st_.optionsOpen) { updateOptions(ctx, in); return; }
        if (st_.detailOpen) { updateDetail(ctx, in); return; }
        updateTiles(ctx, in);
    }

    void render(Context& ctx, IRenderer& r) override {
        (void)ctx;
        cache_.draw(r, animating(), version_, [this, &r](IRenderer& rr) { renderMetro(rr, st_, &rr == &r ? bg_ : cacheBg_); });
    }

private:
    bool animating() const {
        return !enter_.done() || !page_.settled() || !pageEnter_.done() || !info_.done() || !hx_.settled() || !hy_.settled() ||
               !hw_.settled() || !hh_.settled() || !detailAnim_.settled() || !optionsAnim_.settled();
    }

    bool rebuildBackground(Context& ctx) {
        IRenderer& r = *ctx.renderer;
        if (bg_ != kInvalidTexture) r.destroyTexture(bg_);
        SoftRenderer canvas(r.pixelWidth(), r.pixelHeight());
        canvas.beginFrame();
        drawBackground(canvas, st_.bg);
        bg_ = r.createTexture(canvas.pixelWidth(), canvas.pixelHeight(), canvas.pixels());
        if (SoftRenderer* cv = cache_.canvas()) {
            if (cacheBg_ != kInvalidTexture) cv->destroyTexture(cacheBg_);
            cacheBg_ = cv->createTexture(canvas.pixelWidth(), canvas.pixelHeight(), canvas.pixels());
        }
        ++version_;
        return bg_ != kInvalidTexture;
    }

    void updateClock(Context& ctx) {
        int h = 0, m = 0, d = 1, mo = 1, y = 2026;
        ctx.platform->localTime(h, m, d, mo, y);
        char b[16];
        std::snprintf(b, sizeof(b), "%02d:%02d", h, m);
        if (st_.clock != b) ++version_;
        st_.clock = b;
        clockTimer_ = 1.0f;
    }

    void moveHighlight(bool snap) {
        const auto& tiles = categories()[size_t(st_.category)].tiles;
        Rectf rc = tileRect(tiles[size_t(st_.sel[st_.category])]);
        if (snap) { hx_.snap(rc.x); hy_.snap(rc.y); hw_.snap(rc.w); hh_.snap(rc.h); }
        else { hx_.set(rc.x); hy_.set(rc.y); hw_.set(rc.w); hh_.set(rc.h); }
    }

    void goCategory(Context& ctx, int nc, bool fromLeft, float rowY) {
        const auto& cats = categories();
        if (nc < 0 || nc >= int(cats.size())) { ctx.sfx(Sfx::Error, 0.6f); return; }
        st_.category = nc;
        // escolhe o bloco da borda de entrada mais proximo da linha atual
        const auto& tiles = cats[size_t(nc)].tiles;
        int best = 0;
        float bestScore = 1e9f;
        for (int i = 0; i < int(tiles.size()); ++i) {
            Rectf rc = tileRect(tiles[size_t(i)]);
            float edge = fromLeft ? rc.x : -rc.right();
            float score = edge * 2 + std::fabs(rc.cy() - rowY);
            if (score < bestScore) { bestScore = score; best = i; }
        }
        st_.sel[nc] = best;
        page_.set(float(nc));
        pageEnter_.start(0.5f);
        info_.start(0.3f);
        moveHighlight(true);
        ctx.sfx(Sfx::Tab);
    }

    void updateTiles(Context& ctx, const InputFrame& in) {
        const auto& cats = categories();
        int c = st_.category;
        const auto& tiles = cats[size_t(c)].tiles;
        int& sel = st_.sel[c];
        float rowY = tileRect(tiles[size_t(sel)]).cy();
        if (in.press(Button::RB)) { goCategory(ctx, c + 1, true, kGroupY + 70); return; }
        if (in.press(Button::LB)) { goCategory(ctx, c - 1, true, kGroupY + 70); return; }
        int dx = in.press(Button::Right) ? 1 : (in.press(Button::Left) ? -1 : 0);
        int dy = in.press(Button::Down) ? 1 : (in.press(Button::Up) ? -1 : 0);
        if (dx || dy) {
            int nx = spatialNext(tiles, sel, dx, dy);
            if (nx >= 0) {
                sel = nx;
                moveHighlight(false);
                info_.start(0.25f);
                ctx.sfx(Sfx::Navigate);
            } else if (dx > 0) {
                goCategory(ctx, c + 1, true, rowY);
                return;
            } else if (dx < 0) {
                goCategory(ctx, c - 1, false, rowY);
                return;
            } else {
                ctx.sfx(Sfx::Error, 0.4f);
            }
        }
        const MTile& t = tiles[size_t(sel)];
        if (in.press(Button::A)) {
            switch (t.kind) {
                case Kind::OpenSettings: ctx.sfx(Sfx::Confirm); ctx.app->openSettings(); break;
                case Kind::OpenAbout: ctx.sfx(Sfx::Confirm); ctx.app->openAbout(); break;
                case Kind::OpenSounds: ctx.sfx(Sfx::Confirm); ctx.app->openSoundStatus(); break;
                default:
                    st_.detail = buildDetail(cats[size_t(c)], t, &ctx);
                    st_.detailOpen = true;
                    st_.detailSel = 0;
                    detailAnim_.set(1);
                    ctx.sfx(Sfx::Confirm);
                    break;
            }
        }
        if (in.press(Button::X)) {
            int key = c * 100 + sel;
            if (st_.pins.count(key)) { st_.pins.erase(key); ctx.app->toast(t.label + ": desafixado"); }
            else { st_.pins.insert(key); ctx.app->toast(t.label + ": fixado"); }
            ctx.sfx(Sfx::Confirm, 0.8f);
        }
        if (in.press(Button::Y)) { st_.optionsOpen = true; st_.optionsSel = 0; optionsAnim_.set(1); ctx.sfx(Sfx::Open); }
        if (in.press(Button::B)) {
            if (sel != 0) { sel = 0; moveHighlight(false); info_.start(0.25f); ctx.sfx(Sfx::Back); }
            else if (c != 0) goCategory(ctx, 0, true, kGroupY + 70);
            else { ctx.sfx(Sfx::Back, 0.6f); ctx.app->toast("Pressione START para o menu do Neutro X"); }
        }
    }

    void updateDetail(Context& ctx, const InputFrame& in) {
        int n = std::min(7, int(st_.detail.rows.size()));
        int before = st_.detailSel;
        if (in.press(Button::Down) && st_.detailSel + 1 < n) st_.detailSel++;
        if (in.press(Button::Up) && st_.detailSel > 0) st_.detailSel--;
        if (before != st_.detailSel) ctx.sfx(Sfx::Navigate);
        if (in.press(Button::A) && n > 0) {
            const InfoRow& rw = st_.detail.rows[size_t(st_.detailSel)];
            if (rw.action == kActPlay) {
                ctx.sfx(Sfx::Error, 0.7f);
                ctx.app->toast("Jogos de demonstração não são executados pelo Neutro X");
            } else {
                ctx.sfx(Sfx::Open, 0.8f);
                ctx.app->toast(rw.title + ": item de demonstração (somente informativo)");
            }
        }
        if (in.press(Button::B)) {
            st_.detailOpen = false;
            detailAnim_.set(0);
            ctx.sfx(Sfx::Back);
        }
    }

    void updateOptions(Context& ctx, const InputFrame& in) {
        if (in.press(Button::Down)) { st_.optionsSel = (st_.optionsSel + 1) % 3; ctx.sfx(Sfx::Navigate); }
        if (in.press(Button::Up)) { st_.optionsSel = (st_.optionsSel + 2) % 3; ctx.sfx(Sfx::Navigate); }
        int delta = (in.press(Button::Right) || in.press(Button::A)) ? 1 : (in.press(Button::Left) ? -1 : 0);
        if (delta != 0 && st_.optionsSel == 0) {
            st_.bg = (st_.bg + 1) % 2;
            rebuildBackground(ctx);
            ctx.sfx(Sfx::Tab);
        } else if (delta != 0 && st_.optionsSel == 1) {
            st_.color = (st_.color + delta + kTileColorCount) % kTileColorCount;
            ctx.sfx(Sfx::Tab);
        } else if ((in.press(Button::A) && st_.optionsSel == 2) || in.press(Button::B) || in.press(Button::Y)) {
            st_.optionsOpen = false;
            optionsAnim_.set(0);
            ctx.sfx(Sfx::Back);
        }
    }

    MState st_;
    Tween enter_, pageEnter_, info_;
    Smooth page_{0, 9};
    Smooth hx_{kGroupX, 18}, hy_{kGroupY, 18}, hw_{284, 18}, hh_{284, 18};
    Smooth detailAnim_{0, 12};
    Smooth optionsAnim_{0, 14};
    float clockTimer_ = 0;
    SceneCache cache_;
    TextureId bg_ = kInvalidTexture, cacheBg_ = kInvalidTexture;
    unsigned version_ = 1;
};

}  // namespace

void drawMetroV1Preview(IRenderer& r, float time) {
    (void)time;
    MState st;
    st.category = 4;
    st.pagePos = 4;
    st.sel[4] = 2;
    st.hl = tileRect(categories()[4].tiles[2]);
    renderMetro(r, st, kInvalidTexture);
}

std::unique_ptr<Dashboard> createMetroV1Dashboard() { return std::unique_ptr<Dashboard>(new MetroV1Dashboard()); }

}  // namespace nx
