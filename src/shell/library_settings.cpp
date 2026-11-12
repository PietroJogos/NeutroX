// Neutro X - Configuracoes > Jogos > Gerenciar pastas, navegador de armazenamento
// (escolher pasta ou titulo) e teclado na tela (pesquisa).
#include <algorithm>
#include <string>
#include "core/anim.h"
#include "core/fsutil.h"
#include "shell/screens.h"
#include "shell/shell_common.h"
#include "system/launcher.h"

namespace nx {

std::string fitTextShared(IRenderer& r, const std::string& s, const TextStyle& st, float maxW);

namespace {

// Lista de linhas generica com rolagem (rotulo a esquerda, valor a direita).
struct RowDef {
    std::string label, value;
    bool header = false;  // titulo de grupo (nao selecionavel)
};

class RowListBase : public Screen {
public:
    void onEnter(Context& ctx) override {
        enter_.start(0.4f);
        backdrop_.ensure(ctx);
    }
    void onExit(Context& ctx) override { backdrop_.release(ctx); }

protected:
    virtual void buildRows(Context& ctx, std::vector<RowDef>& rows) = 0;
    virtual const char* title() const = 0;
    virtual std::string footer(Context&) { return ""; }

    // Navegacao comum; devolve a linha atual (ou -1).
    int navigate(Context& ctx, const InputFrame& in, float dt) {
        enter_.update(dt);
        hl_.update(dt);
        scroll_.update(dt);
        rows_.clear();
        buildRows(ctx, rows_);
        const int n = int(rows_.size());
        if (n == 0) return -1;
        if (sel_ >= n) sel_ = n - 1;
        if (rows_[size_t(sel_)].header) sel_ = nextSelectable(sel_, +1);
        int before = sel_;
        if (in.press(Button::Down)) sel_ = nextSelectable(sel_, +1);
        if (in.press(Button::Up)) sel_ = nextSelectable(sel_, -1);
        if (sel_ != before) ctx.sfx(Sfx::Navigate);
        hl_.set(float(sel_));
        const int visible = 11;
        float sc = scroll_.target;
        if (sel_ < sc) sc = float(sel_);
        if (sel_ > sc + visible - 1) sc = float(sel_ - visible + 1);
        scroll_.set(sc);
        return sel_;
    }
    int nextSelectable(int from, int dir) const {
        const int n = int(rows_.size());
        int i = from;
        for (int k = 0; k < n; ++k) {
            i = (i + dir + n) % n;
            if (!rows_[size_t(i)].header) return i;
        }
        return from;
    }
    void renderRows(Context& ctx, IRenderer& r, std::initializer_list<Hint> hints) {
        backdrop_.draw(ctx, r);
        const Color accent = ctx.accent();
        r.pushAlpha(ease::outCubic(enter_.progress()));
        TextStyle t;
        t.size = 40;
        t.weight = FontWeight::Bold;
        t.color = Color(255, 255, 255);
        r.drawText(title(), 70, 44, t);
        const float y0 = 116, rh = 46;
        r.pushClip(Rectf{0, y0 - 4, 1280, rh * 11 + 8});
        r.fillRoundRect(Rectf{56, y0 + (hl_.value - scroll_.value) * rh - 2, 1168, rh - 4}, 8, accent.withAlpha(200));
        for (size_t i = 0; i < rows_.size(); ++i) {
            const float y = y0 + (float(i) - scroll_.value) * rh;
            if (y < y0 - rh || y > y0 + rh * 11) continue;
            const RowDef& rw = rows_[i];
            TextStyle ls;
            ls.size = rw.header ? 17 : 22;
            ls.weight = rw.header || int(i) == sel_ ? FontWeight::Bold : FontWeight::Regular;
            ls.color = rw.header ? Color(150, 190, 230) : (int(i) == sel_ ? Color(255, 255, 255) : Color(200, 208, 218));
            if (rw.header) ls.tracking = 1.0f;
            r.drawText(fitTextShared(r, rw.label, ls, 700).c_str(), 76, y + (rw.header ? 14 : 9), ls);
            if (!rw.value.empty()) {
                TextStyle vs;
                vs.size = 20;
                vs.align = Align::Right;
                vs.color = int(i) == sel_ ? Color(255, 255, 255) : Color(160, 170, 182);
                r.drawText(fitTextShared(r, rw.value, vs, 420).c_str(), 1200, y + 10, vs);
            }
        }
        r.popClip();
        const std::string f = footer(ctx);
        if (!f.empty()) {
            TextStyle fs;
            fs.size = 15;
            fs.color = Color(140, 150, 162);
            r.drawText(fitTextShared(r, f, fs, 1140).c_str(), 76, 636, fs);
        }
        r.fillRect(Rectf{0, 668, 1280, 52}, Color(0, 0, 0, 90));
        drawHints(r, hints, 56, 694, 30, Color(205, 212, 220));
        r.popAlpha();
    }

