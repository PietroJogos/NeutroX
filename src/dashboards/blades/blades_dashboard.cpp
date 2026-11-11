// Neutro X - Dashboard "Blades" (inspirado na geracao 2005-2008, referencia 2.0.6717.0).
//
// Recriacao original: quatro laminas verticais (Xbox Live, Jogos, Midia, Sistema).
// A lamina atual fica aberta; as anteriores dobram-se como abas a esquerda e as
// seguintes como abas a direita, com rotulos girados. Esquerda/Direita (ou LB/RB)
// "viram" as laminas; Cima/Baixo movem a barra de selecao; A abre um subpainel;
// B volta; Y abre as opcoes da interface; START abre o menu do Neutro X.
//
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

constexpr int kBlades = 4;
constexpr float kTabW = 46, kTop = 26, kBottom = 694, kEdgeL = 24, kEdgeR = 1256, kRadius = 22;

enum class Act { Detail, OpenNeutroSettings, OpenAbout };

struct BladeItem {
    Icon icon;
    const char* label;
    const char* desc;
    Act act;
};

struct BladeDef {
    const char* name;
    const char* tab;
    Color color;
    const BladeItem* items;
    int count;
};

const BladeItem kLive[] = {
    {Icon::Person, "Perfil do Jogador", "Seu cartão de jogador local: nome, pontos e lema. Tudo fica no próprio aparelho.", Act::Detail},
    {Icon::Friends, "Amigos", "Lista de amigos de demonstração. Sem conexão, todos aparecem offline.", Act::Detail},
    {Icon::Mail, "Mensagens", "Caixa de mensagens local. Nenhuma mensagem nova.", Act::Detail},
    {Icon::Trophy, "Conquistas", "Progresso de conquistas dos jogos de demonstração.", Act::Detail},
    {Icon::Download, "Downloads", "Fila de downloads. No modo offline nada é baixado.", Act::Detail},
    {Icon::Globe, "Configurações da Live", "Representação visual das opções de conexão. Nada é alterado.", Act::Detail},
};
const BladeItem kGames[] = {
    {Icon::Controller, "Biblioteca de Jogos", "Jogos de demonstração do Neutro X, com miniaturas locais.", Act::Detail},
    {Icon::Star, "Jogos Arcade", "Títulos arcade de demonstração.", Act::Detail},
    {Icon::Video, "Demos e Trailers", "Vitrine local de demonstrações.", Act::Detail},
    {Icon::Trophy, "Conquistas", "Pontos e conquistas por jogo (demonstração).", Act::Detail},
    {Icon::Clock, "Jogados Recentemente", "Últimos jogos abertos (demonstração).", Act::Detail},
};
const BladeItem kMedia[] = {
    {Icon::Music, "Música", "Biblioteca de música local de demonstração.", Act::Detail},
    {Icon::Picture, "Imagens", "Visualizador de imagens local de demonstração.", Act::Detail},
    {Icon::Video, "Vídeos", "Biblioteca de vídeos local de demonstração.", Act::Detail},
    {Icon::Disc, "Reproduzir Disco", "Nenhum disco detectado. O Neutro X não acessa a unidade de disco.", Act::Detail},
};
const BladeItem kSystem[] = {
    {Icon::Gear, "Configurações do Console", "Resumo de vídeo, áudio e idioma. Somente visualização: nada do sistema é alterado.", Act::Detail},
    {Icon::Family, "Família", "Configurações de família (representação visual).", Act::Detail},
    {Icon::Storage, "Memória", "Armazenamento usado pelo Neutro X e dispositivos (demonstração).", Act::Detail},
    {Icon::Network, "Rede", "Estado da rede: modo offline.", Act::Detail},
    {Icon::Palette, "Configurações do Neutro X", "Abre as configurações do próprio Neutro X.", Act::OpenNeutroSettings},
    {Icon::Info, "Sobre o Neutro X", "Versão, créditos e informações de segurança.", Act::OpenAbout},
};

#define NX_N(a) int(sizeof(a) / sizeof((a)[0]))
const BladeDef kDefs[kBlades] = {
    {"Xbox Live", "xbox live", Color::hex(0x63B32E), kLive, NX_N(kLive)},
    {"Jogos", "jogos", Color::hex(0x2E9466), kGames, NX_N(kGames)},
    {"Mídia", "mídia", Color::hex(0x3D7FC0), kMedia, NX_N(kMedia)},
    {"Sistema", "sistema", Color::hex(0x8A939B), kSystem, NX_N(kSystem)},
};

// ------------------------------------------------------------ subpaineis

struct DetailRow {
    Icon icon = Icon::Info;
    std::string title, sub, desc;
    float progress = -1;  // 0..1 desenha barra
    int artSeed = 0;      // != 0: arte no painel lateral
    Color c1, c2;
};

struct Detail {
    std::string title;
    std::string note;
    std::vector<DetailRow> rows;
};

