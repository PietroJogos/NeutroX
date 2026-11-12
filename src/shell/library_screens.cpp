// Neutro X - Meus Jogos: biblioteca (carrossel/grade), detalhes do jogo,
// configuracoes de pastas, navegador de armazenamento e teclado na tela.
//
// A biblioteca e unica; a aparencia segue a interface ativa (ou a ultima
// aberta): Blades, NXE, Kinect, Metro V1, Metro V2 ou o proprio Neutro X.
#include <algorithm>
#include <cmath>
#include <ctime>
#include <map>
#include <string>
#include "core/anim.h"
#include "core/fsutil.h"
#include "library/title_meta.h"
#include "shell/screens.h"
#include "shell/shell_common.h"
#include "system/launcher.h"

namespace nx {

std::string fitTextShared(IRenderer& r, const std::string& s, const TextStyle& st, float maxW);

namespace {

// ------------------------------------------------------------------ estilo por interface

struct LibStyle {
    const char* title;     // "Meus Jogos" / "meus jogos"
    Color bgTop, bgBottom;
    Color text, sub, accent, panel, onAccent;
    float radius;
    bool lightWeight;       // Metro: titulos finos e em minusculas
    bool dark;              // fundo escuro
};

LibStyle styleFor(DashboardId id, Color neutroAccent) {
    switch (id) {
        case DashboardId::Blades:
            return {"Meus Jogos", Color::hex(0x2A2A2A), Color::hex(0x050505), Color(255, 255, 255), Color(170, 170, 170),
                    Color::hex(0xF08A24), Color(18, 18, 18, 230), Color(20, 20, 20), 4, false, true};
        case DashboardId::Nxe:
            return {"Meus Jogos", Color::hex(0x3E9BE0), Color::hex(0xE4ECF2), Color::hex(0x2E3640), Color::hex(0x5A6068),
                    Color::hex(0x6EB52E), Color(255, 255, 255, 220), Color(255, 255, 255), 8, false, false};
        case DashboardId::Kinect:
            return {"MEUS JOGOS", Color::hex(0xF6F8F4), Color::hex(0xD6DDD0), Color::hex(0x2B3A2A), Color::hex(0x5E6A5C),
                    Color::hex(0x8CC63A), Color(255, 255, 255, 230), Color(255, 255, 255), 2, false, false};
        case DashboardId::MetroV1:
            return {"meus jogos", Color::hex(0x202020), Color::hex(0x121212), Color(255, 255, 255), Color(160, 160, 160),
                    Color::hex(0x1F8A1F), Color(32, 32, 32, 240), Color(255, 255, 255), 0, true, true};
        case DashboardId::MetroV2:
            return {"meus jogos", Color::hex(0x1E2024), Color::hex(0x0A0B0C), Color(255, 255, 255), Color(140, 145, 152),
                    Color::hex(0x2BA52B), Color(36, 39, 43, 240), Color(255, 255, 255), 0, true, true};
        default:
            return {"Meus Jogos", Color::hex(0x101820), Color::hex(0x05080C), Color(255, 255, 255), Color(160, 170, 182),
                    neutroAccent, Color(20, 28, 38, 235), Color(255, 255, 255), 10, false, true};
    }
}

void drawStyleBackground(IRenderer& r, const LibStyle& s, float time) {
    r.fillGradientV(Rectf{0, 0, 1280, 720}, s.bgTop, s.bgBottom);
    // faixa de luz suave (barata: um gradiente horizontal)
    float x = std::fmod(time * 40.0f, 1600.0f) - 300.0f;
    const uint8_t a = s.dark ? 16 : 22;
    r.fillGradientH(Rectf{x, 0, 150, 720}, s.accent.withAlpha(0), s.accent.withAlpha(a));
    r.fillGradientH(Rectf{x + 150, 0, 150, 720}, s.accent.withAlpha(a), s.accent.withAlpha(0));
}

std::string titleText(const LibStyle& s, const std::string& t) {
    if (!s.lightWeight) return t;
    std::string o = t;
    for (char& c : o)
        if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
    return o;
}

std::string dateText(int64_t t) {
    if (t <= 0) return "—";
    std::time_t tt = std::time_t(t);
    std::tm tmv{};
#ifdef _WIN32
    localtime_s(&tmv, &tt);
#else
    localtime_r(&tt, &tmv);
#endif
    char b[32];
    std::snprintf(b, sizeof b, "%02d/%02d/%04d", tmv.tm_mday, tmv.tm_mon + 1, tmv.tm_year + 1900);
    return b;
}

// Capa (textura) ou arte provisoria identificada como "sem capa".
void drawCover(Context& ctx, IRenderer& r, const GameEntry& g, const Rectf& rc, const LibStyle& s, float alpha,
               std::map<std::string, double>& seenAt) {
    TextureId tex = ctx.app->coverTextures().get(r, g.cover);
    const uint32_t h = g.titleId * 2654435761u;
    Color c1(uint8_t(60 + (h >> 8) % 150), uint8_t(60 + (h >> 16) % 150), uint8_t(60 + (h >> 24) % 150));
    r.pushAlpha(alpha);
    r.fillRect(rc.offset(4, 7), Color(0, 0, 0, s.dark ? 120 : 60));
    float fade = 1;
    if (tex != kInvalidTexture) {
        auto it = seenAt.find(g.cover);
        if (it == seenAt.end()) it = seenAt.emplace(g.cover, ctx.time).first;
        fade = clampf(float(ctx.time - it->second) / 0.35f, 0, 1);  // a capa entra suavemente
    }
    if (tex == kInvalidTexture || fade < 1) {
        drawAbstractArt(r, rc, int(1000 + h % 90000), c1, Color(uint8_t(c1.r / 4), uint8_t(c1.g / 4), uint8_t(c1.b / 4)));
        drawIcon(r, g.kind == GameKind::Xbla ? Icon::Star : g.kind == GameKind::App ? Icon::Apps : Icon::Controller, rc.cx(),
                 rc.y + rc.h * 0.36f, rc.w * 0.3f, Color(255, 255, 255, 200));
        TextStyle ts;
        ts.size = std::max(12.0f, rc.w * 0.075f);
        ts.color = Color(255, 255, 255);
        ts.weight = FontWeight::Bold;
        drawWrapped(r, g.displayName().c_str(), rc.x + rc.w * 0.08f, rc.y + rc.h * 0.6f, rc.w * 0.84f, ts, ts.size * 1.25f, 3);
        if (rc.w > 140) {
            TextStyle ns;
            ns.size = std::max(11.0f, rc.w * 0.05f);
            ns.color = Color(255, 255, 255, 170);
            r.drawText(g.cover.empty() ? "sem capa" : "carregando capa…", rc.x + rc.w * 0.08f, rc.bottom() - ns.size * 1.8f, ns);
        }
    }
    if (tex != kInvalidTexture) r.drawTexture(tex, rc, fade);
    if (!g.available) {
        r.fillRect(rc, Color(0, 0, 0, 140));
        TextStyle us;
        us.size = std::max(12.0f, rc.w * 0.07f);
        us.weight = FontWeight::Bold;
        us.align = Align::Center;
        us.color = Color(255, 210, 120);
        r.drawText("INDISPONÍVEL", rc.cx(), rc.cy() - us.size * 0.5f, us);
    }
    if (g.favorite) {
        r.fillCircle(rc.right() - 16, rc.y + 16, 12, s.accent);
        drawIcon(r, Icon::Star, rc.right() - 16, rc.y + 16, 14, Color(255, 255, 255));
    }
    r.popAlpha();
}

const char* kFilterNames[] = {"Todos", "Favoritos", "Recentes", "Indisponíveis", "Pesquisa"};
constexpr int kFilterN = 5;

// ------------------------------------------------------------------ Meus Jogos

class LibraryScreen : public Screen {
public:
    const char* name() const override { return "Library"; }
    bool wantsGlobalMenu() const override { return true; }
    void onEnter(Context& ctx) override {
        enter_.start(0.45f);
        style_ = ctx.app->libraryStyle();
        rebuild(ctx);
        pos_.snap(0);
    }
    void onExit(Context&) override {}

