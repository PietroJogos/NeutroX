// Neutro X - Dashboard "Metro V2" (inspirado na geracao final 2013-2019, referencia 2.0.17559.0).
//
// Recriacao original: fundo escuro, destaques verdes, cinco categorias e grade
// compacta de blocos em tres linhas, com navegacao espacial rapida. Inclui:
//   - Biblioteca de jogos demonstrativa com filtros por categoria, grade de
//     miniaturas locais e tela informativa de cada jogo;
//   - Paineis de informacoes locais (valores reais do aparelho/aplicativo);
//   - Atalhos para as configuracoes do proprio Neutro X.
// Toda a arte e desenhada por codigo. Nenhum recurso dos executaveis originais e usado.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
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

enum class Kind { Detail, Game, Library, LibraryAZ, InfoPanel, Sounds, Settings, About, GoCategory };
enum class Style { Accent, Art, Dark };

struct VTile {
    int col, row, w, h;
    Icon icon;
    std::string label;
    Kind kind;
    int index;  // jogo, categoria destino ou id de detalhe
    Style style;
    int seed;
    Color c1, c2;
};

struct VCategory {
    const char* name;
    std::vector<VTile> tiles;
};

VTile t(int c, int r, int w, int h, Icon ic, std::string label, Kind k, int idx, Style st = Style::Accent) {
    return VTile{c, r, w, h, ic, std::move(label), k, idx, st, 0, Color(), Color()};
}
VTile art(int c, int r, int w, int h, Icon ic, std::string label, Kind k, int idx, int seed, Color c1, Color c2) {
    return VTile{c, r, w, h, ic, std::move(label), k, idx, Style::Art, seed, c1, c2};
}
VTile game(int c, int r, int w, int h, int gi, const char* prefix = "") {
    int n = 0;
    const DemoGame& g = demoGames(n)[gi];
    return art(c, r, w, h, g.arcade ? Icon::Star : Icon::Controller, std::string(prefix) + g.title, Kind::Game, gi, g.artSeed,
               g.c1, g.c2);
}

std::vector<VCategory> buildCategories() {
    std::vector<VCategory> c(5);
    int vn = 0;
    const DemoMedia* vi = demoVideos(vn);
    c[0].name = "início";
    c[0].tiles = {game(0, 0, 2, 2, 0, "Em destaque: "),
                  t(2, 0, 1, 1, Icon::Controller, "Meus jogos", Kind::Library, 0),
                  t(2, 1, 1, 1, Icon::List, "Biblioteca", Kind::Library, 0, Style::Dark),
                  t(0, 2, 1, 1, Icon::Person, "Perfil", Kind::Detail, 1),
                  t(1, 2, 1, 1, Icon::Friends, "Amigos", Kind::Detail, 2),
                  game(3, 0, 2, 1, 1, "Jogado recentemente: "),
                  t(3, 1, 1, 1, Icon::Mail, "Mensagens", Kind::Detail, 3, Style::Dark),
                  t(4, 1, 1, 1, Icon::Info, "Informações", Kind::InfoPanel, 0),
                  t(2, 2, 2, 1, Icon::Trophy, "Conquistas", Kind::Detail, 4),
                  t(4, 2, 1, 1, Icon::Gear, "Configurações", Kind::GoCategory, 4, Style::Dark)};
    c[1].name = "jogos";
    c[1].tiles = {t(0, 0, 1, 1, Icon::Controller, "Meus jogos", Kind::Library, 0),
                  t(1, 0, 1, 1, Icon::Plus, "Complementos", Kind::Detail, 5),
                  t(0, 1, 1, 1, Icon::Bag, "Explorar jogos", Kind::Library, 0),
                  t(1, 1, 1, 1, Icon::Download, "Demos", Kind::Detail, 6),
                  t(0, 2, 1, 1, Icon::Search, "Pesquisar", Kind::LibraryAZ, 0),
                  t(1, 2, 1, 1, Icon::List, "A - Z", Kind::LibraryAZ, 0),
                  game(2, 0, 2, 2, 1),
                  game(2, 2, 1, 1, 2),
                  game(3, 2, 1, 1, 3),
                  game(4, 0, 1, 1, 4),
                  game(4, 1, 1, 1, 5),
                  t(4, 2, 1, 1, Icon::Apps, "Biblioteca completa", Kind::Library, 0, Style::Dark)};
    c[2].name = "filmes e tv";
    c[2].tiles = {t(0, 0, 1, 1, Icon::Video, "Meus vídeos", Kind::Detail, 10),
                  t(0, 1, 1, 1, Icon::Music, "Música", Kind::Detail, 11),
                  art(1, 0, 2, 2, Icon::Video, vn > 1 ? vi[1].title : "Vídeo", Kind::Detail, 12, 17559, Color::hex(0xE8E8E8),
                      Color::hex(0x5A5A5A)),
                  art(3, 0, 1, 1, Icon::Tv, "Canal Neutro", Kind::Detail, 13, 1751, Color::hex(0x5A6070), Color::hex(0x15181E)),
                  t(3, 1, 1, 1, Icon::Tv, "Séries", Kind::Detail, 14),
                  t(1, 2, 2, 1, Icon::Video, "Trailers", Kind::Detail, 15, Style::Dark),
                  t(0, 2, 1, 1, Icon::Search, "Pesquisar", Kind::Detail, 16)};
    c[3].name = "aplicativos";
    c[3].tiles = {t(0, 0, 1, 1, Icon::Clock, "Relógio", Kind::InfoPanel, 0),
                  t(1, 0, 1, 1, Icon::Globe, "Clima", Kind::Detail, 20),
                  t(0, 1, 1, 1, Icon::Picture, "Fotos", Kind::Detail, 21, Style::Dark),
                  t(1, 1, 1, 1, Icon::List, "Notas", Kind::Detail, 22),
                  t(2, 0, 2, 1, Icon::Apps, "Todos os aplicativos", Kind::Detail, 23),
                  t(2, 1, 1, 1, Icon::Info, "Informações", Kind::InfoPanel, 0, Style::Dark),
                  t(3, 1, 1, 1, Icon::Plus, "Calculadora", Kind::Detail, 24),
                  t(0, 2, 2, 1, Icon::Globe, "Navegador", Kind::Detail, 25, Style::Dark)};
    c[4].name = "configurações";
    c[4].tiles = {t(0, 0, 1, 1, Icon::Gear, "Sistema", Kind::InfoPanel, 0),
                  t(1, 0, 1, 1, Icon::Person, "Perfil", Kind::Detail, 1),
                  t(0, 1, 1, 1, Icon::Family, "Família", Kind::Detail, 30, Style::Dark),
                  t(1, 1, 1, 1, Icon::Storage, "Armazenamento", Kind::InfoPanel, 1),
                  t(0, 2, 1, 1, Icon::Speaker, "Sons originais", Kind::Sounds, 0),
                  t(2, 0, 2, 1, Icon::Palette, "Neutro X", Kind::Settings, 0),
                  t(2, 1, 2, 1, Icon::Info, "Sobre o Neutro X", Kind::About, 0, Style::Dark),
                  t(1, 2, 1, 1, Icon::Globe, "Idioma", Kind::Detail, 31)};
    return c;
}

