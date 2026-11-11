// Neutro X - Dashboard "NXE" (inspirado na geracao 2008-2010, referencia 2.0.9199.0).
//
// Recriacao original: canais empilhados no canto superior esquerdo (Cima/Baixo ou
// LB/RB trocam de canal com deslize vertical); em cada canal, cartoes grandes em
// profundidade (Esquerda/Direita giram a "esteira"). No canal Meu Xbox um avatar
// generico proprio fica em pe diante do cartao de perfil (X: acenar).
// A abre um painel; B volta; Y abre as opcoes (fundo personalizado, avatar).
// Toda a arte e desenhada por codigo. Nenhum recurso dos executaveis originais e usado.
#include <algorithm>
#include <cmath>
#include <cstdio>
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

enum class Kind { Profile, Generic, Game, OpenSettings, OpenAbout };

struct NxeCard {
    Icon icon;
    const char* label;
    Kind kind;
    int index;
};

struct NxeChannel {
    const char* name;
    std::vector<NxeCard> cards;
};

std::vector<NxeChannel> buildChannels() {
    std::vector<NxeChannel> ch(4);
    ch[0].name = "Meu Xbox";
    ch[0].cards = {{Icon::Person, "Perfil", Kind::Profile, 0},       {Icon::Friends, "Amigos", Kind::Generic, 1},
                   {Icon::Trophy, "Conquistas", Kind::Generic, 2},   {Icon::Mail, "Mensagens", Kind::Generic, 3},
                   {Icon::Palette, "Editor de Avatar", Kind::Generic, 4}, {Icon::Clock, "Atividade Recente", Kind::Generic, 5}};
    ch[1].name = "Biblioteca de Jogos";
    int n = 0;
    const DemoGame* g = demoGames(n);
    for (int i = 0; i < n; ++i) ch[1].cards.push_back({g[i].arcade ? Icon::Star : Icon::Controller, g[i].title, Kind::Game, i});
    ch[1].cards.push_back({Icon::List, "Todos os Jogos", Kind::Generic, 10});
    ch[2].name = "Mídia";
    ch[2].cards = {{Icon::Music, "Música", Kind::Generic, 20}, {Icon::Video, "Vídeos", Kind::Generic, 21},
                   {Icon::Picture, "Imagens", Kind::Generic, 22}, {Icon::Disc, "Disco", Kind::Generic, 23}};
    ch[3].name = "Configurações";
    ch[3].cards = {{Icon::Gear, "Sistema", Kind::Generic, 30},        {Icon::Person, "Perfil", Kind::Generic, 31},
                   {Icon::Family, "Família", Kind::Generic, 32},       {Icon::Palette, "Neutro X", Kind::OpenSettings, 0},
                   {Icon::Info, "Sobre o Neutro X", Kind::OpenAbout, 0}};
    return ch;
}

const std::vector<NxeChannel>& channels() {
    static const std::vector<NxeChannel> ch = buildChannels();
    return ch;
}

struct NxeTheme {
    const char* name;
    Color skyTop, skyBottom, floorTop, floorBottom, cardTop, cardBottom, ink;
};

const NxeTheme kThemes[] = {
    {"Verde", Color::hex(0x8CC63A), Color::hex(0xE9F1DC), Color::hex(0xE4E8EA), Color::hex(0xAEB4BA),
     Color::hex(0xC5E453), Color::hex(0x5AA01E), Color::hex(0x5A6068)},
    {"Céu", Color::hex(0x3E9BE0), Color::hex(0xDCEEFA), Color::hex(0xE2E8EE), Color::hex(0xA8B2BC),
     Color::hex(0x8ACBF2), Color::hex(0x2A78C0), Color::hex(0x56606A)},
    {"Pôr do sol", Color::hex(0xF08A24), Color::hex(0xFBE3C4), Color::hex(0xEAE0D6), Color::hex(0xB8A89C),
     Color::hex(0xFFB65A), Color::hex(0xD0601A), Color::hex(0x6A5A50)},
    {"Noite", Color::hex(0x0E1520), Color::hex(0x2A3A4E), Color::hex(0x2A3038), Color::hex(0x121519),
     Color::hex(0x5E7086), Color::hex(0x26303E), Color::hex(0xB0B8C2)},
};
constexpr int kThemeCount = int(sizeof(kThemes) / sizeof(kThemes[0]));

struct ShirtColor {
    const char* name;
    unsigned rgb;
};
const ShirtColor kShirts[] = {{"Azul", 0x2F7FD8}, {"Vermelha", 0xD9363E}, {"Verde", 0x4CA82A},
                              {"Laranja", 0xF08A24}, {"Roxa", 0x8A4FC8},   {"Grafite", 0x3A3F46}};
constexpr int kShirtCount = int(sizeof(kShirts) / sizeof(kShirts[0]));

// Acoes das linhas do painel
enum RowAction { kRowInfo = 0, kRowPlay = 1, kRowShirt = 100 };

struct Panel {
    std::string title;
    Icon icon = Icon::Info;
    std::string note;
    std::vector<InfoRow> rows;
};