    void rebuild(Context& ctx) {
        Library& lib = ctx.app->library();
        std::string keepId = sel_ >= 0 && sel_ < int(view_.size()) ? lib.games()[size_t(view_[size_t(sel_)])].id : "";
        view_ = lib.view(Library::Filter(filter_), query_);
        ver_ = lib.version();
        sel_ = 0;
        for (size_t i = 0; i < view_.size(); ++i)
            if (lib.games()[size_t(view_[i])].id == keepId) sel_ = int(i);
        if (sel_ >= int(view_.size())) sel_ = std::max(0, int(view_.size()) - 1);
        pos_.set(float(sel_));
    }

    void update(Context& ctx, const InputFrame& in, float dt) override {
        enter_.update(dt);
        pos_.update(dt);
        Library& lib = ctx.app->library();
        if (lib.version() != ver_) rebuild(ctx);
        const int n = int(view_.size());
        const bool grid = lib.options().view == 1;
        const int cols = 6;
        int before = sel_;
        if (n > 0) {
            if (grid) {
                if (in.press(Button::Right) && sel_ + 1 < n) sel_++;
                if (in.press(Button::Left) && sel_ > 0) sel_--;
                if (in.press(Button::Down) && sel_ + cols < n) sel_ += cols;
                else if (in.press(Button::Down) && sel_ / cols < (n - 1) / cols) sel_ = n - 1;
                if (in.press(Button::Up) && sel_ >= cols) sel_ -= cols;
            } else {
                if (in.press(Button::Right)) sel_ = (sel_ + 1) % n;  // carrossel da a volta
                if (in.press(Button::Left)) sel_ = (sel_ + n - 1) % n;
            }
        }
        if (sel_ != before) {
            ctx.sfx(Sfx::Navigate);
            // Ao dar a volta no carrossel o deslocamento e curto (nao percorre a lista inteira).
            if (!grid && n >= 9 && std::abs(sel_ - before) > 1) pos_.snap(float(sel_ + (sel_ > before ? -1 : 1)));
            pos_.set(float(sel_));
        }
        if (in.press(Button::LB) || in.press(Button::RB)) {
            int f = filter_ + (in.press(Button::RB) ? 1 : -1);
            if (f >= 0 && f < kFilterN) {
                if (f == int(Library::Filter::Search) && query_.empty()) {
                    openSearch(ctx);
                } else {
                    filter_ = f;
                    sel_ = 0;
                    rebuild(ctx);
                    pos_.snap(0);
                    ctx.sfx(Sfx::Tab);
                }
            } else {
                ctx.sfx(Sfx::Error, 0.5f);
            }
        }
        // (o filtro pode ter mudado acima: usa a lista atual, nao 'n')
        const GameEntry* cur = sel_ >= 0 && sel_ < int(view_.size()) ? &lib.games()[size_t(view_[size_t(sel_)])] : nullptr;
        if (in.press(Button::A)) {
            if (cur) { ctx.sfx(Sfx::Confirm); ctx.app->openGameDetails(cur->id); }
            else { ctx.sfx(Sfx::Open); ctx.app->openLibrarySettings(); }
        }
        if (in.press(Button::X) && cur) {
            const bool fav = !cur->favorite;
            lib.toggleFavorite(cur->id);
            ctx.sfx(Sfx::Confirm, 0.8f);
            ctx.app->toast(fav ? "Adicionado aos favoritos" : "Removido dos favoritos");
        }
        if (in.press(Button::Y)) openOptions(ctx);
        if (in.press(Button::B)) {
            ctx.sfx(Sfx::Back);
            ctx.app->pop();
        }
    }