Detail buildDetail(int blade, int item, Context* ctx) {
    Detail d;
    const BladeItem& it = kDefs[blade].items[item];
    d.title = it.label;
    int n = 0;
    auto row = [&d](Icon ic, std::string t, std::string s, std::string desc = "", float prog = -1) {
        DetailRow r;
        r.icon = ic;
        r.title = std::move(t);
        r.sub = std::move(s);
        r.desc = std::move(desc);
        r.progress = prog;
        d.rows.push_back(r);
    };
    auto gameRow = [&d](const DemoGame& g, const std::string& sub, float prog) {
        DetailRow r;
        r.icon = g.arcade ? Icon::Star : Icon::Controller;
        r.title = g.title;
        r.sub = sub;
        r.desc = g.description;
        r.progress = prog;
        r.artSeed = g.artSeed;
        r.c1 = g.c1;
        r.c2 = g.c2;
        d.rows.push_back(r);
    };
    const DemoProfile& p = demoProfile();
    if (blade == 0) {
        switch (item) {
            case 0:
                row(Icon::Person, p.gamertag, "Nome do jogador (local)");
                row(Icon::Trophy, std::to_string(p.points) + " pontos", "Pontuação de demonstração");
                row(Icon::Mail, p.motto, "Lema");
                row(Icon::Globe, p.zone, "Zona de jogo");
                d.note = "Perfil fictício local. Nenhuma conta online é usada.";
                break;
            case 1: {
                const DemoFriend* f = demoFriends(n);
                for (int i = 0; i < n; ++i) row(Icon::Person, f[i].name, f[i].status);
                d.note = "Modo offline: amigos de demonstração.";
                break;
            }
            case 2:
                row(Icon::Mail, "Bem-vindo ao Neutro X", "Mensagem do sistema · lida",
                    "Esta é uma caixa de mensagens local de demonstração.");
                d.note = "Nenhuma mensagem nova.";
                break;
            case 3:
            case 4:
            case 5:
                if (item == 3) {
                    const DemoGame* g = demoGames(n);
                    for (int i = 0; i < n; ++i)
                        gameRow(g[i], std::to_string(g[i].points) + " de " + std::to_string(g[i].maxPoints) + " pontos",
                                float(g[i].points) / g[i].maxPoints);
                } else if (item == 4) {
                    row(Icon::Download, "Nenhum download na fila", "Modo offline");
                } else {
                    row(Icon::Network, "Conexão", "Desativada (modo offline)");
                    row(Icon::Person, "Presença", "Invisível");
                    row(Icon::Friends, "Privacidade", "Somente amigos (visual)");
                }
                d.note = item == 3 ? "Conquistas de demonstração." : "Representação visual. Nenhuma configuração é alterada.";
                break;
        }
    } else if (blade == 1) {
        const DemoGame* g = demoGames(n);
        switch (item) {
            case 0:
                for (int i = 0; i < n; ++i) gameRow(g[i], std::string(g[i].genre) + " · " + g[i].year, -1);
                d.note = "Jogos de demonstração: abrem apenas telas informativas.";
                break;
            case 1:
                for (int i = 0; i < n; ++i)
                    if (g[i].arcade) gameRow(g[i], std::string("Arcade · ") + g[i].genre, -1);
                d.note = "Títulos arcade de demonstração.";
                break;
            case 2:
                for (int i = 0; i < 3 && i < n; ++i) gameRow(g[i], "Trailer de demonstração · 1:00", -1);
                d.note = "Vitrine local; nada é transmitido.";
                break;
            case 3:
                for (int i = 0; i < n; ++i)
                    gameRow(g[i], std::to_string(g[i].points) + " / " + std::to_string(g[i].maxPoints) + " pontos",
                            float(g[i].points) / g[i].maxPoints);
                d.note = "Pontos de demonstração.";
                break;
            case 4:
                for (int i = 0; i < 4 && i < n; ++i) gameRow(g[i], i == 0 ? "Hoje" : "Esta semana", -1);
                d.note = "Histórico de demonstração.";
                break;
        }
    } else if (blade == 2) {
        const DemoMedia* m = nullptr;
        Icon ic = Icon::Music;
        if (item == 0) { m = demoMusic(n); ic = Icon::Music; }
        else if (item == 1) { m = demoPictures(n); ic = Icon::Picture; }
        else if (item == 2) { m = demoVideos(n); ic = Icon::Video; }
        if (m) {
            for (int i = 0; i < n; ++i) {
                DetailRow r;
                r.icon = ic;
                r.title = m[i].title;
                r.sub = m[i].detail;
                r.artSeed = 300 + item * 10 + i;
                r.c1 = item == 1 ? Color::hex(0x58B4E8) : Color::hex(0x3D7FC0);
                r.c2 = Color::hex(0x10223A);
                d.rows.push_back(r);
            }
            d.note = "Mídia de demonstração. Nenhum arquivo do sistema é lido.";
        } else {
            row(Icon::Disc, "Nenhum disco", "Unidade não acessada pelo Neutro X");
            d.note = "Por segurança, o Neutro X não usa a unidade de disco.";
        }
    } else {
        switch (item) {
            case 0: {
                char res[48] = "1280 × 720";
                if (ctx && ctx->renderer)
                    std::snprintf(res, sizeof(res), "%d × %d", ctx->renderer->pixelWidth(), ctx->renderer->pixelHeight());
                row(Icon::Tv, "Resolução do Neutro X", res);
                row(Icon::Speaker, "Volume dos efeitos",
                    ctx && ctx->settings ? std::to_string(ctx->settings->sfxVolume) + "%" : "80%");
                row(Icon::Globe, "Idioma", "Português (Brasil)");
                row(Icon::Clock, "Relógio", "Hora local do aparelho");
                d.note = "Somente visualização. As configurações do console não são lidas nem alteradas.";
                break;
            }
            case 1:
                row(Icon::Family, "Controle dos pais", "Não configurado (visual)");
                row(Icon::Clock, "Temporizador de família", "Desativado (visual)");
                d.note = "Representação visual.";
                break;
            case 2: {
                std::string path = ctx && ctx->platform ? ctx->platform->dataPath() : "pasta do aplicativo";
                row(Icon::Storage, "Pasta do Neutro X", path, "Configurações, log e sons originais ficam aqui.", 0.02f);
                row(Icon::Storage, "Disco rígido (demonstração)", "120 GB · 34 GB livres", "", 0.72f);
                row(Icon::Storage, "Unidade USB (demonstração)", "16 GB · 15 GB livres", "", 0.06f);
                d.note = "Valores de dispositivos são demonstrativos.";
                break;
            }
            case 3:
                row(Icon::Network, "Conexão", "Modo offline");
                row(Icon::Globe, "Serviços online", "Não utilizados pelo Neutro X");
                d.note = "O Neutro X funciona totalmente offline.";
                break;
        }
    }
    if (d.rows.empty()) row(Icon::Info, "Sem itens", "Demonstração");
    return d;
}