InfoRow mkRow(Icon ic, std::string t, std::string s, std::string d = "", float prog = -1, int action = kRowInfo) {
    InfoRow r;
    r.icon = int(ic);
    r.title = std::move(t);
    r.sub = std::move(s);
    r.desc = std::move(d);
    r.progress = prog;
    r.action = action;
    return r;
}

InfoRow gameRow(const DemoGame& g, const std::string& sub, float prog) {
    InfoRow r = mkRow(g.arcade ? Icon::Star : Icon::Controller, g.title, sub, g.description, prog);
    r.artSeed = g.artSeed;
    r.c1 = g.c1;
    r.c2 = g.c2;
    return r;
}

Panel buildPanel(const NxeCard& card, Context* ctx) {
    Panel p;
    p.title = card.label;
    p.icon = card.icon;
    int n = 0;
    const DemoGame* g = demoGames(n);
    const DemoProfile& prof = demoProfile();
    switch (card.kind) {
        case Kind::Profile:
            p.rows.push_back(mkRow(Icon::Person, prof.gamertag, "Nome do jogador (local)", "Perfil fictício local. Nenhuma conta online é usada."));
            p.rows.push_back(mkRow(Icon::Trophy, std::to_string(prof.points) + " pontos", "Pontuação de demonstração"));
            p.rows.push_back(mkRow(Icon::Mail, prof.motto, "Lema"));
            p.rows.push_back(mkRow(Icon::Globe, prof.zone, "Zona de jogo"));
            p.note = "Perfil local de demonstração.";
            return p;
        case Kind::Game: {
            const DemoGame& gm = g[card.index];
            InfoRow play = gameRow(gm, "Jogo de demonstração (não executável)", -1);
            play.icon = int(Icon::Controller);
            play.title = "Jogar";
            play.action = kRowPlay;
            p.rows.push_back(play);
            p.rows.push_back(gameRow(gm, std::to_string(gm.points) + " de " + std::to_string(gm.maxPoints) + " pontos",
                                     float(gm.points) / gm.maxPoints));
            p.rows.back().title = "Conquistas";
            p.rows.back().icon = int(Icon::Trophy);
            InfoRow det = gameRow(gm, std::string(gm.genre) + " · " + gm.year, -1);
            det.title = "Detalhes";
            det.icon = int(Icon::Info);
            p.rows.push_back(det);
            p.note = "Jogos de demonstração abrem apenas telas informativas.";
            return p;
        }
        default:
            break;
    }
    switch (card.index) {
        case 1: {
            int fn = 0;
            const DemoFriend* f = demoFriends(fn);
            for (int i = 0; i < fn; ++i) p.rows.push_back(mkRow(Icon::Person, f[i].name, f[i].status));
            p.note = "Modo offline: amigos de demonstração.";
            break;
        }
        case 2:
            for (int i = 0; i < n; ++i)
                p.rows.push_back(gameRow(g[i], std::to_string(g[i].points) + " / " + std::to_string(g[i].maxPoints) + " pontos",
                                         float(g[i].points) / g[i].maxPoints));
            p.note = "Conquistas de demonstração.";
            break;
        case 3:
            p.rows.push_back(mkRow(Icon::Mail, "Bem-vindo ao Neutro X", "Mensagem do sistema · lida",
                                   "Esta é uma caixa de mensagens local de demonstração."));
            p.note = "Nenhuma mensagem nova.";
            break;
        case 4:
            for (int i = 0; i < kShirtCount; ++i) {
                InfoRow r = mkRow(Icon::Palette, std::string("Camiseta ") + kShirts[i].name, "Aplicar ao avatar",
                                  "Personalize o avatar genérico do Neutro X. A escolha vale durante a sessão.", -1, kRowShirt + i);
                r.c1 = Color::hex(kShirts[i].rgb);
                r.c2 = brighten(r.c1, -0.6f);
                p.rows.push_back(r);
            }
            p.note = "Avatar genérico próprio (sem serviço online de avatares).";
            break;
        case 5:
            for (int i = 0; i < 4 && i < n; ++i) p.rows.push_back(gameRow(g[i], i == 0 ? "Jogado hoje" : "Jogado esta semana", -1));
            p.note = "Atividade de demonstração.";
            break;
        case 10:
            for (int i = 0; i < n; ++i) p.rows.push_back(gameRow(g[i], std::string(g[i].genre) + " · " + g[i].year, -1));
            p.note = "Biblioteca de demonstração.";
            break;
        case 20:
        case 21:
        case 22: {
            int mn = 0;
            const DemoMedia* m = card.index == 20 ? demoMusic(mn) : card.index == 21 ? demoVideos(mn) : demoPictures(mn);
            for (int i = 0; i < mn; ++i) {
                InfoRow r = mkRow(card.icon, m[i].title, m[i].detail);
                r.artSeed = 900 + card.index * 7 + i;
                r.c1 = Color::hex(0x8CC63A);
                r.c2 = Color::hex(0x24400C);
                p.rows.push_back(r);
            }
            p.note = "Mídia de demonstração. Nenhum arquivo do sistema é lido.";
            break;
        }
        case 23:
            p.rows.push_back(mkRow(Icon::Disc, "Nenhum disco", "A unidade de disco não é usada pelo Neutro X"));
            p.note = "Por segurança, o Neutro X não acessa a unidade de disco.";
            break;
        case 30: {
            char res[48] = "1280 × 720";
            if (ctx && ctx->renderer)
                std::snprintf(res, sizeof(res), "%d × %d", ctx->renderer->pixelWidth(), ctx->renderer->pixelHeight());
            p.rows.push_back(mkRow(Icon::Tv, "Resolução do Neutro X", res));
            p.rows.push_back(mkRow(Icon::Speaker, "Volume dos efeitos",
                                   ctx && ctx->settings ? std::to_string(ctx->settings->sfxVolume) + "%" : "80%"));
            p.rows.push_back(mkRow(Icon::Globe, "Idioma", "Português (Brasil)"));
            p.rows.push_back(mkRow(Icon::Network, "Rede", "Modo offline"));
            p.note = "Somente visualização. As configurações do console não são lidas nem alteradas.";
            break;
        }
        case 31:
            p.rows.push_back(mkRow(Icon::Person, prof.gamertag, "Perfil local"));
            p.rows.push_back(mkRow(Icon::Picture, "Imagem do jogador", "Ícone padrão"));
            p.note = "Representação visual.";
            break;
        case 32:
            p.rows.push_back(mkRow(Icon::Family, "Controle dos pais", "Não configurado (visual)"));
            p.rows.push_back(mkRow(Icon::Clock, "Temporizador de família", "Desativado (visual)"));
            p.note = "Representação visual.";
            break;
        default:
            break;
    }
    if (p.rows.empty()) p.rows.push_back(mkRow(Icon::Info, "Sem itens", "Demonstração"));
    return p;
}