const std::vector<VCategory>& categories() {
    static const std::vector<VCategory> c = buildCategories();
    return c;
}

struct Accent {
    const char* name;
    unsigned base, hi;
};
const Accent kAccents[] = {{"verde", 0x1F8A1F, 0x2BA52B}, {"azul", 0x1F6FC8, 0x3088E0}, {"laranja", 0xC85A12, 0xE0702A}};
constexpr int kAccentCount = int(sizeof(kAccents) / sizeof(kAccents[0]));
const char* const kBgNames[] = {"escuro", "preto"};

constexpr float kCellW = 162, kCellH = 122, kGap = 6, kGX = 150, kGY = 166, kSpacing = 1100;

Rectf tileRect(const VTile& v) {
    return Rectf{kGX + v.col * (kCellW + kGap), kGY + v.row * (kCellH + kGap), v.w * kCellW + (v.w - 1) * kGap,
                 v.h * kCellH + (v.h - 1) * kGap};
}

// ------------------------------------------------------------ biblioteca

struct LibFilter {
    const char* name;
    const char* genre;  // nullptr = todos; "*arcade" = arcade
    bool az;
};
const LibFilter kFilters[] = {{"todos", nullptr, false},     {"ação", "Ação", false},           {"aventura", "Aventura", false},
                              {"corrida", "Corrida", false},  {"quebra-cabeça", "Quebra-cabeça", false},
                              {"simulação", "Simulação", false}, {"arcade", "*arcade", false},   {"a–z", nullptr, true}};
constexpr int kFilterCount = int(sizeof(kFilters) / sizeof(kFilters[0]));

std::vector<int> libraryGames(int filter) {
    int n = 0;
    const DemoGame* g = demoGames(n);
    std::vector<int> out;
    const LibFilter& f = kFilters[filter];
    for (int i = 0; i < n; ++i) {
        if (f.genre && std::strcmp(f.genre, "*arcade") == 0) { if (g[i].arcade) out.push_back(i); }
        else if (f.genre) { if (std::strcmp(g[i].genre, f.genre) == 0) out.push_back(i); }
        else out.push_back(i);
    }
    if (f.az) std::sort(out.begin(), out.end(), [g](int a, int b) { return std::strcmp(g[a].title, g[b].title) < 0; });
    return out;
}

constexpr int kLibCols = 4;
Rectf libRect(int i) {
    int col = i % kLibCols, row = i / kLibCols;
    return Rectf{150 + col * 250.0f, 210 + row * 196.0f, 238, 184};
}

// ------------------------------------------------------------ detalhes

enum RowAct { kInfo = 0, kPlay = 1 };

struct VDetail {
    std::string category, title, subtitle, desc;
    Icon icon = Icon::Info;
    int seed = 0;
    Color c1, c2;
    std::vector<InfoRow> rows;
};

InfoRow row(Icon ic, std::string tt, std::string s, int act = kInfo, float prog = -1) {
    InfoRow r;
    r.icon = int(ic);
    r.title = std::move(tt);
    r.sub = std::move(s);
    r.action = act;
    r.progress = prog;
    return r;
}

VDetail gameDetail(int gi, const char* category) {
    int n = 0;
    const DemoGame& g = demoGames(n)[gi];
    VDetail d;
    d.category = category;
    d.title = g.title;
    d.subtitle = std::string(g.genre) + " · " + g.year + (g.arcade ? " · arcade" : "");
    d.desc = g.description;
    d.icon = g.arcade ? Icon::Star : Icon::Controller;
    d.seed = g.artSeed;
    d.c1 = g.c1;
    d.c2 = g.c2;
    d.rows = {row(Icon::Controller, "jogar", "demonstração (não executável)", kPlay),
              row(Icon::Trophy, "conquistas", std::to_string(g.points) + " de " + std::to_string(g.maxPoints) + " pontos", kInfo,
                  float(g.points) / g.maxPoints),
              row(Icon::Plus, "complementos", "nenhum (demonstração)"), row(Icon::Info, "detalhes", d.subtitle)};
    return d;
}

VDetail tileDetail(const VCategory& cat, const VTile& tl) {
    VDetail d;
    d.category = cat.name;
    d.title = tl.label;
    d.icon = tl.icon;
    d.seed = tl.style == Style::Art ? tl.seed : 0;
    d.c1 = tl.c1;
    d.c2 = tl.c2;
    int n = 0;
    const DemoGame* g = demoGames(n);
    const DemoProfile& p = demoProfile();
    switch (tl.index) {
        case 1:
            d.desc = "Perfil local de demonstração. Nenhuma conta online é usada.";
            d.rows = {row(Icon::Person, p.gamertag, "nome do jogador"), row(Icon::Trophy, std::to_string(p.points) + " pontos", "pontuação"),
                      row(Icon::Mail, p.motto, "lema"), row(Icon::Globe, p.zone, "zona")};
            break;
        case 2: {
            int fn = 0;
            const DemoFriend* f = demoFriends(fn);
            d.desc = "Modo offline: amigos de demonstração.";
            for (int i = 0; i < fn; ++i) d.rows.push_back(row(Icon::Person, f[i].name, f[i].status));
            break;
        }
        case 3:
            d.desc = "Caixa de mensagens local.";
            d.rows = {row(Icon::Mail, "Bem-vindo ao Neutro X", "mensagem do sistema · lida")};
            break;
        case 4:
            d.desc = "Conquistas dos jogos de demonstração.";
            for (int i = 0; i < n && i < 6; ++i)
                d.rows.push_back(row(Icon::Trophy, g[i].title, std::to_string(g[i].points) + " / " + std::to_string(g[i].maxPoints), kInfo,
                                     float(g[i].points) / g[i].maxPoints));
            break;
        case 31:
            d.desc = "O Neutro X está disponível em português do Brasil.";
            d.rows = {row(Icon::Globe, "português (Brasil)", "idioma em uso")};
            break;
        case 30:
            d.desc = "Representação visual.";
            d.rows = {row(Icon::Family, "controle dos pais", "não configurado (visual)"),
                      row(Icon::Clock, "temporizador de família", "desativado (visual)")};
            break;
        default:
            d.desc = "Conteúdo de demonstração local do Neutro X. Nada é baixado nem transmitido.";
            if (tl.index >= 10 && tl.index < 20)
                d.rows = {row(Icon::Video, "assistir", "demonstração (sem vídeo)"), row(Icon::Info, "detalhes", "demonstração")};
            else
                d.rows = {row(tl.icon, "abrir", "demonstração local"), row(Icon::Info, "detalhes", "demonstração")};
            break;
    }
    return d;
}