// ------------------------------------------------------------ estado/visual

struct BladesState {
    float pos = 0;  // lamina animada (indice continuo)
    int blade = 0;
    int sel[kBlades] = {};
    float selAnim[kBlades] = {};
    float featureT = 1;  // 0..1 crossfade do painel de destaque
    int featurePrev = 0;
    float detailAnim = 0;  // 0 = lista, 1 = subpainel
    bool detailOpen = false;
    Detail detail;
    int detailItem = 0;
    int detailSel = 0;
    float detailSelAnim = 0;
    float detailScroll = 0;
    float enter = 1;  // 0..1 animacao de entrada
    float time = 0;
    bool graphite = false;
    bool showClock = true;
    float optionsAnim = 0;
    bool optionsOpen = false;
    int optionsSel = 0;
    std::string clock = "12:00", date = "11/11/2026";
    TextureId bg = kInvalidTexture;
};

void drawBackground(IRenderer& r, bool graphite) {
    const Rectf full{0, 0, float(kLogicalW), float(kLogicalH)};
    if (graphite) {
        r.fillGradientV(full, Color::hex(0x3C4046), Color::hex(0x1C1E22));
        r.fillRadial(640, 380, 700, Color::hex(0x6A7078, 200), Color::hex(0x6A7078, 0), full);
    } else {
        r.fillGradientV(full, Color::hex(0x9AA0A6), Color::hex(0x5E646B));
        r.fillRadial(640, 380, 700, Color::hex(0xE4E7EA), Color::hex(0xE4E7EA, 0), full);
    }
    r.strokeCircle(640, 380, 520, 3, Color(255, 255, 255, 40));
    r.strokeCircle(640, 380, 610, 2, Color(255, 255, 255, 25));
    r.strokeCircle(640, 380, 430, 2, Color(255, 255, 255, 18));
}

Rectf bladeRect(int i, float pos) {
    float d = i - pos;
    float x0o = kEdgeL + i * kTabW, x1o = kEdgeR - (kBlades - 1 - i) * kTabW;
    float x0 = x0o, x1 = x1o;
    if (d <= 0) x1 = lerpf(x1o, x0o + kTabW, ease::inOutCubic(clampf(-d, 0, 1)));
    else x0 = lerpf(x0o, x1o - kTabW, ease::inOutCubic(clampf(d, 0, 1)));
    float inset = 7.0f * clampf(std::fabs(d), 0, 3);
    return Rectf{x0, kTop + inset, x1 - x0, (kBottom - kTop) - 2 * inset};
}

void drawFeaturePanel(IRenderer& r, const Rectf& p, const BladeItem& it, int seed, Color bladeColor, float alpha) {
    if (alpha <= 0.01f) return;
    r.pushAlpha(alpha);
    r.fillRoundRect(p.offset(4, 6), 12, Color(0, 0, 0, 60));
    r.fillRoundRect(p, 12, Color::hex(0xE9EEF1));
    Rectf art = p.inset(8);
    art.h -= 120;
    drawAbstractArt(r, art, seed, brighten(bladeColor, 0.25f), brighten(bladeColor, -0.55f));
    drawIcon(r, it.icon, art.cx(), art.cy() - 10, 110, Color(255, 255, 255, 200), Color(0, 0, 0, 0));
    TextStyle t;
    t.size = 26;
    t.weight = FontWeight::Bold;
    t.color = Color::hex(0x23292F);
    r.drawText(it.label, p.x + 20, art.bottom() + 14, t);
    TextStyle d;
    d.size = 18;
    d.color = Color::hex(0x4A535C);
    drawWrapped(r, it.desc, p.x + 20, art.bottom() + 50, p.w - 40, d, 24, 3);
    r.popAlpha();
}