// ------------------------------------------------------------ estado/visual

struct NxeState {
    float chanPos = 0;
    int channel = 0;
    int card[4] = {};
    float cardPos[4] = {};
    float enter = 1;
    float time = 0;
    bool panelOpen = false;
    float panelAnim = 0;
    Panel panel;
    int panelSel = 0;
    float panelSelAnim = 0;
    float panelScroll = 0;
    int theme = 0;
    bool showAvatar = true;
    int shirt = 0;
    float wave = 0;
    bool optionsOpen = false;
    float optionsAnim = 0;
    int optionsSel = 0;
    std::string clock = "12:00";
};

struct Slot {
    float x, y, w, h, a;
};
const Slot kSlots[] = {
    {-470, 236, 466, 352, 0}, {36, 236, 466, 352, 1},  {502, 272, 352, 262, 1}, {846, 296, 220, 206, 1},
    {1048, 310, 150, 166, 1}, {1180, 322, 100, 140, 1}, {1292, 330, 80, 120, 0},
};

Slot slotAt(float k) {
    k = clampf(k, -1, 5);
    int i = int(std::floor(k + 1));
    if (i >= 6) return kSlots[6];
    float f = (k + 1) - i;
    const Slot& a = kSlots[i];
    const Slot& b = kSlots[i + 1];
    return Slot{lerpf(a.x, b.x, f), lerpf(a.y, b.y, f), lerpf(a.w, b.w, f), lerpf(a.h, b.h, f), lerpf(a.a, b.a, f)};
}

void drawBackground(IRenderer& r, int themeIdx) {
    const NxeTheme& t = kThemes[themeIdx];
    const Rectf full{0, 0, float(kLogicalW), float(kLogicalH)};
    r.fillGradientV(Rectf{0, 0, 1280, 430}, t.skyTop, t.skyBottom);
    r.fillGradientV(Rectf{0, 430, 1280, 290}, t.floorTop, t.floorBottom);
    r.fillGradientV(Rectf{0, 380, 1280, 70}, t.skyBottom.withAlpha(0), t.floorTop);
    r.fillRadial(900, 120, 520, Color(255, 255, 255, themeIdx == 3 ? 30 : 110), Color(255, 255, 255, 0), full);
    for (int i = 0; i < 16; ++i) {
        float x = std::fmod(i * 173.0f + 40, 1300.0f);
        float y = 50 + std::fmod(i * 97.0f, 330.0f);
        r.fillCircle(x, y, 10.0f + (i % 4) * 9.0f, Color(255, 255, 255, uint8_t((themeIdx == 3 ? 8 : 18) + (i % 3) * 9)));
    }
    r.strokeCircle(1180, 690, 120, 2, Color(255, 255, 255, 60));
    r.strokeCircle(1180, 690, 170, 2, Color(255, 255, 255, 40));
    r.strokeCircle(1180, 690, 220, 2, Color(255, 255, 255, 22));
}