// ------------------------------------------------------------ estado

struct VState {
    int category = 0;
    float pagePos = 0;
    int sel[5] = {};
    Rectf hl{kGX, kGY, 330, 250};
    float ulX = 150, ulW = 60;  // sublinhado da categoria
    float pageEnter = 1, enter = 1;
    // biblioteca
    bool libOpen = false;
    float libAnim = 0;
    int filter = 0;
    float filterX = 0, filterW = 0;
    int libSel = 0;
    Rectf libHl = libRect(0);
    // detalhe
    bool detailOpen = false;
    bool detailFromLib = false;
    float detailAnim = 0;
    VDetail detail;
    int detailSel = 0;
    // painel de informacoes
    bool infoOpen = false;
    float infoAnim = 0;
    int infoFocus = 0;
    std::vector<std::pair<std::string, std::string>> info;
    // opcoes
    bool optionsOpen = false;
    float optionsAnim = 0;
    int optionsSel = 0;
    int bg = 0, accent = 0;
    std::set<int> pins;
    std::string clock = "12:00";
};

Color accentBase(const VState& st) { return Color::hex(kAccents[st.accent].base); }
Color accentHi(const VState& st) { return Color::hex(kAccents[st.accent].hi); }

void drawBackground(IRenderer& r, int bg) {
    const Rectf full{0, 0, float(kLogicalW), float(kLogicalH)};
    if (bg == 0) {
        r.fillGradientV(full, Color::hex(0x3A3D42), Color::hex(0x1B1D20));
        r.fillRadial(560, 200, 700, Color(255, 255, 255, 22), Color(255, 255, 255, 0), full);
    } else {
        r.fillGradientV(full, Color::hex(0x18191B), Color::hex(0x08090A));
        r.fillRadial(560, 200, 700, Color(255, 255, 255, 10), Color(255, 255, 255, 0), full);
    }
}

void drawTile(IRenderer& r, const VTile& v, const Rectf& rc, const VState& st, bool selected) {
    const Color base = selected ? accentHi(st) : accentBase(st);
    float ks = std::min(rc.w / kCellW, rc.h / kCellH);
    switch (v.style) {
        case Style::Accent:
            r.fillRect(rc, base);
            drawIcon(r, v.icon, rc.cx(), rc.y + rc.h * 0.4f, 40 * std::min(ks, 1.6f), Color(255, 255, 255), base);
            break;
        case Style::Dark: {
            Color d = selected ? Color::hex(0x3A3E44) : Color::hex(0x2B2E33);
            r.fillRect(rc, d);
            drawIcon(r, v.icon, rc.cx(), rc.y + rc.h * 0.4f, 40 * std::min(ks, 1.6f), accentHi(st), d);
            break;
        }
        case Style::Art:
            drawAbstractArt(r, rc, v.seed, v.c1, v.c2);
            r.fillGradientV(Rectf{rc.x, rc.bottom() - 52, rc.w, 52}, Color(0, 0, 0, 0), Color(0, 0, 0, 170));
            break;
    }
    TextStyle ls;
    ls.size = 18;
    ls.color = Color(255, 255, 255);
    r.pushClip(rc);
    r.drawText(v.label.c_str(), rc.x + 10, rc.bottom() - 30, ls);
    r.popClip();
    (void)st;
}

void drawTopBar(IRenderer& r, const VState& st) {
    TextStyle tag;
    tag.size = 22;
    tag.align = Align::Right;
    tag.color = Color(225, 228, 232);
    std::string who = std::string(demoProfile().gamertag) + "  ·  " + st.clock;
    r.drawText(who.c_str(), 1130, 46, tag);
    r.fillRect(Rectf{1142, 38, 40, 40}, Color::hex(0x3E8FD0));
    drawIcon(r, Icon::Person, 1162, 60, 30, Color(255, 255, 255, 230));
}

void drawHeader(IRenderer& r, const VState& st, float e) {
    const auto& cats = categories();
    TextStyle cs;
    cs.size = 34;
    r.pushAlpha(e);
    float x = 150;
    for (int i = 0; i < int(cats.size()); ++i) {
        bool cur = i == st.category;
        cs.weight = cur ? FontWeight::Regular : FontWeight::Light;
        cs.color = cur ? Color(255, 255, 255) : Color(140, 145, 152);
        r.drawText(cats[size_t(i)].name, x, 96, cs);
        x += r.measureText(cats[size_t(i)].name, cs) + 40;
    }
    r.fillRect(Rectf{st.ulX, 142, st.ulW, 3}, accentHi(st));
    r.popAlpha();
}

void drawRowsList(IRenderer& r, const std::vector<InfoRow>& rows, int sel, float x, float y, float w, const VState& st) {
    for (int i = 0; i < int(rows.size()) && i < 7; ++i) {
        const InfoRow& rw = rows[size_t(i)];
        float yy = y + i * 64.0f;
        bool s = i == sel;
        if (s) {
            r.fillRect(Rectf{x - 14, yy + 2, w, 56}, Color(255, 255, 255, 18));
            r.fillRect(Rectf{x - 14, yy + 2, 5, 56}, accentHi(st));
        }
        TextStyle tt;
        tt.size = 26;
        tt.weight = s ? FontWeight::Regular : FontWeight::Light;
        tt.color = s ? Color(255, 255, 255) : Color(196, 200, 206);
        r.drawText(rw.title.c_str(), x, yy + 6, tt);
        TextStyle ss;
        ss.size = 15;
        ss.color = Color(140, 145, 152);
        r.drawText(rw.sub.c_str(), x + 1, yy + 38, ss);
        if (rw.progress >= 0) {
            Rectf bar{x + w - 180, yy + 24, 150, 6};
            r.fillRect(bar, Color(255, 255, 255, 30));
            r.fillRect(Rectf{bar.x, bar.y, bar.w * clampf(rw.progress, 0, 1), bar.h}, accentHi(st));
        }
    }
}