void drawDetailPanel(IRenderer& r, const Rectf& p, const DetailRow& row, Color bladeColor) {
    r.fillRoundRect(p.offset(4, 6), 12, Color(0, 0, 0, 60));
    r.fillRoundRect(p, 12, Color::hex(0xE9EEF1));
    Rectf art = p.inset(8);
    art.h -= 120;
    if (row.artSeed) drawAbstractArt(r, art, row.artSeed, row.c1, row.c2);
    else r.fillGradientV(art, brighten(bladeColor, 0.2f), brighten(bladeColor, -0.5f));
    drawIcon(r, row.icon, art.cx(), art.cy(), 100, Color(255, 255, 255, 200));
    TextStyle t;
    t.size = 24;
    t.weight = FontWeight::Bold;
    t.color = Color::hex(0x23292F);
    r.drawText(row.title.c_str(), p.x + 20, art.bottom() + 14, t);
    TextStyle d;
    d.size = 17;
    d.color = Color::hex(0x4A535C);
    const std::string& text = row.desc.empty() ? row.sub : row.desc;
    drawWrapped(r, text.c_str(), p.x + 20, art.bottom() + 48, p.w - 40, d, 23, 3);
}

constexpr float kOpenW = (kEdgeR - kEdgeL) - (kBlades - 1) * kTabW;