void drawCard(IRenderer& r, const NxeCard& c, const Slot& s, const NxeTheme& t) {
    Rectf rc{s.x, s.y, s.w, s.h};
    r.fillRoundRect(rc.offset(3, 6), 7, Color(0, 0, 0, 40));
    if (c.kind == Kind::Game) {
        int n = 0;
        const DemoGame& g = demoGames(n)[c.index];
        r.fillRoundRect(rc, 7, g.c2);
        drawAbstractArt(r, rc.inset(2), g.artSeed, g.c1, g.c2);
    } else {
        r.fillRoundRect(rc, 7, t.cardBottom);
        r.pushClip(rc.inset(1));
        r.fillGradientV(rc, t.cardTop, t.cardBottom);
        r.fillCircle(rc.x + rc.w * 0.3f, rc.y + rc.h * 0.55f, rc.h * 0.45f, Color(255, 255, 255, 18));
        r.fillGradientV(Rectf{rc.x, rc.y, rc.w, rc.h * 0.35f}, Color(255, 255, 255, 50), Color(255, 255, 255, 0));
        r.popClip();
    }
    if (c.kind == Kind::Profile) {
        const DemoProfile& p = demoProfile();
        float k = s.w / 466.0f;
        TextStyle ps;
        ps.size = 34 * k;
        ps.color = Color(255, 255, 255);
        r.drawText(p.gamertag, rc.x + 20 * k, rc.y + 22 * k, ps);
        std::string pts = std::to_string(p.points) + " P";
        r.drawText(pts.c_str(), rc.x + 20 * k, rc.y + 62 * k, ps);
        ps.size = 22 * k;
        r.drawText("Jogados recentemente", rc.x + 20 * k, rc.y + 160 * k, ps);
        int n = 0;
        const DemoGame* g = demoGames(n);
        for (int i = 0; i < 4 && i < n; ++i) {
            Rectf th{rc.x + (20 + i * 70) * k, rc.y + 192 * k, 60 * k, 60 * k};
            drawAbstractArt(r, th, g[i].artSeed, g[i].c1, g[i].c2);
            r.strokeRect(th, 2, Color(255, 255, 255, 150));
        }
    } else {
        Color bg = c.kind == Kind::Game ? Color(0, 0, 0, 0) : lerpColor(t.cardTop, t.cardBottom, 0.45f);
        drawIcon(r, c.icon, rc.cx(), rc.y + rc.h * 0.42f, rc.h * 0.34f, Color(255, 255, 255, 235), bg);
    }
    if (s.h >= 200) {
        if (c.kind == Kind::Game)
            r.fillGradientV(Rectf{rc.x, rc.bottom() - 70, rc.w, 70}, Color(0, 0, 0, 0), Color(0, 0, 0, 150));
        TextStyle ls;
        ls.size = s.h > 300 ? 28 : 21;
        ls.color = Color(255, 255, 255);
        if (c.kind != Kind::Profile) r.drawText(c.label, rc.x + 16, rc.bottom() - ls.size - 16, ls);
    }
}

void drawChannelLabels(IRenderer& r, const NxeState& st) {
    const auto& ch = channels();
    for (int i = 0; i < int(ch.size()); ++i) {
        float k = clampf(i - st.chanPos, -4.0f, 1.0f);
        // tabela: k = -4 ... 1
        const float ys[] = {14, 40, 70, 102, 140, 196};
        const float ss[] = {18, 22, 26, 32, 54, 40};
        const float as[] = {0, 0.42f, 0.6f, 0.82f, 1.0f, 0};
        int i0 = int(std::floor(k + 4));
        if (i0 >= 5) i0 = 4;
        float f = (k + 4) - i0;
        float y = lerpf(ys[i0], ys[i0 + 1], f), sz = lerpf(ss[i0], ss[i0 + 1], f), a = lerpf(as[i0], as[i0 + 1], f);
        if (a <= 0.01f) continue;
        TextStyle ts;
        ts.size = sz;
        ts.weight = std::fabs(k) < 0.5f ? FontWeight::Bold : FontWeight::Regular;
        ts.color = Color(255, 255, 255).fade(a);
        r.drawText(ch[size_t(i)].name, 40, y, ts);
        if (std::fabs(k) < 0.5f) r.fillRect(Rectf{22, y + sz * 0.33f, 10, 10}, Color(255, 255, 255).fade(a));
    }
}