void drawDetail(IRenderer& r, const VState& st) {
    float a = ease::outCubic(st.detailAnim);
    if (a <= 0.01f) return;
    const VDetail& d = st.detail;
    r.pushAlpha(a);
    r.fillRect(Rectf{0, 0, 1280, 720}, st.bg == 0 ? Color::hex(0x1E2024) : Color::hex(0x0A0B0C));
    r.pushOffset((1 - a) * 240, 0);
    TextStyle bc;
    bc.size = 20;
    bc.weight = FontWeight::Light;
    bc.color = Color(140, 145, 152);
    r.drawText(d.category.c_str(), 152, 48, bc);
    TextStyle tt;
    tt.size = 54;
    tt.weight = FontWeight::Light;
    tt.color = Color(255, 255, 255);
    r.drawText(d.title.c_str(), 148, 74, tt);
    if (!d.subtitle.empty()) {
        TextStyle ss;
        ss.size = 20;
        ss.color = accentHi(st);
        r.drawText(d.subtitle.c_str(), 152, 140, ss);
    }
    Rectf ar{150, 186, 500, 282};
    if (d.seed) drawAbstractArt(r, ar, d.seed, d.c1, d.c2);
    else r.fillRect(ar, accentBase(st));
    drawIcon(r, d.icon, ar.cx(), ar.cy(), 100, Color(255, 255, 255, 210), d.seed ? Color(0, 0, 0, 0) : accentBase(st));
    TextStyle ds;
    ds.size = 19;
    ds.weight = FontWeight::Light;
    ds.color = Color(196, 200, 206);
    drawWrapped(r, d.desc.c_str(), 150, 486, 500, ds, 27, 3);
    drawRowsList(r, d.rows, st.detailSel, 712, 186, 470, st);
    drawHints(r, {{Button::B, "voltar"}, {Button::A, "selecionar"}}, 150, 662, 30, Color(225, 228, 232));
    r.popOffset();
    r.popAlpha();
}

void drawLibrary(IRenderer& r, const VState& st) {
    float a = ease::outCubic(st.libAnim);
    if (a <= 0.01f) return;
    int n = 0;
    const DemoGame* g = demoGames(n);
    r.pushAlpha(a);
    r.fillRect(Rectf{0, 0, 1280, 720}, st.bg == 0 ? Color::hex(0x1E2024) : Color::hex(0x0A0B0C));
    r.pushOffset((1 - a) * 300, 0);
    TextStyle tt;
    tt.size = 48;
    tt.weight = FontWeight::Light;
    tt.color = Color(255, 255, 255);
    r.drawText("biblioteca de jogos", 148, 44, tt);
    drawTopBar(r, st);
    // filtros
    TextStyle fs;
    fs.size = 22;
    float x = 150;
    for (int i = 0; i < kFilterCount; ++i) {
        bool cur = i == st.filter;
        fs.weight = cur ? FontWeight::Regular : FontWeight::Light;
        fs.color = cur ? Color(255, 255, 255) : Color(140, 145, 152);
        r.drawText(kFilters[i].name, x, 132, fs);
        x += r.measureText(kFilters[i].name, fs) + 30;
    }
    r.fillRect(Rectf{st.filterX, 164, st.filterW, 3}, accentHi(st));
    std::vector<int> games = libraryGames(st.filter);
    TextStyle cnt;
    cnt.size = 16;
    cnt.color = Color(140, 145, 152);
    std::string c = std::to_string(games.size()) + (games.size() == 1 ? " jogo" : " jogos");
    r.drawText(c.c_str(), 150, 180, cnt);
    for (int i = 0; i < int(games.size()); ++i) {
        if (i == st.libSel) continue;
        const DemoGame& gm = g[games[size_t(i)]];
        Rectf rc = libRect(i);
        drawAbstractArt(r, rc, gm.artSeed, gm.c1, gm.c2);
        r.fillGradientV(Rectf{rc.x, rc.bottom() - 56, rc.w, 56}, Color(0, 0, 0, 0), Color(0, 0, 0, 180));
        TextStyle ls;
        ls.size = 19;
        ls.color = Color(255, 255, 255);
        r.drawText(gm.title, rc.x + 10, rc.bottom() - 32, ls);
    }
    if (!games.empty()) {
        int si = clampi(st.libSel, 0, int(games.size()) - 1);
        const DemoGame& gm = g[games[size_t(si)]];
        Rectf rc = st.libHl.scaledAboutCenter(1.06f);
        r.fillRect(rc.offset(6, 9), Color(0, 0, 0, 110));
        drawAbstractArt(r, rc, gm.artSeed, gm.c1, gm.c2);
        r.fillGradientV(Rectf{rc.x, rc.bottom() - 60, rc.w, 60}, Color(0, 0, 0, 0), Color(0, 0, 0, 190));
        drawIcon(r, gm.arcade ? Icon::Star : Icon::Controller, rc.cx(), rc.cy() - 14, 56, Color(255, 255, 255, 200));
        TextStyle ls;
        ls.size = 20;
        ls.color = Color(255, 255, 255);
        r.drawText(gm.title, rc.x + 10, rc.bottom() - 34, ls);
        r.strokeRect(rc.inset(-3), 3, accentHi(st));
        TextStyle info;
        info.size = 18;
        info.weight = FontWeight::Light;
        info.color = Color(196, 200, 206);
        std::string line = std::string(gm.genre) + " · " + gm.year + " · " + gm.description;
        r.drawText(line.c_str(), 150, 610, info);
    }
    drawHints(r, {{Button::LB, "/"}, {Button::RB, "filtro"}, {Button::A, "informações"}, {Button::B, "voltar"}}, 150, 664, 30,
              Color(225, 228, 232));
    r.popOffset();
    r.popAlpha();
}