void drawContent(IRenderer& r, const BladesState& st, int bi, const Rectf& Rlive, float alpha) {
    const BladeDef& def = kDefs[bi];
    // Layout sempre na largura aberta; o recorte esconde o que nao cabe
    // enquanto a lamina abre ou fecha (nada se sobrepoe).
    const Rectf R{Rlive.x, Rlive.y, kOpenW, Rlive.h};
    const float cx = R.x + kTabW + 34, cy = R.y;
    TextStyle title;
    title.size = 44;
    title.color = Color(255, 255, 255);
    r.drawText(def.name, cx, cy + 18, title);
    // Informacoes basicas: relogio e jogador
    if (st.showClock) {
        TextStyle ck;
        ck.size = 30;
        ck.align = Align::Right;
        ck.color = Color(255, 255, 255);
        r.drawText(st.clock.c_str(), R.right() - 30, cy + 20, ck);
        ck.size = 15;
        ck.color = Color(255, 255, 255, 190);
        r.drawText(st.date.c_str(), R.right() - 30, cy + 56, ck);
    }
    TextStyle tag;
    tag.size = 18;
    tag.align = Align::Right;
    tag.color = Color(255, 255, 255, 220);
    std::string who = std::string(demoProfile().gamertag) + "  ·  offline";
    r.drawText(who.c_str(), R.right() - (st.showClock ? 150 : 30), cy + 30, tag);
    r.drawLine(cx - 10, cy + 84, R.right() - 30, cy + 84, 2, Color(255, 255, 255, 70));

    const float listX = cx - 14, listW = 520, rowH = 76, listY = cy + 96;
    const Rectf panel{cx + 540, cy + 104, R.right() - 30 - (cx + 540), 452};
    const float da = ease::inOutCubic(st.detailAnim);

    // ---- lista principal
    if (da < 0.999f) {
        r.pushAlpha(1 - da);
        r.pushOffset(-60 * da, 0);
        float sy = listY + st.selAnim[bi] * rowH;
        r.fillGradientH(Rectf{listX, sy, listW, rowH - 4}, Color(255, 255, 255, 150), Color(255, 255, 255, 0));
        r.fillRect(Rectf{listX, sy + rowH - 8, 74, 4}, Color(255, 255, 255, 235));
        TextStyle it;
        it.size = 30;
        it.color = Color(255, 255, 255);
        for (int i = 0; i < def.count; ++i) {
            float y = listY + i * rowH;
            drawIcon(r, def.items[i].icon, listX + 50, y + rowH * 0.5f - 2, 44, Color(255, 255, 255, 235), def.color);
            it.weight = i == st.sel[bi] ? FontWeight::Bold : FontWeight::Regular;
            r.drawText(def.items[i].label, listX + 112, y + rowH * 0.5f - 18, it);
            r.drawLine(listX, y + rowH - 2, listX + listW, y + rowH - 2, 1.5f, Color(255, 255, 255, 45));
        }
        // painel de destaque com crossfade
        float ft = ease::outCubic(st.featureT);
        if (ft < 1 && st.featurePrev != st.sel[bi] && st.featurePrev < def.count)
            drawFeaturePanel(r, panel, def.items[st.featurePrev], 6717 + bi * 31 + st.featurePrev, def.color, 1 - ft);
        drawFeaturePanel(r, panel.offset(0, (1 - ft) * 10), def.items[st.sel[bi]], 6717 + bi * 31 + st.sel[bi], def.color, ft);
        r.popOffset();
        r.popAlpha();
    }

    // ---- subpainel
    if (da > 0.001f && bi == st.blade) {
        r.pushAlpha(da);
        r.pushOffset(60 * (1 - da), 0);
        drawIcon(r, Icon::ArrowLeft, listX + 24, listY + 22, 26, Color(255, 255, 255, 220));
        TextStyle ht;
        ht.size = 28;
        ht.weight = FontWeight::Bold;
        ht.color = Color(255, 255, 255);
        r.drawText(st.detail.title.c_str(), listX + 52, listY + 6, ht);
        const float dRowH = 62, dY = listY + 56;
        const int visible = 6;
        r.pushClip(Rectf{listX - 4, dY - 4, listW + 8, visible * dRowH + 4});
        float sy = dY + (st.detailSelAnim - st.detailScroll) * dRowH;
        r.fillGradientH(Rectf{listX, sy, listW, dRowH - 4}, Color(255, 255, 255, 150), Color(255, 255, 255, 0));
        r.fillRect(Rectf{listX, sy + dRowH - 8, 60, 4}, Color(255, 255, 255, 235));
        for (int i = 0; i < int(st.detail.rows.size()); ++i) {
            float y = dY + (i - st.detailScroll) * dRowH;
            if (y < dY - dRowH || y > dY + visible * dRowH) continue;
            const DetailRow& row = st.detail.rows[size_t(i)];
            drawIcon(r, row.icon, listX + 34, y + dRowH * 0.5f - 2, 32, Color(255, 255, 255, 230), def.color);
            TextStyle rt;
            rt.size = 22;
            rt.color = Color(255, 255, 255);
            rt.weight = i == st.detailSel ? FontWeight::Bold : FontWeight::Regular;
            r.drawText(row.title.c_str(), listX + 72, y + 8, rt);
            TextStyle rs;
            rs.size = 15;
            rs.color = Color(255, 255, 255, 200);
            r.drawText(row.sub.c_str(), listX + 72, y + 36, rs);
            if (row.progress >= 0) {
                Rectf bar{listX + listW - 150, y + 24, 130, 8};
                r.fillRoundRect(bar, 4, Color(0, 0, 0, 60));
                r.fillRoundRect(Rectf{bar.x, bar.y, bar.w * clampf(row.progress, 0, 1), bar.h}, 4, Color(255, 255, 255, 230));
            }
            r.drawLine(listX, y + dRowH - 2, listX + listW, y + dRowH - 2, 1.2f, Color(255, 255, 255, 40));
        }
        r.popClip();
        if (!st.detail.note.empty()) {
            TextStyle nt;
            nt.size = 16;
            nt.color = Color(255, 255, 255, 200);
            r.drawText(st.detail.note.c_str(), listX, dY + visible * dRowH + 10, nt);
        }
        if (!st.detail.rows.empty())
            drawDetailPanel(r, panel, st.detail.rows[size_t(clampi(st.detailSel, 0, int(st.detail.rows.size()) - 1))], def.color);
        r.popOffset();
        r.popAlpha();
    }

    // ---- rodape
    r.drawLine(cx - 10, R.bottom() - 74, R.right() - 30, R.bottom() - 74, 2, Color(255, 255, 255, 55));
    float hy = R.bottom() - 38;
    drawHints(r, {{Button::Y, "Opções"}, {Button::Start, "Neutro X"}}, cx, hy, 32, Color(255, 255, 255));
    if (st.detailOpen)
        drawHints(r, {{Button::B, "Voltar"}, {Button::A, "Selecionar"}}, R.right() - 30, hy, 32, Color(255, 255, 255), Align::Right);
    else
        drawHints(r, {{Button::LB, "/"}, {Button::RB, "Lâminas"}, {Button::A, "Selecionar"}}, R.right() - 30, hy, 32,
                  Color(255, 255, 255), Align::Right);
    (void)alpha;
}