void drawPanel(IRenderer& r, const NxeState& st, const NxeTheme& t) {
    float a = ease::outCubic(st.panelAnim);
    if (a <= 0.01f) return;
    r.fillRect(Rectf{0, 0, 1280, 720}, Color(0, 0, 0, uint8_t(90 * a)));
    r.pushAlpha(a);
    r.pushOffset((1 - a) * 760, 0);
    const Rectf P{70, 66, 1140, 588};
    const Color dark = Color::hex(0x2B3238);
    r.fillRoundRect(P.offset(5, 10), 16, Color(0, 0, 0, 70));
    r.fillRoundRect(P, 16, Color::hex(0xF6F8F2));
    r.pushClip(Rectf{P.x, P.y, P.w, 84});
    r.fillRoundRect(P, 16, t.cardBottom);
    r.fillGradientV(Rectf{P.x, P.y, P.w, 84}, t.cardTop, t.cardBottom);
    r.popClip();
    drawIcon(r, st.panel.icon, P.right() - 60, P.y + 42, 46, Color(255, 255, 255, 230), lerpColor(t.cardTop, t.cardBottom, 0.5f));
    TextStyle tt;
    tt.size = 36;
    tt.weight = FontWeight::Bold;
    tt.color = Color(255, 255, 255);
    r.drawText(st.panel.title.c_str(), P.x + 30, P.y + 22, tt);

    const float lx = P.x + 24, lw = 548, ly = P.y + 104, rowH = 62;
    const int visible = 7;
    r.pushClip(Rectf{lx - 4, ly - 4, lw + 8, visible * rowH + 4});
    float sy = ly + (st.panelSelAnim - st.panelScroll) * rowH;
    r.fillRoundRect(Rectf{lx, sy, lw, rowH - 6}, 8, t.cardBottom);
    for (int i = 0; i < int(st.panel.rows.size()); ++i) {
        float y = ly + (i - st.panelScroll) * rowH;
        if (y < ly - rowH || y > ly + visible * rowH) continue;
        const InfoRow& row = st.panel.rows[size_t(i)];
        bool sel = i == st.panelSel;
        Color fg = sel ? Color(255, 255, 255) : dark;
        if (row.action >= kRowShirt) {
            r.fillCircle(lx + 30, y + 27, 15, row.c1);
            r.strokeCircle(lx + 30, y + 27, 15, 2, Color(255, 255, 255, 200));
        } else {
            drawIcon(r, Icon(row.icon), lx + 30, y + 27, 30, fg, sel ? t.cardBottom : Color::hex(0xF6F8F2));
        }
        TextStyle rt;
        rt.size = 22;
        rt.color = fg;
        rt.weight = sel ? FontWeight::Bold : FontWeight::Regular;
        r.drawText(row.title.c_str(), lx + 62, y + 6, rt);
        TextStyle rs;
        rs.size = 15;
        rs.color = sel ? Color(255, 255, 255, 220) : Color::hex(0x6A737C);
        r.drawText(row.sub.c_str(), lx + 62, y + 33, rs);
        if (row.progress >= 0) {
            Rectf bar{lx + lw - 150, y + 24, 130, 8};
            r.fillRoundRect(bar, 4, sel ? Color(255, 255, 255, 80) : Color(0, 0, 0, 30));
            r.fillRoundRect(Rectf{bar.x, bar.y, bar.w * clampf(row.progress, 0, 1), bar.h}, 4,
                            sel ? Color(255, 255, 255) : t.cardBottom);
        }
    }
    r.popClip();
    TextStyle nt;
    nt.size = 16;
    nt.color = Color::hex(0x6A737C);
    r.drawText(st.panel.note.c_str(), lx, ly + visible * rowH + 8, nt);

    // Painel de informacoes do item selecionado
    if (!st.panel.rows.empty()) {
        const InfoRow& row = st.panel.rows[size_t(clampi(st.panelSel, 0, int(st.panel.rows.size()) - 1))];
        Rectf art{P.x + 600, P.y + 104, P.w - 630, 250};
        if (row.artSeed) drawAbstractArt(r, art, row.artSeed, row.c1, row.c2);
        else if (row.action >= kRowShirt) r.fillGradientV(art, row.c1, row.c2);
        else r.fillGradientV(art, t.cardTop, t.cardBottom);
        if (row.action >= kRowShirt) {
            AvatarLook look;
            look.shirt = row.c1;
            drawAvatar(r, art.cx(), art.bottom() - 10, 220, look, 0);
        } else {
            drawIcon(r, Icon(row.icon), art.cx(), art.cy(), 90, Color(255, 255, 255, 210));
        }
        TextStyle it;
        it.size = 26;
        it.weight = FontWeight::Bold;
        it.color = dark;
        r.drawText(row.title.c_str(), art.x, art.bottom() + 16, it);
        TextStyle ds;
        ds.size = 18;
        ds.color = Color::hex(0x4A535C);
        const std::string& d = row.desc.empty() ? row.sub : row.desc;
        drawWrapped(r, d.c_str(), art.x, art.bottom() + 54, art.w, ds, 25, 4);
    }
    drawHints(r, {{Button::B, "Voltar"}, {Button::A, "Selecionar"}}, P.right() - 30, P.bottom() - 32, 30, dark,
              Align::Right);
    r.popOffset();
    r.popAlpha();
}