    NeutroBackdrop backdrop_;
    std::vector<RowDef> rows_;
    int sel_ = 0;
    Smooth hl_{0, 20}, scroll_{0, 14};
    Tween enter_;
};

const char* yesNo(bool b) { return b ? "Sim" : "Não"; }

// ------------------------------------------------------------------ Jogos e pastas

class LibrarySettingsScreen : public RowListBase {
public:
    const char* name() const override { return "LibrarySettings"; }
    const char* title() const override { return "Jogos e pastas"; }

    enum Kind { Folder, AddFolder, Depth, ScanStart, TXex, TGod, TXbla, TApps, Dups, Ignored, Covers, Packages, AddManual, ScanNow, Aurora };
    struct Item { Kind kind; int index; };

    void buildRows(Context& ctx, std::vector<RowDef>& rows) override {
        Library& lib = ctx.app->library();
        const auto& roots = ctx.app->storageRootsCached();
        items_.clear();
        auto add = [&](Kind k, int idx, std::string label, std::string value) {
            rows.push_back({std::move(label), std::move(value), false});
            items_.push_back({k, idx});
        };
        auto header = [&](const char* t) {
            rows.push_back({t, "", true});
            items_.push_back({Folder, -1});
        };
        header("PASTAS MONITORADAS");
        for (size_t i = 0; i < lib.folders().size(); ++i) {
            const LibraryFolder& f = lib.folders()[i];
            add(Folder, int(i), localPathToConsole(f.path, roots),
                std::string(f.enabled ? "" : "desativada · ") + "profundidade " + std::to_string(f.maxDepth) +
                    (dirExists(f.path) ? "" : " · não encontrada"));
        }
        add(AddFolder, 0, "+ Adicionar pasta…", "navegar no armazenamento");
        header("BUSCA");
        add(Depth, 0, "Profundidade padrão (novas pastas)", std::to_string(lib.options().defaultDepth) + " níveis");
        add(ScanStart, 0, "Procurar automaticamente ao iniciar", yesNo(lib.options().scanOnStart));
        add(TXex, 0, "Jogos extraídos (default.xex)", yesNo(lib.options().typeXex));
        add(TGod, 0, "Games on Demand (GOD)", yesNo(lib.options().typeGod));
        add(TXbla, 0, "Xbox Live Arcade (XBLA)", yesNo(lib.options().typeXbla));
        add(TApps, 0, "Aplicativos (.xex com outro nome)", yesNo(lib.options().typeApps));
        add(Dups, 0, "Detectar e ocultar duplicatas", yesNo(lib.options().detectDuplicates));
        add(Ignored, 0, "Pastas ignoradas", std::to_string(lib.options().ignore.size()));
        add(ScanNow, 0, "Procurar agora", ctx.app->libraryScanning() ? "em andamento…" : "");
        add(AddManual, 0, "Adicionar jogo manualmente…", "escolher um título");
        header("CAPAS E INICIALIZAÇÃO");
        add(Covers, 0, "Baixar capas automaticamente (XboxUnity)", yesNo(lib.options().autoCovers));
        add(Packages, 0, "Iniciar GOD/XBLA (experimental)", yesNo(lib.options().launchPackages));
        {
            LaunchTarget a = ctx.app->exitTargetInfo(ExitTarget::Aurora);
            add(Aurora, 0, "Caminho do Aurora (Sair > Voltar ao Aurora)",
                !ctx.settings->auroraPath.empty() ? localPathToConsole(ctx.settings->auroraPath, roots)
                : a.found                         ? "automático: " + localPathToConsole(a.path, roots)
                                                  : "automático (não encontrado)");
        }
    }