void drawInfoPanel(IRenderer& r, const VState& st) {
    float a = ease::outCubic(st.infoAnim);
    if (a <= 0.01f) return;
    r.pushAlpha(a);
    r.fillRect(Rectf{0, 0, 1280, 720}, Color(0, 0, 0, 150));
    Rectf p{120, 70 + (1 - a) * 40, 1040, 580};
    r.fillRect(p, st.bg == 0 ? Color::hex(0x24272B) : Color::hex(0x101113));
    r.fillRect(Rectf{p.x, p.y, 6, p.h}, accentHi(st));
    TextStyle tt;
    tt.size = 40;
    tt.weight = FontWeight::Light;
    tt.color = Color(255, 255, 255);
    r.drawText(st.infoFocus == 1 ? "armazenamento e sistema" : "informações locais", p.x + 40, p.y + 26, tt);
    TextStyle sub;
    sub.size = 16;
    sub.color = Color(140, 145, 152);
    r.drawText("Valores reais deste aparelho e do Neutro X. Somente leitura.", p.x + 42, p.y + 80, sub);
    for (size_t i = 0; i < st.info.size(); ++i) {
        int col = int(i) % 2, rw = int(i) / 2;
        float x = p.x + 40 + col * 500, y = p.y + 124 + rw * 78.0f;
        TextStyle k;
        k.size = 16;
        k.color = Color(140, 145, 152);
        r.drawText(st.info[i].first.c_str(), x, y, k);
        TextStyle v;
        v.size = 22;
        v.color = Color(235, 238, 242);
        r.pushClip(Rectf{x, y + 20, 470, 34});
        r.drawText(st.info[i].second.c_str(), x, y + 24, v);
        r.popClip();
    }
    drawHints(r, {{Button::B, "fechar"}}, p.x + 40, p.bottom() - 34, 30, Color(225, 228, 232));
    r.popAlpha();
}

void drawOptions(IRenderer& r, const VState& st) {
    float a = ease::outCubic(st.optionsAnim);
    if (a <= 0.01f) return;
    r.pushAlpha(a);
    r.fillRect(Rectf{0, 0, 1280, 720}, Color(0, 0, 0, 140));
    Rectf box{0, 200 + (1 - a) * 30, 1280, 300};
    r.fillRect(box, Color::hex(0x24272B));
    TextStyle tt;
    tt.size = 40;
    tt.weight = FontWeight::Light;
    tt.color = Color(255, 255, 255);
    r.drawText("opções", 150, box.y + 26, tt);
    const char* labels[3] = {"fundo", "destaque", "fechar"};
    std::string vals[3] = {kBgNames[st.bg], kAccents[st.accent].name, ""};
    for (int i = 0; i < 3; ++i) {
        float y = box.y + 92 + i * 62.0f;
        bool s = i == st.optionsSel;
        if (s) r.fillRect(Rectf{134, y + 6, 5, 44}, accentHi(st));
        TextStyle ls;
        ls.size = 28;
        ls.weight = s ? FontWeight::Regular : FontWeight::Light;
        ls.color = s ? Color(255, 255, 255) : Color(170, 175, 182);
        r.drawText(labels[i], 150, y + 10, ls);
        if (i == 1) r.fillRect(Rectf{520, y + 14, 28, 28}, accentHi(st));
        ls.color = Color(170, 175, 182);
        r.drawText(vals[i].c_str(), i == 1 ? 562 : 520, y + 10, ls);
    }
    r.popAlpha();
}

void renderMetroV2(IRenderer& r, const VState& st, TextureId bg) {
    const auto& cats = categories();
    if (bg != kInvalidTexture) r.drawTexture(bg, Rectf{0, 0, float(kLogicalW), float(kLogicalH)});
    else drawBackground(r, st.bg);
    float e = ease::outCubic(st.enter);
    drawTopBar(r, st);
    drawHeader(r, st, e);
    for (int p = 0; p < int(cats.size()); ++p) {
        float d = p - st.pagePos;
        if (d < -1.5f || d > 1.5f) continue;
        bool cur = p == st.category;
        float pageA = cur ? 1.0f : 0.4f * clampf(1.5f - std::fabs(d), 0, 1);
        r.pushOffset(d * kSpacing, 0);
        const auto& tiles = cats[size_t(p)].tiles;
        for (int i = 0; i < int(tiles.size()); ++i) {
            if (cur && i == st.sel[p]) continue;
            float local = 1;
            if (cur) local = ease::outCubic(clampf((st.pageEnter - 0.04f * tiles[size_t(i)].col) / 0.45f, 0, 1));
            local *= ease::outCubic(clampf((st.enter - 0.03f * i) / 0.55f, 0, 1));
            Rectf rc = tileRect(tiles[size_t(i)]).offset((1 - local) * 90, 0);
            r.pushAlpha(pageA * local);
            drawTile(r, tiles[size_t(i)], rc, st, false);
            if (st.pins.count(p * 100 + i)) drawIcon(r, Icon::Star, rc.right() - 14, rc.y + 14, 16, Color::hex(0xFFD23A));
            r.popAlpha();
        }
        r.popOffset();
    }
    {
        int p = st.category;
        const auto& tiles = cats[size_t(p)].tiles;
        int i = clampi(st.sel[p], 0, int(tiles.size()) - 1);
        float d = p - st.pagePos;
        float local = ease::outCubic(clampf(st.pageEnter / 0.45f, 0, 1)) * ease::outCubic(clampf(st.enter / 0.55f, 0, 1));
        Rectf rc = st.hl.scaledAboutCenter(1.07f).offset(d * kSpacing + (1 - local) * 90, 0);
        r.pushAlpha(local);
        r.fillRect(rc.offset(6, 9), Color(0, 0, 0, 120));
        drawTile(r, tiles[size_t(i)], rc, st, true);
        if (tiles[size_t(i)].style == Style::Art) r.strokeRect(rc.inset(-2), 2, accentHi(st));
        if (st.pins.count(p * 100 + i)) drawIcon(r, Icon::Star, rc.right() - 14, rc.y + 14, 16, Color::hex(0xFFD23A));
        r.popAlpha();
    }
    drawHints(r, {{Button::A, "selecionar"}, {Button::X, "fixar"}, {Button::Y, "opções"}, {Button::LB, "/"}, {Button::RB, "categorias"}},
              150, 640, 30, Color(225, 228, 232));
    drawLibrary(r, st);
    drawDetail(r, st);
    drawInfoPanel(r, st);
    drawOptions(r, st);
}

int spatialNext(const std::vector<VTile>& tiles, int cur, int dx, int dy) {
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
        bool overlap = (dx != 0) ? (b.y < a.bottom() && b.bottom() > a.y) : (b.x < a.right() && b.right() > a.x);
        float score = px + (overlap ? 0 : 1000) + perp * 0.5f;
        if (score < best) { best = score; bestI = i; }
    }
    return bestI;
}