// Camada estatica (vai para o cache): fundo, canais, cartoes, painel, rodape.
void renderNxeStatic(IRenderer& r, const NxeState& st, TextureId bg) {
    const NxeTheme& t = kThemes[st.theme];
    if (bg != kInvalidTexture) r.drawTexture(bg, Rectf{0, 0, float(kLogicalW), float(kLogicalH)});
    else drawBackground(r, st.theme);
    float e = ease::outCubic(st.enter);
    r.pushAlpha(e);
    drawChannelLabels(r, st);
    r.popAlpha();

    // Perfil no canto superior direito
    const DemoProfile& p = demoProfile();
    TextStyle tag;
    tag.size = 26;
    tag.align = Align::Right;
    tag.color = Color(255, 255, 255);
    r.drawText(p.gamertag, 1150, 26, tag);
    std::string pts = std::to_string(p.points) + " P";
    r.drawText(pts.c_str(), 1150, 58, tag);
    tag.size = 16;
    tag.color = Color(255, 255, 255, 210);
    r.drawText(st.clock.c_str(), 1150, 92, tag);
    r.fillRoundRect(Rectf{1166, 26, 70, 70}, 6, Color::hex(0xF08A24));
    drawIcon(r, Icon::Person, 1201, 64, 52, Color(255, 255, 255, 220));

    // Fileiras de cartoes (canal atual e o que entra/sai)
    const auto& ch = channels();
    for (int ci = 0; ci < int(ch.size()); ++ci) {
        float d = ci - st.chanPos;
        if (std::fabs(d) >= 1.0f) continue;
        float rowA = 1.0f - std::fabs(d);
        const auto& cards = ch[size_t(ci)].cards;
        r.pushAlpha(rowA);
        r.pushOffset(0, d * 420);
        // do fundo para a frente
        for (int k = int(cards.size()) - 1; k >= 0; --k) {
            float rel = k - st.cardPos[ci];
            if (rel < -1.0f || rel > 5.0f) continue;
            Slot s = slotAt(rel);
            float local = ease::outCubic(clampf((st.enter - 0.06f * clampf(rel, 0, 5)) / 0.7f, 0, 1));
            s.x += (1 - local) * 700;
            float a = s.a * local;
            if (a <= 0.01f) continue;
            r.pushAlpha(a);
            drawCard(r, cards[size_t(k)], s, t);
            r.popAlpha();
        }
        r.popOffset();
        r.popAlpha();
    }

    // Rodape: contador, proximo canal e dicas
    int c = st.channel;
    TextStyle cnt;
    cnt.size = 20;
    cnt.color = t.ink;
    std::string pos = std::to_string(st.card[c] + 1) + " de " + std::to_string(ch[size_t(c)].cards.size());
    r.drawText(pos.c_str(), 40, 604, cnt);
    if (c + 1 < int(ch.size())) {
        std::string next = std::string("\xE2\x86\x93 ") + ch[size_t(c + 1)].name;
        cnt.align = Align::Right;
        r.drawText(next.c_str(), 1240, 604, cnt);
    }
    if (c == 0)
        drawHints(r, {{Button::A, "Selecionar"}, {Button::X, "Acenar"}, {Button::Y, "Opções"}}, 40, 676, 32, t.ink);
    else
        drawHints(r, {{Button::A, "Selecionar"}, {Button::Y, "Opções"}}, 40, 676, 32, t.ink);
    drawPanel(r, st, t);
}

// Camada ao vivo: avatar (anima sempre) e opcoes.
void renderNxeLive(IRenderer& r, const NxeState& st) {
    // Avatar diante do cartao de perfil (canal Meu Xbox, cartao 0).
    float chA = 1.0f - clampf(std::fabs(0 - st.chanPos), 0, 1);
    if (st.showAvatar && chA > 0.01f) {
        Slot s = slotAt(0 - st.cardPos[0]);
        float local = ease::outCubic(clampf((st.enter - 0.2f) / 0.7f, 0, 1));
        float a = chA * s.a * local * (1.0f - ease::outCubic(st.panelAnim));
        if (a > 0.01f) {
            AvatarLook look;
            look.shirt = Color::hex(kShirts[st.shirt].rgb);
            r.pushAlpha(a);
            r.pushOffset(0, (0 - st.chanPos) * 420 + (1 - local) * 40);
            drawAvatar(r, s.x + s.w * 0.82f, s.y + s.h + 52 * (s.h / 352.0f), s.h * 1.22f, look, st.time, st.wave);
            r.popOffset();
            r.popAlpha();
        }
    }
    float oa = ease::outCubic(st.optionsAnim);
    if (oa > 0.01f) {
        const NxeTheme& t = kThemes[st.theme];
        r.pushAlpha(oa);
        r.fillRect(Rectf{0, 0, 1280, 720}, Color(0, 0, 0, 100));
        Rectf box = Rectf{380, 200, 520, 300}.scaledAboutCenter(0.92f + 0.08f * oa);
        r.fillRoundRect(box.offset(5, 8), 16, Color(0, 0, 0, 80));
        r.fillRoundRect(box, 16, Color::hex(0xF6F8F2));
        r.pushClip(Rectf{box.x, box.y, box.w, 74});
        r.fillRoundRect(box, 16, t.cardBottom);
        r.fillGradientV(Rectf{box.x, box.y, box.w, 74}, t.cardTop, t.cardBottom);
        r.popClip();
        TextStyle tt;
        tt.size = 30;
        tt.weight = FontWeight::Bold;
        tt.color = Color(255, 255, 255);
        r.drawText("Opções da interface", box.x + 28, box.y + 20, tt);
        const char* labels[3] = {"Fundo", "Avatar", "Fechar"};
        std::string vals[3] = {kThemes[st.theme].name, st.showAvatar ? "Mostrar" : "Ocultar", ""};
        for (int i = 0; i < 3; ++i) {
            Rectf row{box.x + 18, box.y + 92 + i * 62.0f, box.w - 36, 52};
            bool sel = i == st.optionsSel;
            if (sel) r.fillRoundRect(row, 8, t.cardBottom);
            TextStyle ls;
            ls.size = 24;
            ls.color = sel ? Color(255, 255, 255) : Color::hex(0x2B3238);
            ls.weight = sel ? FontWeight::Bold : FontWeight::Regular;
            r.drawText(labels[i], row.x + 16, row.y + 14, ls);
            ls.align = Align::Right;
            r.drawText(vals[i].c_str(), row.right() - 16, row.y + 14, ls);
        }
        r.popAlpha();
    }
}