    void openSearch(Context& ctx) {
        ctx.sfx(Sfx::Open);
        ctx.app->push(createKeyboardScreen("Pesquisar jogos", query_, [this](Context& c, const std::string& text) {
            query_ = text;
            filter_ = text.empty() ? int(Library::Filter::All) : int(Library::Filter::Search);
            sel_ = 0;
            rebuild(c);
            pos_.snap(0);
        }));
    }

    void openOptions(Context& ctx) {
        Library& lib = ctx.app->library();
        std::vector<ChoiceOption> o = {
            {std::string("Visualização: ") + (lib.options().view == 1 ? "grade (trocar para carrossel)" : "carrossel (trocar para grade)"), "", true},
            {"Pesquisar…", query_.empty() ? "por nome" : "atual: " + query_, true},
            {"Atualizar biblioteca", ctx.app->libraryScanning() ? "busca em andamento" : "procurar jogos agora nas pastas", !ctx.app->libraryScanning()},
            {"Gerenciar pastas", "Configurações > Jogos", true},
            {"Adicionar jogo manualmente…", "escolher um default.xex ou pacote GOD/XBLA", true},
            {"Baixar capas que faltam", "XboxUnity (precisa de internet)", true},
            {"Importar capas da pasta capas_importar", "arquivos <TitleID>.jpg ou .png", true},
            {"Cancelar", "", true},
        };
        ctx.sfx(Sfx::Open);
        ctx.app->push(createChoiceDialog("Opções de Meus Jogos", "", std::move(o), [this](Context& c, int i) {
            Library& l = c.app->library();
            switch (i) {
                case 0: l.options().view = l.options().view == 1 ? 0 : 1; l.markDirty(); pos_.snap(float(sel_)); break;
                case 1: openSearch(c); break;
                case 2: if (c.app->startLibraryScan(true)) c.app->toast("Procurando jogos…"); break;
                case 3: c.app->openLibrarySettings(); break;
                case 4: openManualAdd(c); break;
                case 5: {
                    int k = 0;
                    for (const GameEntry& g : l.games())
                        if (g.available && g.duplicateOf.empty() && (g.cover.empty() || g.coverSource == 3)) {
                            c.app->requestCover(g.id, false, false);
                            ++k;
                        }
                    c.app->toast(k ? "Baixando " + std::to_string(k) + " capas" : "Nenhuma capa faltando");
                    break;
                }
                case 6: {
                    int k = c.app->importCovers();
                    c.app->toast(k ? std::to_string(k) + " capas importadas" : "Nenhuma capa válida em " + c.app->coverService().importDir());
                    break;
                }
                default: break;
            }
        }));
    }