    std::string footer(Context& ctx) override {
        Library& lib = ctx.app->library();
        if (ctx.app->libraryScanning()) return "Procurando… " + ctx.app->libraryScanProgress();
        return lib.lastScanSummary().empty() ? "Nenhuma busca feita ainda. Arquivo: biblioteca.ini (pasta do Neutro X)."
                                             : "Última busca: " + lib.lastScanSummary();
    }

    void update(Context& ctx, const InputFrame& in, float dt) override {
        const int row = navigate(ctx, in, dt);
        if (in.press(Button::B)) {
            ctx.sfx(Sfx::Back);
            ctx.app->saveLibrary(true);
            ctx.app->saveSettings();
            ctx.app->pop();
            return;
        }
        if (row < 0 || row >= int(items_.size())) return;
        const Item it = items_[size_t(row)];
        Library& lib = ctx.app->library();
        const int delta = (in.press(Button::Right) ? 1 : 0) - (in.press(Button::Left) ? 1 : 0);
        auto toggle = [&](bool& b) { b = !b; lib.markDirty(); ctx.sfx(Sfx::Tab); };
        if (delta != 0) {
            if (it.kind == Depth) {
                lib.options().defaultDepth = std::max(1, std::min(12, lib.options().defaultDepth + delta));
                lib.markDirty();
                ctx.sfx(Sfx::Tab);
            } else if (it.kind == Folder && it.index >= 0) {
                LibraryFolder& f = lib.folders()[size_t(it.index)];
                f.maxDepth = std::max(1, std::min(12, f.maxDepth + delta));
                lib.markDirty();
                ctx.sfx(Sfx::Tab);
            }
        }
        if (in.press(Button::X) && it.kind == Aurora && !ctx.settings->auroraPath.empty()) {
            ctx.settings->auroraPath.clear();
            ctx.app->saveSettings();
            ctx.app->toast("Caminho do Aurora: automático");
        }
        if (!in.press(Button::A)) return;
        switch (it.kind) {
            case Folder: if (it.index >= 0) folderOptions(ctx, it.index); break;
            case AddFolder:
                ctx.sfx(Sfx::Open);
                ctx.app->push(createFolderBrowser(BrowseMode::PickFolder, "Adicionar pasta de jogos", [](Context& c, const std::string& p) {
                    std::string why;
                    if (c.app->library().addFolder(p, &why)) {
                        c.app->toast("Pasta adicionada; procurando jogos…");
                        c.app->startLibraryScan(true);
                    } else {
                        c.sfx(Sfx::Error);
                        c.app->toast("Pasta não adicionada: " + why);
                    }
                }));
                break;
            case Depth: lib.options().defaultDepth = lib.options().defaultDepth % 12 + 1; lib.markDirty(); ctx.sfx(Sfx::Tab); break;
            case ScanStart: toggle(lib.options().scanOnStart); break;
            case TXex: toggle(lib.options().typeXex); break;
            case TGod: toggle(lib.options().typeGod); break;
            case TXbla: toggle(lib.options().typeXbla); break;
            case TApps: toggle(lib.options().typeApps); break;
            case Dups: toggle(lib.options().detectDuplicates); lib.recomputeDuplicates(); break;
            case Covers: toggle(lib.options().autoCovers); break;
            case Packages:
                if (!lib.options().launchPackages) {
                    ctx.app->push(createConfirmDialog("Ativar início de GOD/XBLA?",
                                                      "Experimental: o pacote é entregue ao carregador do sistema (XAM). Não foi testado no console; se falhar, o console volta ao dashboard.",
                                                      [](Context& c) {
                                                          c.app->library().options().launchPackages = true;
                                                          c.app->library().markDirty();
                                                      }));
                } else {
                    toggle(lib.options().launchPackages);
                }
                break;
            case Ignored: ignoredOptions(ctx); break;
            case ScanNow:
                if (ctx.app->startLibraryScan(true)) { ctx.sfx(Sfx::Confirm); ctx.app->toast("Procurando jogos…"); }
                else ctx.sfx(Sfx::Error);
                break;
            case AddManual:
                ctx.sfx(Sfx::Open);
                ctx.app->push(createFolderBrowser(BrowseMode::PickTitle, "Adicionar jogo manualmente", [](Context& c, const std::string& p) {
                    std::string why, id;
                    if (c.app->library().addManual(p, &why, &id)) c.app->toast("Adicionado: " + c.app->library().find(id)->displayName());
                    else { c.sfx(Sfx::Error); c.app->toast("Não adicionado: " + why); }
                }));
                break;
            case Aurora:
                ctx.sfx(Sfx::Open);
                ctx.app->push(createFolderBrowser(BrowseMode::PickXex, "Escolher o Aurora.xex", [](Context& c, const std::string& p) {
                    c.settings->auroraPath = p;
                    c.app->saveSettings();
                    c.app->toast("Caminho do Aurora salvo");
                }));
                break;
        }
    }