// ------------------------------------------------------------ dashboard

class NxeDashboard : public Dashboard {
public:
    NxeDashboard() : Dashboard(DashboardId::Nxe) {}

    bool load(Context& ctx, std::string& error) override {
        loadOptions(ctx);  // opcoes salvas desta interface
        if (!cache_.init(*ctx.renderer) || !rebuildBackground(ctx)) {
            error = "falha ao criar as texturas do NXE";
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
        enter_.start(0.85f);
        st_.enter = 0;
        updateClock(ctx);
    }

    std::string debugState() const override {
        char b[160];
        std::snprintf(b, sizeof(b), "channel=%d card=%d panel=%d panelSel=%d options=%d theme=%d avatar=%d shirt=%d wave=%d",
                      st_.channel, st_.card[st_.channel], st_.panelOpen ? 1 : 0, st_.panelSel, st_.optionsOpen ? 1 : 0,
                      st_.theme, st_.showAvatar ? 1 : 0, st_.shirt, waveT_ > 0 ? 1 : 0);
        return b;
    }

    void update(Context& ctx, const InputFrame& in, float dt) override {
        enter_.update(dt);
        st_.enter = enter_.progress();
        st_.time += dt;
        chan_.update(dt);
        st_.chanPos = chan_.value;
        for (int i = 0; i < 4; ++i) {
            cardAnim_[i].update(dt);
            st_.cardPos[i] = cardAnim_[i].value;
        }
        panelAnim_.update(dt);
        st_.panelAnim = panelAnim_.value;
        panelSel_.update(dt);
        st_.panelSelAnim = panelSel_.value;
        panelScroll_.update(dt);
        st_.panelScroll = panelScroll_.value;
        optionsAnim_.update(dt);
        st_.optionsAnim = optionsAnim_.value;
        if (waveT_ > 0) waveT_ = std::max(0.0f, waveT_ - dt);
        st_.wave = std::min(1.0f, std::min(waveT_ / 0.35f, (kWaveDur - waveT_) / 0.25f));
        if (waveT_ <= 0) st_.wave = 0;
        clockTimer_ -= dt;
        if (clockTimer_ <= 0) updateClock(ctx);

        for (int b = 0; b < kButtonCount; ++b)
            if (in.pressed[b]) { ++version_; break; }
        if (st_.optionsOpen) { updateOptions(ctx, in); return; }
        if (st_.panelOpen) { updatePanel(ctx, in); return; }
        updateHome(ctx, in);
    }

    void render(Context& ctx, IRenderer& r) override {
        (void)ctx;
        cache_.draw(r, animating(), version_, [this](IRenderer& rr) { renderNxeStatic(rr, st_, bg_); });
        renderNxeLive(r, st_);
    }

private:
    static constexpr float kWaveDur = 2.4f;

    void loadOptions(Context& ctx) {
        st_.theme = ctx.settings->opt("nxe.theme", 0, 0, kThemeCount - 1);
        st_.showAvatar = ctx.settings->opt("nxe.avatar", 1, 0, 1) != 0;
        st_.shirt = ctx.settings->opt("nxe.shirt", 0, 0, kShirtCount - 1);
    }
    void saveOptions(Context& ctx) {
        ctx.settings->setOpt("nxe.theme", st_.theme);
        ctx.settings->setOpt("nxe.avatar", st_.showAvatar ? 1 : 0);
        ctx.settings->setOpt("nxe.shirt", st_.shirt);
        ctx.app->saveSettings();
    }

    bool animating() const {
        if (!enter_.done() || !chan_.settled() || !panelAnim_.settled() || !panelSel_.settled() || !panelScroll_.settled())
            return true;
        for (const Smooth& s : cardAnim_)
            if (!s.settled()) return true;
        return false;
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

    void updateClock(Context& ctx) {
        int h = 0, m = 0, d = 1, mo = 1, y = 2026;
        ctx.platform->localTime(h, m, d, mo, y);
        char b[16];
        std::snprintf(b, sizeof(b), "%02d:%02d", h, m);
        if (st_.clock != b) ++version_;
        st_.clock = b;
        clockTimer_ = 1.0f;
    }

    void setChannel(Context& ctx, int delta) {
        int n = int(channels().size());
        int nc = st_.channel + delta;
        if (nc < 0 || nc >= n) { ctx.sfx(Sfx::Error, 0.6f); return; }
        st_.channel = nc;
        chan_.set(float(nc));
        ctx.sfx(Sfx::Tab);
    }

    void updateHome(Context& ctx, const InputFrame& in) {
        const auto& ch = channels();
        int c = st_.channel;
        int ncards = int(ch[size_t(c)].cards.size());
        if (in.press(Button::Down) || in.press(Button::RB)) setChannel(ctx, +1);
        else if (in.press(Button::Up) || in.press(Button::LB)) setChannel(ctx, -1);
        c = st_.channel;
        ncards = int(ch[size_t(c)].cards.size());
        int before = st_.card[c];
        if (in.press(Button::Right) && st_.card[c] + 1 < ncards) st_.card[c]++;
        if (in.press(Button::Left) && st_.card[c] > 0) st_.card[c]--;
        if (before != st_.card[c]) {
            cardAnim_[c].set(float(st_.card[c]));
            ctx.sfx(Sfx::Navigate);
        } else if (in.press(Button::Right) || in.press(Button::Left)) {
            ctx.sfx(Sfx::Error, 0.4f);
        }
        if (in.press(Button::A)) {
            const NxeCard& card = ch[size_t(c)].cards[size_t(st_.card[c])];
            if (card.kind == Kind::OpenSettings) { ctx.sfx(Sfx::Confirm); ctx.app->openSettings(); }
            else if (card.kind == Kind::OpenAbout) { ctx.sfx(Sfx::Confirm); ctx.app->openAbout(); }
            else {
                st_.panel = buildPanel(card, &ctx);
                st_.panelOpen = true;
                st_.panelSel = 0;
                panelSel_.snap(0);
                panelScroll_.snap(0);
                panelAnim_.set(1);
                ctx.sfx(Sfx::Confirm);
            }
        }
        if (in.press(Button::X)) {
            if (c == 0 && st_.showAvatar) { waveT_ = kWaveDur; ctx.sfx(Sfx::Open, 0.8f); }
            else ctx.sfx(Sfx::Error, 0.5f);
        }
        if (in.press(Button::Y)) openOptions(ctx);
        if (in.press(Button::B)) {
            if (st_.card[c] != 0) { st_.card[c] = 0; cardAnim_[c].set(0); ctx.sfx(Sfx::Back); }
            else if (c != 0) setChannel(ctx, -c);
            else { ctx.sfx(Sfx::Back, 0.6f); ctx.app->toast("Pressione START para o menu do Neutro X"); }
        }
    }

    void updatePanel(Context& ctx, const InputFrame& in) {
        int n = int(st_.panel.rows.size());
        int before = st_.panelSel;
        if (in.press(Button::Down) && st_.panelSel + 1 < n) st_.panelSel++;
        if (in.press(Button::Up) && st_.panelSel > 0) st_.panelSel--;
        if (before != st_.panelSel) {
            ctx.sfx(Sfx::Navigate);
            panelSel_.set(float(st_.panelSel));
            const int visible = 7;
            float scroll = panelScroll_.target;
            if (st_.panelSel < scroll) scroll = float(st_.panelSel);
            if (st_.panelSel > scroll + visible - 1) scroll = float(st_.panelSel - visible + 1);
            panelScroll_.set(scroll);
        }
        if (in.press(Button::A) && n > 0) {
            const InfoRow& row = st_.panel.rows[size_t(st_.panelSel)];
            if (row.action >= kRowShirt) {
                st_.shirt = row.action - kRowShirt; saveOptions(ctx);
                ctx.sfx(Sfx::Confirm);
                ctx.app->toast(std::string("Avatar: ") + row.title);
            } else if (row.action == kRowPlay) {
                ctx.sfx(Sfx::Error, 0.7f);
                ctx.app->toast("Jogos de demonstração não são executados pelo Neutro X");
            } else {
                ctx.sfx(Sfx::Open, 0.8f);
                ctx.app->toast(row.title + ": item de demonstração (somente informativo)");
            }
        }
        if (in.press(Button::B)) {
            st_.panelOpen = false;
            panelAnim_.set(0);
            ctx.sfx(Sfx::Back);
        }
    }

    void openOptions(Context& ctx) {
        st_.optionsOpen = true;
        st_.optionsSel = 0;
        optionsAnim_.set(1);
        ctx.sfx(Sfx::Open);
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

    NxeState st_;
    Tween enter_;
    Smooth chan_{0, 10};
    Smooth cardAnim_[4] = {Smooth(0, 12), Smooth(0, 12), Smooth(0, 12), Smooth(0, 12)};
    Smooth panelAnim_{0, 13};
    Smooth panelSel_{0, 18};
    Smooth panelScroll_{0, 14};
    Smooth optionsAnim_{0, 16};
    float waveT_ = 0;
    float clockTimer_ = 0;
    SceneCache cache_;
    TextureId bg_ = kInvalidTexture;
    unsigned version_ = 1;
};

}  // namespace

void drawNxePreview(IRenderer& r, float time) {
    NxeState st;
    st.time = time;
    renderNxeStatic(r, st, kInvalidTexture);
    renderNxeLive(r, st);
}

std::unique_ptr<Dashboard> createNxeDashboard() { return std::unique_ptr<Dashboard>(new NxeDashboard()); }

}  // namespace nx