    static void openManualAdd(Context& ctx) {
        ctx.app->push(createFolderBrowser(BrowseMode::PickTitle, "Adicionar jogo manualmente", [](Context& c, const std::string& path) {
            std::string why, id;
            if (c.app->library().addManual(path, &why, &id)) {
                c.app->toast("Adicionado: " + c.app->library().find(id)->displayName());
                if (c.app->library().options().autoCovers) c.app->requestCover(id, false, false);
            } else {
                c.sfx(Sfx::Error);
                c.app->toast("Não adicionado: " + why);
            }
        }));
    }

    void render(Context& ctx, IRenderer& r) override {
        const LibStyle s = styleFor(style_, ctx.accent());
        drawStyleBackground(r, s, float(ctx.time));
        Library& lib = ctx.app->library();
        const float e = ease::outCubic(enter_.progress());
        r.pushAlpha(e);
        // Cabecalho
        TextStyle t;
        t.size = 46;
        t.weight = s.lightWeight ? FontWeight::Light : FontWeight::Bold;
        t.color = s.text;
        r.drawText(s.title, 64, 38, t);
        TextStyle cs;
        cs.size = 18;
        cs.color = s.sub;
        const int total = int(lib.view(Library::Filter::All).size());
        std::string count = std::to_string(total) + (total == 1 ? " jogo" : " jogos");
        if (filter_ == int(Library::Filter::Search)) count += "  ·  pesquisa: \"" + query_ + "\" (" + std::to_string(view_.size()) + ")";
        r.drawText(count.c_str(), 66, 96, cs);
        drawClock(ctx, r, 1224, 40, s.text);
        // Filtros
        TextStyle fs;
        fs.size = 22;
        float x = 66;
        for (int i = 0; i < kFilterN; ++i) {
            const bool on = i == filter_;
            fs.weight = on ? FontWeight::Bold : (s.lightWeight ? FontWeight::Light : FontWeight::Regular);
            fs.color = on ? s.text : s.sub;
            std::string label = titleText(s, kFilterNames[i]);
            r.drawText(label.c_str(), x, 132, fs);
            const float w = r.measureText(label.c_str(), fs);
            if (on) r.fillRect(Rectf{x, 162, w, 3}, s.accent);
            x += w + 34;
        }
        // Estado da busca/capas
        std::string status;
        if (ctx.app->libraryScanning())
            status = "Procurando jogos… " + std::to_string(ctx.app->libraryScanFolders()) + " pastas";
        else if (int p = ctx.app->coverService().pending())
            status = "Baixando capas: " + std::to_string(p);
        if (!status.empty()) {
            drawSpinner(r, 1210, 142, 9, float(ctx.time), s.accent);
            TextStyle ss = cs;
            ss.align = Align::Right;
            r.drawText(status.c_str(), 1190, 132, ss);
        }

        if (view_.empty()) {
            renderEmpty(ctx, r, s);
        } else if (lib.options().view == 1) {
            renderGrid(ctx, r, s);
        } else {
            renderCarousel(ctx, r, s);
        }
        r.fillRect(Rectf{0, 668, 1280, 52}, Color(0, 0, 0, s.dark ? 110 : 40));
        drawHints(r, {{Button::A, view_.empty() ? "Gerenciar pastas" : "Detalhes"}, {Button::X, "Favoritar"}, {Button::Y, "Opções"},
                      {Button::LB, "/"}, {Button::RB, "Filtro"}, {Button::B, "Voltar"}},
                  56, 694, 30, s.dark ? Color(220, 224, 230) : Color(40, 46, 52));
        r.popAlpha();
    }