void drawBlade(IRenderer& r, const BladesState& st, int i, float entryOffset) {
    const BladeDef& def = kDefs[i];
    Rectf R = bladeRect(i, st.pos).offset(entryOffset, 0);
    float d = std::fabs(i - st.pos);
    float open = 1.0f - clampf(d, 0, 1);
    Color base = brighten(def.color, -0.06f * clampf(d, 0, 3));
    // sombra so nas bordas (direita e inferior): barata no renderizador em software
    if (open > 0.5f) {
        r.pushAlpha((open - 0.5f) * 2);
        r.fillRect(Rectf{R.right() - kRadius * 0.5f, R.y + 18, 8, R.h - 16}, Color(0, 0, 0, 50));
        r.fillRect(Rectf{R.x + 18, R.bottom() - 4, R.w - 16, 9}, Color(0, 0, 0, 45));
        r.popAlpha();
    }
    r.fillRoundRect(R, kRadius, brighten(base, -0.3f));
    r.pushClip(R.inset(3));
    r.fillGradientV(R, brighten(base, 0.2f), brighten(base, -0.22f));
    if (R.w > kTabW * 2) {
        // brilho diagonal sutil
        Vec2 q[4] = {{R.x + R.w * 0.35f, R.y}, {R.x + R.w * 0.62f, R.y}, {R.x + R.w * 0.42f, R.bottom()}, {R.x + R.w * 0.15f, R.bottom()}};
        r.fillConvex(q, 4, Color(255, 255, 255, 16));
    }
    r.popClip();
    // Aba (faixa esquerda) com rotulo girado
    r.pushClip(Rectf{R.x, R.y, kTabW, R.h});
    r.fillRoundRect(R, kRadius, brighten(base, -0.2f));
    r.popClip();
    r.drawLine(R.x + kTabW, R.y + 14, R.x + kTabW, R.bottom() - 14, 2, Color(255, 255, 255, 80));
    r.strokeRoundRect(R, kRadius, 2.5f, Color(255, 255, 255, 110));
    TextStyle tab;
    tab.size = 28;
    tab.rotated90 = true;
    tab.tracking = 1.5f;
    tab.color = Color(255, 255, 255, uint8_t(170 + 70 * open));
    r.drawText(def.tab, R.x + (kTabW - 28) * 0.5f + 1, R.y + 44, tab);

    if (open > 0.01f) {
        r.pushClip(Rectf{R.x + kTabW + 2, R.y + 2, R.w - kTabW - 4, R.h - 4});
        r.pushAlpha(ease::outCubic(open));
        drawContent(r, st, i, R, open);
        r.popAlpha();
        r.popClip();
    }
}

void drawOptions(IRenderer& r, const BladesState& st) {
    float a = ease::outCubic(st.optionsAnim);
    if (a <= 0.01f) return;
    r.pushAlpha(a);
    r.fillRect(Rectf{0, 0, 1280, 720}, Color(0, 0, 0, 110));
    Rectf box = Rectf{390, 220, 500, 280}.scaledAboutCenter(0.92f + 0.08f * a);
    const Color c = kDefs[st.blade].color;
    r.fillRoundRect(box.offset(5, 8), 18, Color(0, 0, 0, 80));
    r.fillRoundRect(box, 18, brighten(c, -0.35f));
    r.pushClip(box.inset(3));
    r.fillGradientV(box, brighten(c, 0.15f), brighten(c, -0.3f));
    r.popClip();
    r.strokeRoundRect(box, 18, 2.5f, Color(255, 255, 255, 140));
    TextStyle t;
    t.size = 30;
    t.color = Color(255, 255, 255);
    r.drawText("Opções da interface", box.x + 30, box.y + 24, t);
    const char* labels[3] = {"Fundo", "Relógio", "Fechar"};
    std::string vals[3] = {st.graphite ? "Grafite" : "Clássico", st.showClock ? "Mostrar" : "Ocultar", ""};
    for (int i = 0; i < 3; ++i) {
        Rectf row{box.x + 20, box.y + 80 + i * 56.0f, box.w - 40, 48};
        if (i == st.optionsSel) r.fillGradientH(row, Color(255, 255, 255, 150), Color(255, 255, 255, 20));
        TextStyle ls;
        ls.size = 24;
        ls.color = Color(255, 255, 255);
        ls.weight = i == st.optionsSel ? FontWeight::Bold : FontWeight::Regular;
        r.drawText(labels[i], row.x + 16, row.y + 12, ls);
        ls.align = Align::Right;
        r.drawText(vals[i].c_str(), row.right() - 16, row.y + 12, ls);
    }
    r.popAlpha();
}

void renderBlades(IRenderer& r, const BladesState& st, TextureId bg) {
    float e = ease::outCubic(st.enter);
    if (bg != kInvalidTexture) r.drawTexture(bg, Rectf{0, 0, float(kLogicalW), float(kLogicalH)});
    else drawBackground(r, st.graphite);
    // Desenha das laminas mais distantes para a atual.
    int order[kBlades];
    for (int i = 0; i < kBlades; ++i) order[i] = i;
    for (int a = 0; a < kBlades; ++a)
        for (int b = a + 1; b < kBlades; ++b)
            if (std::fabs(order[b] - st.pos) > std::fabs(order[a] - st.pos)) std::swap(order[a], order[b]);
    for (int k = 0; k < kBlades; ++k) {
        int i = order[k];
        float local = ease::outCubic(clampf((st.enter - 0.08f * i) / 0.7f, 0, 1));
        float off = (1 - local) * (900.0f + 60 * i);
        if (e < 1) r.pushAlpha(local);
        drawBlade(r, st, i, off);
        if (e < 1) r.popAlpha();
    }
    drawOptions(r, st);
}

// ------------------------------------------------------------ dashboard

class BladesDashboard : public Dashboard {
public:
    BladesDashboard() : Dashboard(DashboardId::Blades) {}