// ------------------------------------------------------------ dashboard

class MetroV2Dashboard : public Dashboard {
public:
    MetroV2Dashboard() : Dashboard(DashboardId::MetroV2) {}

    bool load(Context& ctx, std::string& error) override {
        loadOptions(ctx);  // opcoes salvas desta interface
        if (!cache_.init(*ctx.renderer) || !rebuildBackground(ctx)) {
            error = "falha ao criar as texturas do Metro V2";
            return false;
        }
        return true;
    }

    void unload(Context& ctx) override {
        if (bg_ != kInvalidTexture) ctx.renderer->destroyTexture(bg_);
        bg_ = kInvalidTexture;
        cache_.release(*ctx.renderer);
    }

    void onEnter(Context& ctx) override {
        enter_.start(0.7f);
        st_.enter = 0;
        moveHighlight(true);
        moveUnderline(ctx, true);
        updateClock(ctx);
    }

    bool debugSelect(int group, int item) override {
        const auto& cats = categories();
        if (group < 0 || group >= int(cats.size()) || item < 0 || item >= int(cats[size_t(group)].tiles.size())) return false;
        st_.category = group;
        st_.sel[group] = item;
        page_.snap(float(group));
        moveHighlight(true);
        ulDirty_ = true;
        ++version_;
        return true;
    }

    std::string debugState() const override {
        char b[200];
        std::snprintf(b, sizeof(b),
                      "category=%d sel=%d library=%d filter=%d libSel=%d libCount=%d detail=%d detailSel=%d info=%d options=%d "
                      "bg=%d accent=%d pins=%d",
                      st_.category, st_.sel[st_.category], st_.libOpen ? 1 : 0, st_.filter, st_.libSel,
                      int(libraryGames(st_.filter).size()), st_.detailOpen ? 1 : 0, st_.detailSel, st_.infoOpen ? 1 : 0,
                      st_.optionsOpen ? 1 : 0, st_.bg, st_.accent, int(st_.pins.size()));
        return b;
    }

    void update(Context& ctx, const InputFrame& in, float dt) override {
        enter_.update(dt);
        st_.enter = enter_.progress();
        page_.update(dt);
        st_.pagePos = page_.value;
        pageEnter_.update(dt);
        st_.pageEnter = pageEnter_.progress();
        hx_.update(dt); hy_.update(dt); hw_.update(dt); hh_.update(dt);
        st_.hl = Rectf{hx_.value, hy_.value, hw_.value, hh_.value};
        ulX_.update(dt); ulW_.update(dt);
        st_.ulX = ulX_.value;
        st_.ulW = ulW_.value;
        libAnim_.update(dt);
        st_.libAnim = libAnim_.value;
        lx_.update(dt); ly_.update(dt);
        st_.libHl = Rectf{lx_.value, ly_.value, 238, 184};
        fX_.update(dt); fW_.update(dt);
        st_.filterX = fX_.value;
        st_.filterW = fW_.value;
        detailAnim_.update(dt);
        st_.detailAnim = detailAnim_.value;
        infoAnim_.update(dt);
        st_.infoAnim = infoAnim_.value;
        optionsAnim_.update(dt);
        st_.optionsAnim = optionsAnim_.value;
        clockTimer_ -= dt;
        if (clockTimer_ <= 0) updateClock(ctx);
        if (ulDirty_) { moveUnderline(ctx, true); ulDirty_ = false; }

        for (int b = 0; b < kButtonCount; ++b)
            if (in.pressed[b]) { ++version_; break; }
        if (st_.optionsOpen) { updateOptions(ctx, in); return; }
        if (st_.infoOpen) {
            if (in.press(Button::B) || in.press(Button::A)) { st_.infoOpen = false; infoAnim_.set(0); ctx.sfx(Sfx::Back); }
            return;
        }
        if (st_.detailOpen) { updateDetail(ctx, in); return; }
        if (st_.libOpen) { updateLibrary(ctx, in); return; }
        updateTiles(ctx, in);
    }

    void render(Context& ctx, IRenderer& r) override {
        (void)ctx;
        cache_.draw(r, animating(), version_, [this](IRenderer& rr) { renderMetroV2(rr, st_, bg_); });
    }

private:
    void loadOptions(Context& ctx) {
        st_.bg = ctx.settings->opt("metro2.bg", 0, 0, 1);
        st_.accent = ctx.settings->opt("metro2.accent", 0, 0, kAccentCount - 1);
    }
    void saveOptions(Context& ctx) {
        ctx.settings->setOpt("metro2.bg", st_.bg);
        ctx.settings->setOpt("metro2.accent", st_.accent);
        ctx.app->saveSettings();
    }

    bool animating() const {
        return !enter_.done() || !page_.settled() || !pageEnter_.done() || !hx_.settled() || !hy_.settled() || !hw_.settled() ||
               !hh_.settled() || !ulX_.settled() || !ulW_.settled() || !libAnim_.settled() || !lx_.settled() || !ly_.settled() ||
               !fX_.settled() || !fW_.settled() || !detailAnim_.settled() || !infoAnim_.settled() || !optionsAnim_.settled();
    }

    bool rebuildBackground(Context& ctx) {
        IRenderer& r = *ctx.renderer;
        if (bg_ != kInvalidTexture) r.destroyTexture(bg_);
        SoftRenderer canvas(r.pixelWidth(), r.pixelHeight());
        canvas.beginFrame();
        drawBackground(canvas, st_.bg);
        bg_ = r.createTexture(canvas.pixelWidth(), canvas.pixelHeight(), canvas.pixels());
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
        if (st_.infoOpen) buildInfo(ctx);
    }

    // Mede os rotulos do cabecalho/filtros com a mesma fonte usada no desenho.
    void moveUnderline(Context& ctx, bool snap) {
        IRenderer& r = *ctx.renderer;
        const auto& cats = categories();
        TextStyle cs;
        cs.size = 34;
        float x = 150, ux = 150, uw = 60;
        for (int i = 0; i < int(cats.size()); ++i) {
            cs.weight = i == st_.category ? FontWeight::Regular : FontWeight::Light;
            float w = r.measureText(cats[size_t(i)].name, cs);
            if (i == st_.category) { ux = x; uw = w; }
            x += w + 40;
        }
        if (snap) { ulX_.snap(ux); ulW_.snap(uw); } else { ulX_.set(ux); ulW_.set(uw); }
    }