    void renderEmpty(Context& ctx, IRenderer& r, const LibStyle& s) {
        Library& lib = ctx.app->library();
        Rectf box{240, 230, 800, 300};
        r.fillRoundRect(box, s.radius + 6, s.panel);
        drawIcon(r, Icon::Controller, 640, 300, 70, s.accent);
        TextStyle t;
        t.size = 28;
        t.weight = FontWeight::Bold;
        t.align = Align::Center;
        t.color = s.dark ? Color(255, 255, 255) : s.text;
        const bool searching = filter_ != int(Library::Filter::All);
        r.drawText(searching ? "Nada neste filtro" : (lib.folders().empty() ? "Nenhuma pasta de jogos" : "Nenhum jogo encontrado"), 640,
                   360, t);
        TextStyle b;
        b.size = 19;
        b.color = s.dark ? Color(190, 198, 208) : s.sub;
        const char* msg = searching ? "Troque o filtro com LB/RB."
                          : lib.folders().empty()
                              ? "Pressione A para adicionar uma pasta (ex.: Hdd1:\\Games ou Usb0:\\Games). Só títulos reais, lidos do cabeçalho, entram."
                              : "Os títulos precisam ter default.xex (jogo extraído) ou ser pacotes GOD/XBLA. Veja o log da última busca.";
        drawWrapped(r, msg, 290, 410, 700, b, 27, 3);
        if (!lib.lastScanSummary().empty()) {
            TextStyle l = b;
            l.size = 15;
            r.drawText(("Última busca: " + lib.lastScanSummary()).c_str(), 290, 492, l);
        }
    }