    void folderOptions(Context& ctx, int idx) {
        Library& lib = ctx.app->library();
        const LibraryFolder f = lib.folders()[size_t(idx)];
        std::vector<ChoiceOption> o = {
            {"Procurar agora", "busca em todas as pastas ativas", !ctx.app->libraryScanning()},
            {f.enabled ? "Desativar (não procurar nesta pasta)" : "Ativar", "", true},
            {"Remover pasta monitorada", "tira a pasta e as entradas dela; os jogos NÃO são apagados", true},
            {"Cancelar", "", true},
        };
        ctx.sfx(Sfx::Open);
        ctx.app->push(createChoiceDialog(localPathToConsole(f.path, ctx.app->storageRootsCached()),
                                         "Profundidade: " + std::to_string(f.maxDepth) + " níveis (←/→ na lista para alterar)", std::move(o),
                                         [idx](Context& c, int i) {
                                             Library& l = c.app->library();
                                             if (idx >= int(l.folders().size())) return;
                                             if (i == 0) c.app->startLibraryScan(true);
                                             else if (i == 1) { l.folders()[size_t(idx)].enabled = !l.folders()[size_t(idx)].enabled; l.markDirty(); }
                                             else if (i == 2) { l.removeFolder(size_t(idx)); c.app->toast("Pasta removida da lista"); }
                                         }));
    }

    void ignoredOptions(Context& ctx) {
        Library& lib = ctx.app->library();
        std::vector<ChoiceOption> o;
        for (const std::string& s : lib.options().ignore) o.push_back({"Parar de ignorar: " + localPathToConsole(s, ctx.app->storageRootsCached()), "", true});
        o.push_back({"+ Ignorar uma pasta…", "a busca não entra nela", true});
        o.push_back({"Cancelar", "", true});
        const int n = int(lib.options().ignore.size());
        ctx.sfx(Sfx::Open);
        ctx.app->push(createChoiceDialog("Pastas ignoradas", "Itens com '/' são caminhos; sem '/' valem para qualquer pasta com esse nome.",
                                         std::move(o), [n](Context& c, int i) {
                                             if (i < n) {
                                                 c.app->library().removeIgnore(size_t(i));
                                             } else if (i == n) {
                                                 c.app->push(createFolderBrowser(BrowseMode::PickFolder, "Escolher pasta a ignorar",
                                                                                 [](Context& c2, const std::string& p) {
                                                                                     c2.app->library().addIgnore(p);
                                                                                     c2.app->toast("Pasta ignorada");
                                                                                 }));
                                             }
                                         }));
    }