    void moveFilterLine(Context& ctx, bool snap) {
        IRenderer& r = *ctx.renderer;
        TextStyle fs;
        fs.size = 22;
        float x = 150, fx = 150, fw = 40;
        for (int i = 0; i < kFilterCount; ++i) {
            fs.weight = i == st_.filter ? FontWeight::Regular : FontWeight::Light;
            float w = r.measureText(kFilters[i].name, fs);
            if (i == st_.filter) { fx = x; fw = w; }
            x += w + 30;
        }
        if (snap) { fX_.snap(fx); fW_.snap(fw); } else { fX_.set(fx); fW_.set(fw); }
    }

    void moveHighlight(bool snap) {
        const auto& tiles = categories()[size_t(st_.category)].tiles;
        Rectf rc = tileRect(tiles[size_t(st_.sel[st_.category])]);
        if (snap) { hx_.snap(rc.x); hy_.snap(rc.y); hw_.snap(rc.w); hh_.snap(rc.h); }
        else { hx_.set(rc.x); hy_.set(rc.y); hw_.set(rc.w); hh_.set(rc.h); }
    }

    void moveLibHighlight(bool snap) {
        Rectf rc = libRect(st_.libSel);
        if (snap) { lx_.snap(rc.x); ly_.snap(rc.y); } else { lx_.set(rc.x); ly_.set(rc.y); }
    }

    void goCategory(Context& ctx, int nc, bool fromLeft, float rowY) {
        const auto& cats = categories();
        if (nc < 0 || nc >= int(cats.size())) { ctx.sfx(Sfx::Error, 0.6f); return; }
        st_.category = nc;
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
        pageEnter_.start(0.4f);
        moveHighlight(true);
        moveUnderline(ctx, false);
        ctx.sfx(Sfx::Tab);
    }

    void openLibrary(Context& ctx, int filter) {
        st_.libOpen = true;
        st_.filter = filter;
        st_.libSel = 0;
        libAnim_.set(1);
        moveLibHighlight(true);
        moveFilterLine(ctx, true);
        ctx.sfx(Sfx::Confirm);
    }

    void openDetail(Context& ctx, const VDetail& d, bool fromLib) {
        st_.detail = d;
        st_.detailOpen = true;
        st_.detailFromLib = fromLib;
        st_.detailSel = 0;
        detailAnim_.set(1);
        ctx.sfx(Sfx::Confirm);
    }

    void buildInfo(Context& ctx) {
        auto& in = st_.info;
        in.clear();
        int h = 0, m = 0, d = 1, mo = 1, y = 2026;
        ctx.platform->localTime(h, m, d, mo, y);
        char b[64];
        std::snprintf(b, sizeof(b), "%02d:%02d · %02d/%02d/%04d", h, m, d, mo, y);
        in.push_back({"hora local", b});
        in.push_back({"versão do Neutro X", kNeutroXVersion});
        std::snprintf(b, sizeof(b), "%d × %d", ctx.renderer->pixelWidth(), ctx.renderer->pixelHeight());
        in.push_back({"resolução de saída do Neutro X", b});
        in.push_back({"dashboard atual", "Metro V2 (referência 2.0.17559.0)"});
        const DashboardInfo* def = findDashboard(ctx.settings->defaultDashboard);
        in.push_back({"dashboard padrão", def ? def->name : "menu do Neutro X"});
        in.push_back({"volume dos efeitos", std::to_string(ctx.settings->sfxVolume) + "%"});
        OrigReport rep = ctx.audio->originalsReport();
        in.push_back({"sons originais carregados",
                      std::to_string(rep.totalLoaded()) + " de " + std::to_string((kSoundThemeCount - 1) * kSfxCount) +
                          (ctx.settings->dashboardSounds == 0 ? " · fonte: originais" : " · fonte: recriados")});
        std::snprintf(b, sizeof(b), "%d texturas · %u KB", ctx.renderer->textureCount(), unsigned(ctx.renderer->textureBytes() / 1024));
        in.push_back({"memória de texturas em uso", b});
        in.push_back({"pasta de dados do Neutro X", ctx.platform->dataPath()});
        in.push_back({"conexão", "modo offline (nenhum serviço online é usado)"});
        ++version_;
    }

    void updateTiles(Context& ctx, const InputFrame& in) {
        const auto& cats = categories();
        int c = st_.category;
        const auto& tiles = cats[size_t(c)].tiles;
        int& sel = st_.sel[c];
        float rowY = tileRect(tiles[size_t(sel)]).cy();
        if (in.press(Button::RB)) { goCategory(ctx, c + 1, true, kGY + 60); return; }
        if (in.press(Button::LB)) { goCategory(ctx, c - 1, true, kGY + 60); return; }
        int dx = in.press(Button::Right) ? 1 : (in.press(Button::Left) ? -1 : 0);
        int dy = in.press(Button::Down) ? 1 : (in.press(Button::Up) ? -1 : 0);
        if (dx || dy) {
            int nx = spatialNext(tiles, sel, dx, dy);
            if (nx >= 0) { sel = nx; moveHighlight(false); ctx.sfx(Sfx::Navigate); }
            else if (dx > 0) { goCategory(ctx, c + 1, true, rowY); return; }
            else if (dx < 0) { goCategory(ctx, c - 1, false, rowY); return; }
            else ctx.sfx(Sfx::Error, 0.4f);
        }
        const VTile& tl = tiles[size_t(sel)];
        if (in.press(Button::A)) {
            switch (tl.kind) {
                case Kind::Game: openDetail(ctx, gameDetail(tl.index, cats[size_t(c)].name), false); break;
                case Kind::Library: openLibrary(ctx, 0); break;
                case Kind::LibraryAZ: openLibrary(ctx, kFilterCount - 1); break;
                case Kind::InfoPanel:
                    st_.infoOpen = true;
                    st_.infoFocus = tl.index;
                    buildInfo(ctx);
                    infoAnim_.set(1);
                    ctx.sfx(Sfx::Open);
                    break;
                case Kind::Sounds: ctx.sfx(Sfx::Confirm); ctx.app->openSoundStatus(); break;
                case Kind::Settings: ctx.sfx(Sfx::Confirm); ctx.app->openSettings(); break;
                case Kind::About: ctx.sfx(Sfx::Confirm); ctx.app->openAbout(); break;
                case Kind::GoCategory: goCategory(ctx, tl.index, true, kGY + 60); break;
                case Kind::Detail: openDetail(ctx, tileDetail(cats[size_t(c)], tl), false); break;
            }
        }
        if (in.press(Button::X)) {
            int key = c * 100 + sel;
            if (st_.pins.count(key)) { st_.pins.erase(key); ctx.app->toast(tl.label + ": desafixado"); }
            else { st_.pins.insert(key); ctx.app->toast(tl.label + ": fixado"); }
            ctx.sfx(Sfx::Confirm, 0.8f);
        }
        if (in.press(Button::Y)) { st_.optionsOpen = true; st_.optionsSel = 0; optionsAnim_.set(1); ctx.sfx(Sfx::Open); }
        if (in.press(Button::B)) {
            if (sel != 0) { sel = 0; moveHighlight(false); ctx.sfx(Sfx::Back); }
            else if (c != 0) goCategory(ctx, 0, true, kGY + 60);
            else { ctx.sfx(Sfx::Back, 0.6f); ctx.app->toast("Pressione START para o menu do Neutro X"); }
        }
    }