    void renderCarousel(Context& ctx, IRenderer& r, const LibStyle& s) {
        Library& lib = ctx.app->library();
        const int n = int(view_.size());
        const float p = pos_.value;
        const float cy = 372, bigW = 250, bigH = 350, step = 220;
        // Desenha de fora para dentro (o central por cima).
        std::vector<int> order;
        for (int d = -4; d <= 4; ++d) order.push_back(d);
        std::sort(order.begin(), order.end(), [](int a, int b) { return std::abs(a) > std::abs(b); });
        // Com poucos jogos nao repete capas (so da a volta visualmente com 9 ou mais).
        const bool wrap = n >= 9;
        for (int d : order) {
            int idx = int(std::lround(p)) + d;
            if (wrap) idx = ((idx % n) + n) % n;
            else if (idx < 0 || idx >= n) continue;
            float off = float(int(std::lround(p)) + d) - p;
            if (std::fabs(off) > 4.2f) continue;
            const float k = 1.0f - std::min(1.0f, std::fabs(off)) * 0.3f - std::max(0.0f, std::fabs(off) - 1) * 0.06f;
            const float w = bigW * k, h = bigH * k;
            const float x = 640 + off * step - w * 0.5f + (off > 0 ? 40 : off < 0 ? -40 : 0) * std::min(1.0f, std::fabs(off));
            Rectf rc{x, cy - h * 0.5f, w, h};
            const float a = clampf(1.15f - std::fabs(off) * 0.22f, 0, 1);
            drawCover(ctx, r, lib.games()[size_t(view_[size_t(idx)])], rc, s, a, seenAt_);
            if (std::fabs(off) < 0.5f) r.strokeRect(rc.inset(-4), 3, s.accent);
        }
        // Informacoes do selecionado
        const GameEntry& g = lib.games()[size_t(view_[size_t(sel_)])];
        TextStyle nt;
        nt.size = 30;
        nt.weight = s.lightWeight ? FontWeight::Regular : FontWeight::Bold;
        nt.align = Align::Center;
        nt.color = s.text;
        r.drawText(fitTextShared(r, g.displayName(), nt, 1100).c_str(), 640, 566, nt);
        TextStyle it;
        it.size = 17;
        it.align = Align::Center;
        it.color = s.sub;
        std::string info = std::string("Title ID ") + hex8(g.titleId) + "  ·  " + gameKindName(g.kind) +
                           (g.available ? "" : "  ·  indisponível") + "  ·  " + std::to_string(sel_ + 1) + "/" + std::to_string(view_.size());
        r.drawText(info.c_str(), 640, 608, it);
        TextStyle pt = it;
        pt.size = 14;
        r.drawText(fitTextShared(r, localPathToConsole(g.path, ctx.app->storageRootsCached()), pt, 1100).c_str(), 640, 634, pt);
    }

    void renderGrid(Context& ctx, IRenderer& r, const LibStyle& s) {
        Library& lib = ctx.app->library();
        const int n = int(view_.size()), cols = 6;
        const float cw = 168, ch = 236, gx = 64, gy = 190, sx = 192, sy = 300;
        // rolagem suave por linha
        const float rowF = std::floor(pos_.value / cols);
        const float targetRow = float(sel_ / cols);
        scrollRow_ += (std::max(0.0f, targetRow - 0.0f) - scrollRow_) * 0.25f;
        (void)rowF;
        r.pushClip(Rectf{0, 176, 1280, 490});
        for (int i = 0; i < n; ++i) {
            const int row = i / cols, col = i % cols;
            const float y = gy + (row - scrollRow_) * sy;
            if (y < -ch || y > 720) continue;
            Rectf rc{gx + col * sx, y, cw, ch};
            const bool on = i == sel_;
            if (on) rc = rc.scaledAboutCenter(1.05f);
            drawCover(ctx, r, lib.games()[size_t(view_[size_t(i)])], rc, s, 1.0f, seenAt_);
            if (on) r.strokeRect(rc.inset(-3), 3, s.accent);
            TextStyle ls;
            ls.size = 16;
            ls.color = on ? s.text : s.sub;
            ls.weight = on ? FontWeight::Bold : FontWeight::Regular;
            r.drawText(fitTextShared(r, lib.games()[size_t(view_[size_t(i)])].displayName(), ls, cw).c_str(), rc.x, rc.bottom() + 8, ls);
        }
        r.popClip();
    }

private:
    DashboardId style_ = DashboardId::Count;
    std::vector<int> view_;
    unsigned ver_ = 0;
    int sel_ = 0, filter_ = 0;
    std::string query_;
    Smooth pos_{0, 12};
    float scrollRow_ = 0;
    Tween enter_;
    std::map<std::string, double> seenAt_;
};

// ------------------------------------------------------------------ Detalhes do jogo

class GameDetailScreen : public Screen {
public:
    explicit GameDetailScreen(std::string id) : id_(std::move(id)) {}
    const char* name() const override { return "GameDetail"; }
    bool wantsGlobalMenu() const override { return true; }
    void onEnter(Context& ctx) override {
        enter_.start(0.4f);
        style_ = ctx.app->libraryStyle();
        hl_.snap(0);
    }
    enum Act { Play = 0, Favorite, Cover, Remove, Settings, ActCount };