    bool load(Context& ctx, std::string& error) override {
        if (!cache_.init(*ctx.renderer) || !rebuildBackground(ctx)) {
            error = "falha ao criar as texturas do Blades";
            return false;
        }
        return true;
    }

    void unload(Context& ctx) override {
        if (st_.bg != kInvalidTexture) ctx.renderer->destroyTexture(st_.bg);
        st_.bg = kInvalidTexture;
        cache_.release(*ctx.renderer);  // libera o canvas e suas texturas
        cacheBg_ = kInvalidTexture;
    }

    void onEnter(Context& ctx) override {
        enter_.start(0.9f);
        st_.enter = 0;
        pos_.snap(float(st_.blade));
        updateClock(ctx);
    }

    std::string debugState() const override {
        char b[128];
        std::snprintf(b, sizeof(b), "blade=%d sel=%d detail=%d detailSel=%d options=%d graphite=%d clock=%d", st_.blade,
                      st_.sel[st_.blade], st_.detailOpen ? 1 : 0, st_.detailSel, st_.optionsOpen ? 1 : 0,
                      st_.graphite ? 1 : 0, st_.showClock ? 1 : 0);
        return b;
    }

    void update(Context& ctx, const InputFrame& in, float dt) override {
        enter_.update(dt);
        st_.enter = enter_.progress();
        st_.time += dt;
        pos_.update(dt);
        st_.pos = pos_.value;
        for (int i = 0; i < kBlades; ++i) {
            selAnim_[i].update(dt);
            st_.selAnim[i] = selAnim_[i].value;
        }
        detailAnim_.update(dt);
        st_.detailAnim = detailAnim_.value;
        detailSelAnim_.update(dt);
        st_.detailSelAnim = detailSelAnim_.value;
        detailScroll_.update(dt);
        st_.detailScroll = detailScroll_.value;
        optionsAnim_.update(dt);
        st_.optionsAnim = optionsAnim_.value;
        feature_.update(dt);
        st_.featureT = feature_.progress();
        clockTimer_ -= dt;
        if (clockTimer_ <= 0) updateClock(ctx);

        for (int b = 0; b < kButtonCount; ++b)
            if (in.pressed[b]) { ++version_; break; }
        if (st_.optionsOpen) { updateOptions(ctx, in); return; }
        if (st_.detailOpen) { updateDetail(ctx, in); return; }
        updateList(ctx, in);
    }

    void render(Context& ctx, IRenderer& r) override {
        (void)ctx;
        // Ao vivo usa a textura de fundo do renderizador principal; em cache, a copia do canvas.
        cache_.draw(r, animating(), version_, [this, &r](IRenderer& rr) { renderBlades(rr, st_, &rr == &r ? st_.bg : cacheBg_); });
    }

private:
    bool animating() const {
        if (!enter_.done() || !feature_.done() || !pos_.settled() || !detailAnim_.settled() || !detailSelAnim_.settled() ||
            !detailScroll_.settled() || !optionsAnim_.settled())
            return true;
        for (const Smooth& s : selAnim_)
            if (!s.settled()) return true;
        return false;
    }

    bool rebuildBackground(Context& ctx) {
        IRenderer& r = *ctx.renderer;
        if (st_.bg != kInvalidTexture) r.destroyTexture(st_.bg);
        SoftRenderer canvas(r.pixelWidth(), r.pixelHeight());
        canvas.beginFrame();
        drawBackground(canvas, st_.graphite);
        st_.bg = r.createTexture(canvas.pixelWidth(), canvas.pixelHeight(), canvas.pixels());
        if (SoftRenderer* cv = cache_.canvas()) {
            if (cacheBg_ != kInvalidTexture) cv->destroyTexture(cacheBg_);
            cacheBg_ = cv->createTexture(canvas.pixelWidth(), canvas.pixelHeight(), canvas.pixels());
        }
        ++version_;
        return st_.bg != kInvalidTexture;
    }

    void updateClock(Context& ctx) {
        int h = 0, m = 0, d = 1, mo = 1, y = 2026;
        ctx.platform->localTime(h, m, d, mo, y);
        char b[32];
        std::snprintf(b, sizeof(b), "%02d:%02d", h, m);
        if (st_.clock != b) ++version_;
        st_.clock = b;
        std::snprintf(b, sizeof(b), "%02d/%02d/%04d", d, mo, y);
        st_.date = b;
        clockTimer_ = 1.0f;
    }

    void switchBlade(Context& ctx, int delta) {
        int nb = st_.blade + delta;
        if (nb < 0 || nb >= kBlades) { ctx.sfx(Sfx::Error, 0.6f); return; }
        st_.blade = nb;
        pos_.set(float(nb));
        st_.featurePrev = st_.sel[nb];
        feature_.t = 1;
        ctx.sfx(Sfx::Tab);
    }