    void render(Context& ctx, IRenderer& r) override {
        renderRows(ctx, r, {{Button::A, "Selecionar"}, {Button::Left, "/"}, {Button::Right, "Ajustar"}, {Button::B, "Salvar e voltar"}});
    }

private:
    std::vector<Item> items_;
};

// ------------------------------------------------------------------ navegador

class FolderBrowserScreen : public RowListBase {
public:
    FolderBrowserScreen(BrowseMode mode, std::string title, std::function<void(Context&, const std::string&)> fn)
        : mode_(mode), title_(std::move(title)), fn_(std::move(fn)) {}
    const char* name() const override { return "FolderBrowser"; }
    const char* title() const override { return title_.c_str(); }

    void onEnter(Context& ctx) override {
        RowListBase::onEnter(ctx);
        load(ctx);
    }

    void load(Context& ctx) {
        entries_.clear();
        error_.clear();
        if (cwd_.empty()) return;
        std::vector<DirEntry> list;
        if (!listDir(cwd_, list, &error_)) {
            if (error_.empty()) error_ = "não foi possível ler a pasta";
            return;
        }
        for (const DirEntry& e : list) {
            if (entries_.size() >= 3000) break;
            if (e.isDir) { entries_.push_back(e); continue; }
            const std::string l = toLowerAscii(e.name);
            const bool xex = endsWithNoCase(l, ".xex");
            const bool pkg = l.find('.') == std::string::npos && e.size >= 0x971A;
            if ((mode_ == BrowseMode::PickXex && xex) || (mode_ == BrowseMode::PickTitle && (xex || pkg))) entries_.push_back(e);
        }
        (void)ctx;
    }

    void buildRows(Context& ctx, std::vector<RowDef>& rows) override {
        if (cwd_.empty()) {
            for (const StorageRoot& r : ctx.app->storageRootsCached()) rows.push_back({r.label, r.path, false});
            if (rows.empty()) rows.push_back({"Nenhuma unidade de armazenamento encontrada", "", true});
            return;
        }
        if (mode_ == BrowseMode::PickFolder) rows.push_back({"[ Usar esta pasta ]", "X", false});
        for (const DirEntry& e : entries_)
            rows.push_back({(e.isDir ? "> " : "") + e.name, e.isDir ? "" : std::to_string(e.size / 1024) + " KB", false});
        if (!error_.empty()) rows.push_back({"Erro: " + error_, "", true});
        else if (entries_.empty()) rows.push_back({mode_ == BrowseMode::PickFolder ? "(sem subpastas)" : "(nada para escolher aqui)", "", true});
    }

    std::string footer(Context& ctx) override {
        return cwd_.empty() ? "Escolha uma unidade" : "Pasta atual: " + localPathToConsole(cwd_, ctx.app->storageRootsCached());
    }

    void update(Context& ctx, const InputFrame& in, float dt) override {
        const int row = navigate(ctx, in, dt);
        if (in.press(Button::B)) {
            ctx.sfx(Sfx::Back);
            if (cwd_.empty()) { ctx.app->pop(); return; }
            // sobe um nivel (da raiz da unidade volta para a lista de unidades)
            bool atRoot = false;
            for (const StorageRoot& r : ctx.app->storageRootsCached())
                if (toLowerAscii(normalizePath(r.path)) == toLowerAscii(normalizePath(cwd_))) atRoot = true;
            const std::string prevName = baseName(cwd_);
            cwd_ = atRoot ? "" : parentPath(cwd_);
            load(ctx);
            sel_ = 0;
            rows_.clear();
            buildRows(ctx, rows_);
            for (size_t i = 0; i < rows_.size(); ++i)
                if (rows_[i].label == "> " + prevName) sel_ = int(i);
            hl_.snap(float(sel_));
            scroll_.snap(std::max(0.0f, float(sel_) - 5));
            return;
        }
        if (in.press(Button::X) && mode_ == BrowseMode::PickFolder && !cwd_.empty()) { choose(ctx, cwd_); return; }
        if (!in.press(Button::A) || row < 0 || rows_[size_t(row)].header) return;
        if (cwd_.empty()) {
            const auto& roots = ctx.app->storageRootsCached();
            if (row < int(roots.size())) enter(ctx, roots[size_t(row)].path);
            return;
        }
        int idx = row - (mode_ == BrowseMode::PickFolder ? 1 : 0);
        if (idx < 0) { choose(ctx, cwd_); return; }
        if (idx >= int(entries_.size())) return;
        const DirEntry& e = entries_[size_t(idx)];
        const std::string full = joinPath(cwd_, e.name);
        if (e.isDir) enter(ctx, full);
        else choose(ctx, full);
    }