    void update(Context& ctx, const InputFrame& in, float dt) override {
        enter_.update(dt);
        hl_.update(dt);
        GameEntry* g = ctx.app->library().find(id_);
        if (!g) { ctx.app->pop(); return; }
        int before = sel_;
        if (in.press(Button::Down)) sel_ = (sel_ + 1) % ActCount;
        if (in.press(Button::Up)) sel_ = (sel_ + ActCount - 1) % ActCount;
        if (sel_ != before) { ctx.sfx(Sfx::Navigate); hl_.set(float(sel_)); }
        if (in.press(Button::X)) activate(ctx, Favorite);
        if (in.press(Button::A)) activate(ctx, sel_);
        if (in.press(Button::B)) { ctx.sfx(Sfx::Back); ctx.app->pop(); }
    }

    void activate(Context& ctx, int act) {
        GameEntry* g = ctx.app->library().find(id_);
        if (!g) return;
        switch (act) {
            case Play: {
                const std::string id = id_, nameS = g->displayName();
                ctx.sfx(Sfx::Open);
                ctx.app->push(createConfirmDialog("Jogar " + nameS + "?",
                                                  "O Neutro X salva a biblioteca, libera áudio e vídeo e inicia o jogo. Nada do sistema é alterado.",
                                                  [id](Context& c) {
                                                      std::string why;
                                                      if (!c.app->launchGame(id, &why)) {
                                                          c.sfx(Sfx::Error);
                                                          c.app->toast("Não foi possível iniciar: " + why);
                                                      }
                                                  }));
                break;
            }
            case Favorite:
                ctx.app->library().toggleFavorite(id_);
                ctx.sfx(Sfx::Confirm, 0.8f);
                ctx.app->toast(ctx.app->library().find(id_)->favorite ? "Adicionado aos favoritos" : "Removido dos favoritos");
                break;
            case Cover:
                ctx.sfx(Sfx::Confirm);
                ctx.app->requestCover(id_, true, true);
                ctx.app->toast("Procurando capa no XboxUnity…");
                break;
            case Remove: {
                const std::string id = id_;
                ctx.sfx(Sfx::Open);
                ctx.app->push(createConfirmDialog("Remover da biblioteca?",
                                                  "Só a entrada sai da lista (a pasta vai para os ignorados). O jogo NÃO é apagado do disco.",
                                                  [id](Context& c) {
                                                      c.app->library().remove(id);
                                                      c.app->toast("Removido da biblioteca (o jogo continua no disco)");
                                                      c.app->pop();
                                                  }));
                break;
            }
            case Settings:
                ctx.sfx(Sfx::Open);
                ctx.app->openLibrarySettings();
                break;
        }
    }