    void updateList(Context& ctx, const InputFrame& in) {
        const BladeDef& def = kDefs[st_.blade];
        int& sel = st_.sel[st_.blade];
        if (in.press(Button::Right) || in.press(Button::RB)) switchBlade(ctx, +1);
        else if (in.press(Button::Left) || in.press(Button::LB)) switchBlade(ctx, -1);
        int before = sel;
        if (in.press(Button::Down)) sel = sel + 1 < def.count ? sel + 1 : sel;
        if (in.press(Button::Up)) sel = sel > 0 ? sel - 1 : sel;
        if (sel != before) {
            ctx.sfx(Sfx::Navigate);
            selAnim_[st_.blade].set(float(sel));
            st_.featurePrev = before;
            feature_.start(0.25f);
        } else if ((in.press(Button::Down) || in.press(Button::Up))) {
            ctx.sfx(Sfx::Error, 0.4f);
        }
        if (in.press(Button::A)) {
            const BladeItem& it = def.items[sel];
            switch (it.act) {
                case Act::Detail:
                    st_.detail = buildDetail(st_.blade, sel, &ctx);
                    st_.detailItem = sel;
                    st_.detailSel = 0;
                    detailSelAnim_.snap(0);
                    detailScroll_.snap(0);
                    st_.detailOpen = true;
                    detailAnim_.set(1);
                    ctx.sfx(Sfx::Confirm);
                    break;
                case Act::OpenNeutroSettings:
                    ctx.sfx(Sfx::Confirm);
                    ctx.app->openSettings();
                    break;
                case Act::OpenAbout:
                    ctx.sfx(Sfx::Confirm);
                    ctx.app->openAbout();
                    break;
            }
        }
        if (in.press(Button::X)) {
            ctx.sfx(Sfx::Open, 0.7f);
            ctx.app->toast(std::string(def.items[sel].label) + ": " + def.items[sel].desc);
        }
        if (in.press(Button::Y)) openOptions(ctx);
        if (in.press(Button::B)) {
            if (st_.blade != 0) switchBlade(ctx, -st_.blade);  // volta para a primeira lamina
            else { ctx.sfx(Sfx::Back, 0.6f); ctx.app->toast("Pressione START para o menu do Neutro X"); }
        }
    }

    void updateDetail(Context& ctx, const InputFrame& in) {
        int n = int(st_.detail.rows.size());
        int before = st_.detailSel;
        if (in.press(Button::Down) && st_.detailSel + 1 < n) st_.detailSel++;
        if (in.press(Button::Up) && st_.detailSel > 0) st_.detailSel--;
        if (before != st_.detailSel) {
            ctx.sfx(Sfx::Navigate);
            detailSelAnim_.set(float(st_.detailSel));
            const int visible = 6;
            float scroll = detailScroll_.target;
            if (st_.detailSel < scroll) scroll = float(st_.detailSel);
            if (st_.detailSel > scroll + visible - 1) scroll = float(st_.detailSel - visible + 1);
            detailScroll_.set(scroll);
        }
        if (in.press(Button::A) && n > 0) {
            const DetailRow& row = st_.detail.rows[size_t(st_.detailSel)];
            ctx.sfx(Sfx::Open, 0.8f);
            ctx.app->toast(row.title + ": item de demonstração (somente informativo)");
        }
        if (in.press(Button::Y)) openOptions(ctx);
        if (in.press(Button::B) || in.press(Button::Left)) {
            st_.detailOpen = false;
            detailAnim_.set(0);
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
        bool toggle = in.press(Button::A) || in.press(Button::Left) || in.press(Button::Right);
        if (toggle && st_.optionsSel == 0) {
            st_.graphite = !st_.graphite;
            rebuildBackground(ctx);
            ctx.sfx(Sfx::Tab);
        } else if (toggle && st_.optionsSel == 1) {
            st_.showClock = !st_.showClock;
            ctx.sfx(Sfx::Tab);
        } else if ((in.press(Button::A) && st_.optionsSel == 2) || in.press(Button::B) || in.press(Button::Y)) {
            st_.optionsOpen = false;
            optionsAnim_.set(0);
            ctx.sfx(Sfx::Back);
        }
    }

    BladesState st_;
    Tween enter_;
    Tween feature_;
    Smooth pos_{0, 9};
    Smooth selAnim_[kBlades] = {Smooth(0, 18), Smooth(0, 18), Smooth(0, 18), Smooth(0, 18)};
    Smooth detailAnim_{0, 14};
    Smooth detailSelAnim_{0, 18};
    Smooth detailScroll_{0, 14};
    Smooth optionsAnim_{0, 16};
    float clockTimer_ = 0;
    SceneCache cache_;
    TextureId cacheBg_ = kInvalidTexture;
    unsigned version_ = 1;
};

}  // namespace

void drawBladesPreview(IRenderer& r, float time) {
    BladesState st;
    st.time = time;
    st.sel[0] = 0;
    st.featureT = 1;
    renderBlades(r, st, kInvalidTexture);
}

std::unique_ptr<Dashboard> createBladesDashboard() { return std::unique_ptr<Dashboard>(new BladesDashboard()); }

}  // namespace nx