    void enter(Context& ctx, const std::string& path) {
        ctx.sfx(Sfx::Confirm, 0.7f);
        cwd_ = normalizePath(path);
        load(ctx);
        sel_ = 0;
        hl_.snap(0);
        scroll_.snap(0);
    }

    void choose(Context& ctx, const std::string& path) {
        ctx.sfx(Sfx::Confirm);
        auto fn = fn_;
        ctx.app->pop();
        if (fn) fn(ctx, normalizePath(path));
    }

    void render(Context& ctx, IRenderer& r) override {
        if (mode_ == BrowseMode::PickFolder)
            renderRows(ctx, r, {{Button::A, "Abrir"}, {Button::X, "Usar esta pasta"}, {Button::B, "Voltar"}});
        else
            renderRows(ctx, r, {{Button::A, "Abrir/Escolher"}, {Button::B, "Voltar"}});
    }

private:
    BrowseMode mode_;
    std::string title_;
    std::function<void(Context&, const std::string&)> fn_;
    std::string cwd_;  // "" = lista de unidades
    std::vector<DirEntry> entries_;
    std::string error_;
};

// ------------------------------------------------------------------ teclado

class KeyboardScreen : public Screen {
public:
    KeyboardScreen(std::string title, std::string text, std::function<void(Context&, const std::string&)> fn)
        : title_(std::move(title)), text_(std::move(text)), fn_(std::move(fn)) {}
    const char* name() const override { return "Keyboard"; }
    bool isOverlay() const override { return true; }
    void onEnter(Context&) override { open_.start(0.2f); }