    void render(Context& ctx, IRenderer& r) override {
        const LibStyle s = styleFor(style_, ctx.accent());
        drawStyleBackground(r, s, float(ctx.time));
        GameEntry* gp = ctx.app->library().find(id_);
        if (!gp) return;
        const GameEntry& g = *gp;
        const float e = ease::outCubic(enter_.progress());
        r.pushAlpha(e);
        r.pushOffset((1 - e) * 30, 0);
        Rectf cover{70, 120, 320, 448};
        drawCover(ctx, r, g, cover, s, 1.0f, seenAt_);
        TextStyle t;
        t.size = 38;
        t.weight = s.lightWeight ? FontWeight::Light : FontWeight::Bold;
        t.color = s.text;
        r.drawText(fitTextShared(r, g.displayName(), t, 820).c_str(), 430, 56, t);
        // Informacoes (todas lidas do cabecalho real ou da biblioteca)
        int dups = 0;
        for (const GameEntry& o : ctx.app->library().games())
            if (o.duplicateOf == g.id) ++dups;
        char ver[32];
        std::snprintf(ver, sizeof ver, "%u.%u.%u.%u", (g.version >> 28) & 0xF, (g.version >> 24) & 0xF, (g.version >> 8) & 0xFFFF,
                      g.version & 0xFF);
        static const char* const kCoverSrc[] = {"nenhuma", "XboxUnity", "importada", "miniatura do pacote"};
        const std::pair<std::string, std::string> rows[] = {
            {"Title ID", hex8(g.titleId)},
            {"Media ID", hex8(g.mediaId)},
            {"Versão", ver},
            {"Tipo", gameKindName(g.kind)},
            {"Disco", g.discCount > 1 ? std::to_string(g.discNumber) + " de " + std::to_string(g.discCount) : "—"},
            {"Caminho", localPathToConsole(g.path, ctx.app->storageRootsCached())},
            {"Origem", g.manual ? "adicionado manualmente" : "pasta " + localPathToConsole(g.folder, ctx.app->storageRootsCached())},
            {"Adicionado", dateText(g.added) + (g.lastPlayed ? "  ·  jogado " + dateText(g.lastPlayed) : "")},
            {"Estado", std::string(g.available ? "disponível" : "INDISPONÍVEL (arquivo não encontrado)") +
                           (dups ? "  ·  " + std::to_string(dups) + (dups == 1 ? " cópia duplicada" : " cópias duplicadas") : "")},
            {"Capa", kCoverSrc[std::max(0, std::min(3, g.coverSource))]},
        };
        TextStyle k;
        k.size = 17;
        k.color = s.sub;
        TextStyle v;
        v.size = 19;
        v.color = s.text;
        float y = 116;
        for (const auto& rw : rows) {
            r.drawText(rw.first.c_str(), 432, y, k);
            r.drawText(fitTextShared(r, rw.second, v, 640).c_str(), 560, y - 1, v);
            y += 33;
        }
        // Acoes
        static const char* const kActs[] = {"Jogar", "Favoritar", "Atualizar capa", "Remover da biblioteca", "Configurações"};
        static const Icon kIcons[] = {Icon::Controller, Icon::Star, Icon::Download, Icon::Plus, Icon::Gear};
        const float ax = 432, ay = 466, aw = 360, ah = 38;
        r.fillRoundRect(Rectf{ax - 8, ay + hl_.value * (ah + 4) - 3, aw, ah}, s.radius, s.accent);
        for (int i = 0; i < ActCount; ++i) {
            const bool on = i == sel_;
            TextStyle as;
            as.size = 21;
            as.weight = on ? FontWeight::Bold : FontWeight::Regular;
            as.color = on ? s.onAccent : s.text;
            std::string label = i == Favorite ? (g.favorite ? "Remover dos favoritos" : "Favoritar") : kActs[i];
            label = titleText(s, label);
            drawIcon(r, kIcons[i], ax + 14, ay + i * (ah + 4) + 15, 20, as.color, on ? s.accent : Color(0, 0, 0, 0));
            r.drawText(label.c_str(), ax + 38, ay + i * (ah + 4) + 4, as);
        }
        if (!ctx.platform->canLaunchTitles() || ((g.kind == GameKind::God || g.kind == GameKind::Xbla) && !ctx.app->library().options().launchPackages)) {
            TextStyle w;
            w.size = 15;
            w.color = Color(240, 170, 90);
            const char* why = !ctx.platform->canLaunchTitles() ? "Prévia do Windows: o botão Jogar não executa títulos do Xbox 360."
                                                               : "GOD/XBLA: iniciar está desativado (experimental). Ative em Configurações > Jogos.";
            drawWrapped(r, why, 820, 470, 420, w, 20, 4);
        }
        r.popOffset();
        r.fillRect(Rectf{0, 668, 1280, 52}, Color(0, 0, 0, s.dark ? 110 : 40));
        drawHints(r, {{Button::A, "Selecionar"}, {Button::X, "Favoritar"}, {Button::B, "Voltar"}}, 56, 694, 30,
                  s.dark ? Color(220, 224, 230) : Color(40, 46, 52));
        r.popAlpha();
    }

private:
    std::string id_;
    DashboardId style_ = DashboardId::Count;
    int sel_ = 0;
    Smooth hl_{0, 20};
    Tween enter_;
    std::map<std::string, double> seenAt_;
};

}  // namespace

std::unique_ptr<Screen> createLibraryScreen() { return std::unique_ptr<Screen>(new LibraryScreen()); }
std::unique_ptr<Screen> createGameDetailScreen(const std::string& id) { return std::unique_ptr<Screen>(new GameDetailScreen(id)); }

void App::openLibrary() { push(createLibraryScreen()); }
void App::openGameDetails(const std::string& id) {
    if (lib_.find(id)) push(createGameDetailScreen(id));
    else toast("Jogo não encontrado na biblioteca");
}
void App::openLibrarySettings() { push(createLibrarySettingsScreen()); }

}  // namespace nx