    void updateLibrary(Context& ctx, const InputFrame& in) {
        std::vector<int> games = libraryGames(st_.filter);
        int n = int(games.size());
        if (in.press(Button::RB) || in.press(Button::LB)) {
            int nf = st_.filter + (in.press(Button::RB) ? 1 : -1);
            if (nf < 0 || nf >= kFilterCount) { ctx.sfx(Sfx::Error, 0.6f); return; }
            st_.filter = nf;
            st_.libSel = 0;
            moveLibHighlight(true);
            moveFilterLine(ctx, false);
            ctx.sfx(Sfx::Tab);
            return;
        }
        int before = st_.libSel;
        int col = st_.libSel % kLibCols;
        if (in.press(Button::Right) && col + 1 < kLibCols && st_.libSel + 1 < n) st_.libSel++;
        if (in.press(Button::Left) && col > 0) st_.libSel--;
        if (in.press(Button::Down) && st_.libSel + kLibCols < n) st_.libSel += kLibCols;
        if (in.press(Button::Up) && st_.libSel >= kLibCols) st_.libSel -= kLibCols;
        if (before != st_.libSel) { moveLibHighlight(false); ctx.sfx(Sfx::Navigate); }
        else if (in.press(Button::Right) || in.press(Button::Left) || in.press(Button::Up) || in.press(Button::Down)) ctx.sfx(Sfx::Error, 0.4f);
        if (in.press(Button::A) && n > 0) openDetail(ctx, gameDetail(games[size_t(st_.libSel)], "biblioteca de jogos"), true);
        if (in.press(Button::B)) { st_.libOpen = false; libAnim_.set(0); ctx.sfx(Sfx::Back); }
    }

    void updateDetail(Context& ctx, const InputFrame& in) {
        int n = std::min(7, int(st_.detail.rows.size()));
        int before = st_.detailSel;
        if (in.press(Button::Down) && st_.detailSel + 1 < n) st_.detailSel++;
        if (in.press(Button::Up) && st_.detailSel > 0) st_.detailSel--;
        if (before != st_.detailSel) ctx.sfx(Sfx::Navigate);
        if (in.press(Button::A) && n > 0) {
            const InfoRow& rw = st_.detail.rows[size_t(st_.detailSel)];
            if (rw.action == kPlay) {
                ctx.sfx(Sfx::Error, 0.7f);
                ctx.app->toast("Jogos de demonstração não são executados pelo Neutro X");
            } else {
                ctx.sfx(Sfx::Open, 0.8f);
                ctx.app->toast(rw.title + ": item de demonstração (somente informativo)");
            }
        }
        if (in.press(Button::B)) { st_.detailOpen = false; detailAnim_.set(0); ctx.sfx(Sfx::Back); }
    }

    void updateOptions(Context& ctx, const InputFrame& in) {
        if (in.press(Button::Down)) { st_.optionsSel = (st_.optionsSel + 1) % 3; ctx.sfx(Sfx::Navigate); }
        if (in.press(Button::Up)) { st_.optionsSel = (st_.optionsSel + 2) % 3; ctx.sfx(Sfx::Navigate); }
        int delta = (in.press(Button::Right) || in.press(Button::A)) ? 1 : (in.press(Button::Left) ? -1 : 0);
        if (delta != 0 && st_.optionsSel == 0) {
            st_.bg = (st_.bg + 1) % 2; saveOptions(ctx);
            rebuildBackground(ctx);
            ctx.sfx(Sfx::Tab);
        } else if (delta != 0 && st_.optionsSel == 1) {
            st_.accent = (st_.accent + delta + kAccentCount) % kAccentCount; saveOptions(ctx);
            ctx.sfx(Sfx::Tab);
        } else if ((in.press(Button::A) && st_.optionsSel == 2) || in.press(Button::B) || in.press(Button::Y)) {
            st_.optionsOpen = false;
            optionsAnim_.set(0);
            ctx.sfx(Sfx::Back);
        }
    }

    VState st_;
    Tween enter_, pageEnter_;
    Smooth page_{0, 11};
    Smooth hx_{kGX, 20}, hy_{kGY, 20}, hw_{330, 20}, hh_{250, 20};
    Smooth ulX_{150, 16}, ulW_{60, 16};
    Smooth libAnim_{0, 14};
    Smooth lx_{150, 20}, ly_{210, 20};
    Smooth fX_{150, 16}, fW_{60, 16};
    Smooth detailAnim_{0, 14};
    Smooth infoAnim_{0, 14};
    Smooth optionsAnim_{0, 14};
    float clockTimer_ = 0;
    bool ulDirty_ = false;
    SceneCache cache_;
    TextureId bg_ = kInvalidTexture;
    unsigned version_ = 1;
};

}  // namespace

void drawMetroV2Preview(IRenderer& r, float time) {
    (void)time;
    VState st;
    st.category = 1;
    st.pagePos = 1;
    st.sel[1] = 2;
    st.hl = tileRect(categories()[1].tiles[2]);
    // sublinhado aproximado sob "jogos"
    TextStyle cs;
    cs.size = 34;
    cs.weight = FontWeight::Light;
    st.ulX = 150 + r.measureText("início", cs) + 40;
    cs.weight = FontWeight::Regular;
    st.ulW = r.measureText("jogos", cs);
    renderMetroV2(r, st, kInvalidTexture);
}

std::unique_ptr<Dashboard> createMetroV2Dashboard() { return std::unique_ptr<Dashboard>(new MetroV2Dashboard()); }

}  // namespace nx