    static constexpr int kCols = 10, kRows = 5;
    const char* key(int r, int c) const {
        static const char* const rows[4][10] = {
            {"A", "B", "C", "D", "E", "F", "G", "H", "I", "J"}, {"K", "L", "M", "N", "O", "P", "Q", "R", "S", "T"},
            {"U", "V", "W", "X", "Y", "Z", "1", "2", "3", "4"}, {"5", "6", "7", "8", "9", "0", "-", ":", "'", "&"}};
        if (r < 4) return rows[r][c];
        static const char* const last[10] = {"ESPAÇO", "ESPAÇO", "ESPAÇO", "APAGAR", "APAGAR", "LIMPAR", "LIMPAR", "OK", "OK", "OK"};
        return last[c];
    }
    void update(Context& ctx, const InputFrame& in, float dt) override {
        open_.update(dt);
        int br = r_, bc = c_;
        if (in.press(Button::Right)) c_ = (c_ + 1) % kCols;
        if (in.press(Button::Left)) c_ = (c_ + kCols - 1) % kCols;
        if (in.press(Button::Down)) r_ = (r_ + 1) % kRows;
        if (in.press(Button::Up)) r_ = (r_ + kRows - 1) % kRows;
        if (br != r_ || bc != c_) ctx.sfx(Sfx::Navigate, 0.7f);
        auto backspace = [&]() {
            if (text_.empty()) return;
            size_t n = text_.size() - 1;
            while (n > 0 && (static_cast<unsigned char>(text_[n]) & 0xC0) == 0x80) --n;
            text_.erase(n);
        };
        if (in.press(Button::X)) { backspace(); ctx.sfx(Sfx::Back, 0.6f); }
        if (in.press(Button::Y) && text_.size() < 40) { text_ += ' '; ctx.sfx(Sfx::Confirm, 0.6f); }
        if (in.press(Button::A)) {
            const std::string k = key(r_, c_);
            if (k == "ESPAÇO") { if (text_.size() < 40) text_ += ' '; }
            else if (k == "APAGAR") backspace();
            else if (k == "LIMPAR") text_.clear();
            else if (k == "OK") { finish(ctx); return; }
            else if (text_.size() < 40) text_ += k;
            ctx.sfx(Sfx::Confirm, 0.6f);
        }
        if (in.press(Button::Start)) { finish(ctx); return; }
        if (in.press(Button::B)) { ctx.sfx(Sfx::Back); ctx.app->pop(); }
    }
    void finish(Context& ctx) {
        ctx.sfx(Sfx::Confirm);
        auto fn = fn_;
        std::string t = text_;
        while (!t.empty() && t.back() == ' ') t.pop_back();
        ctx.app->pop();
        if (fn) fn(ctx, t);
    }
    void render(Context& ctx, IRenderer& r) override {
        const float p = open_.progress();
        r.fillRect(Rectf{0, 0, 1280, 720}, Color(0, 0, 0, uint8_t(170 * p)));
        Rectf box{220, 120, 840, 470};
        r.pushAlpha(clampf(p * 2, 0, 1));
        r.fillRoundRect(box, 16, Color(24, 30, 38));
        r.strokeRoundRect(box, 16, 2, ctx.accent());
        TextStyle t;
        t.size = 28;
        t.weight = FontWeight::Bold;
        t.color = Color(255, 255, 255);
        r.drawText(title_.c_str(), box.x + 30, box.y + 22, t);
        Rectf field{box.x + 30, box.y + 70, box.w - 60, 52};
        r.fillRoundRect(field, 8, Color(10, 14, 18));
        TextStyle ft;
        ft.size = 26;
        ft.color = Color(255, 255, 255);
        std::string shown = text_ + (std::fmod(ctx.time, 1.0) < 0.5 ? "|" : "");
        r.drawText(shown.c_str(), field.x + 14, field.y + 12, ft);
        const float kx = box.x + 30, ky = box.y + 146, kw = 78, kh = 52;
        for (int rr = 0; rr < kRows; ++rr)
            for (int cc = 0; cc < kCols; ++cc) {
                // a ultima linha tem teclas largas (mesma tecla em varias colunas)
                if (rr == 4 && cc > 0 && std::string(key(rr, cc)) == key(rr, cc - 1)) continue;
                int span = 1;
                while (rr == 4 && cc + span < kCols && std::string(key(rr, cc + span)) == key(rr, cc)) ++span;
                Rectf k{kx + cc * kw, ky + rr * kh, kw * span - 6, kh - 6};
                const bool on = rr == r_ && (rr < 4 ? cc == c_ : (c_ >= cc && c_ < cc + span));
                r.fillRoundRect(k, 6, on ? ctx.accent() : Color(44, 52, 62));
                TextStyle ks;
                ks.size = rr == 4 ? 18 : 24;
                ks.align = Align::Center;
                ks.weight = on ? FontWeight::Bold : FontWeight::Regular;
                ks.color = Color(255, 255, 255);
                r.drawText(key(rr, cc), k.cx(), textTopForCenter(k.cy(), ks.size), ks);
            }
        drawHints(r, {{Button::A, "Tecla"}, {Button::X, "Apagar"}, {Button::Y, "Espaço"}, {Button::Start, "OK"}, {Button::B, "Cancelar"}},
                  box.x + 30, box.bottom() - 26, 26, Color(205, 212, 220));
        r.popAlpha();
    }

private:
    std::string title_, text_;
    std::function<void(Context&, const std::string&)> fn_;
    Tween open_;
    int r_ = 0, c_ = 0;
};

}  // namespace

std::unique_ptr<Screen> createLibrarySettingsScreen() { return std::unique_ptr<Screen>(new LibrarySettingsScreen()); }
std::unique_ptr<Screen> createFolderBrowser(BrowseMode mode, const std::string& title,
                                            std::function<void(Context&, const std::string&)> onChoose) {
    return std::unique_ptr<Screen>(new FolderBrowserScreen(mode, title, std::move(onChoose)));
}
std::unique_ptr<Screen> createKeyboardScreen(const std::string& title, const std::string& initial,
                                             std::function<void(Context&, const std::string&)> onDone) {
    return std::unique_ptr<Screen>(new KeyboardScreen(title, initial, std::move(onDone)));
}

}  // namespace nx
