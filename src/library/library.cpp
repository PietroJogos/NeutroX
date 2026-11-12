// Neutro X - biblioteca de jogos. Ver library.h.
#include "library/library.h"

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <map>
#include <sstream>
#include "core/fsutil.h"
#include "library/title_meta.h"

namespace nx {

namespace {

constexpr uint32_t kOwnTitleId = 0x4E455558;  // o proprio Neutro X
constexpr int kMaxDirs = 20000;
constexpr int kMaxTitles = 3000;

std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

std::string oneLine(std::string s) {
    for (char& c : s)
        if (c == '\n' || c == '\r') c = ' ';
    return s.size() > 400 ? s.substr(0, 400) : s;
}

// Minusculas e sem acentos comuns (UTF-8 de 2 bytes do Latin-1), para ordenar e pesquisar.
std::string foldKey(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (c == 0xC3 && i + 1 < s.size()) {
            unsigned char d = static_cast<unsigned char>(s[i + 1]);
            ++i;
            const char* map = "aaaaaaaceeeeiiiidnooooo/ouuuuyty";  // 0xA0..0xBF (minusculas)
            unsigned idx = (d >= 0x80 && d <= 0x9F) ? d - 0x80 : (d >= 0xA0 && d <= 0xBF) ? d - 0xA0 : 99;
            if (idx < 32) out.push_back(map[idx]);
            continue;
        }
        out.push_back(c >= 'A' && c <= 'Z' ? char(c - 'A' + 'a') : char(c));
    }
    return out;
}

bool parseBool(const std::string& v) { return v == "1" || v == "sim" || v == "true"; }

int toInt(const std::string& v, int def) {
    char* end = nullptr;
    long n = std::strtol(v.c_str(), &end, 10);
    return end && *end == 0 && !v.empty() ? int(n) : def;
}

int64_t toInt64(const std::string& v) {
    char* end = nullptr;
    long long n = std::strtoll(v.c_str(), &end, 10);
    return end && *end == 0 && !v.empty() ? int64_t(n) : 0;
}

std::string pathKey(const std::string& p) { return toLowerAscii(normalizePath(p)); }

// "Disc 1", "DISC2", "Disco 1" -> numero; 0 se nao for pasta de disco.
int discFolderNumber(const std::string& name) {
    std::string l = toLowerAscii(name);
    size_t p = l.rfind("disc") == 0 ? 4 : (l.rfind("disco") == 0 ? 5 : std::string::npos);
    if (p == std::string::npos) return 0;
    if (l.size() > p && l[p] == 'o') ++p;
    while (p < l.size() && (l[p] == ' ' || l[p] == '_' || l[p] == '-')) ++p;
    int n = 0;
    while (p < l.size() && l[p] >= '0' && l[p] <= '9') n = n * 10 + (l[p++] - '0');
    return p == l.size() ? n : 0;
}

}  // namespace

const char* gameKindName(GameKind k) {
    switch (k) {
        case GameKind::Xex: return "Jogo extraído (default.xex)";
        case GameKind::God: return "Games on Demand (GOD)";
        case GameKind::Xbla: return "Xbox Live Arcade (XBLA)";
        case GameKind::App: return "Aplicativo (.xex)";
    }
    return "?";
}

std::string stableGameId(uint32_t titleId, const std::string& path) {
    // FNV-1a do caminho normalizado (estavel entre execucoes).
    uint32_t h = 2166136261u;
    for (char c : pathKey(path)) { h ^= uint8_t(c); h *= 16777619u; }
    return hex8(titleId) + "-" + hex8(h);
}

bool matchesQuery(const std::string& name, const std::string& query) {
    const std::string q = foldKey(trim(query));
    if (q.empty()) return true;
    return foldKey(name).find(q) != std::string::npos;
}

// ------------------------------------------------------------ persistencia

bool Library::load(const std::string& path) {
    games_.clear();
    folders_.clear();
    opts_ = LibraryOptions();
    std::vector<uint8_t> bytes;
    if (!readFileBytes(path, bytes, 8u * 1024u * 1024u)) { dirty_ = false; return false; }
    std::istringstream in(std::string(bytes.begin(), bytes.end()));
    std::string line, section;
    GameEntry* g = nullptr;
    LibraryFolder* f = nullptr;
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;
        if (line[0] == '[') {
            section = line;
            g = nullptr;
            f = nullptr;
            if (section == "[jogo]") { games_.emplace_back(); g = &games_.back(); }
            else if (section == "[pasta]") { folders_.emplace_back(); f = &folders_.back(); }
            continue;
        }
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string k = trim(line.substr(0, eq)), v = trim(line.substr(eq + 1));
        if (section == "[opcoes]") {
            if (k == "busca_ao_iniciar") opts_.scanOnStart = parseBool(v);
            else if (k == "profundidade_padrao") opts_.defaultDepth = std::max(1, std::min(12, toInt(v, 4)));
            else if (k == "tipo_xex") opts_.typeXex = parseBool(v);
            else if (k == "tipo_god") opts_.typeGod = parseBool(v);
            else if (k == "tipo_xbla") opts_.typeXbla = parseBool(v);
            else if (k == "tipo_aplicativos") opts_.typeApps = parseBool(v);
            else if (k == "detectar_duplicatas") opts_.detectDuplicates = parseBool(v);
            else if (k == "capas_automaticas") opts_.autoCovers = parseBool(v);
            else if (k == "iniciar_pacotes") opts_.launchPackages = parseBool(v);
            else if (k == "visualizacao") opts_.view = toInt(v, 0) == 1 ? 1 : 0;
            else if (k == "ultima_busca") lastScan_ = toInt64(v);
            else if (k == "resumo_ultima_busca") lastScanSummary_ = v;
        } else if (section == "[ignorar]" && k == "item" && !v.empty()) {
            opts_.ignore.push_back(v);
        } else if (f) {
            if (k == "caminho") f->path = v;
            else if (k == "profundidade") f->maxDepth = std::max(1, std::min(12, toInt(v, 4)));
            else if (k == "ativa") f->enabled = parseBool(v);
        } else if (g) {
            if (k == "id") g->id = v;
            else if (k == "nome") g->name = v;
            else if (k == "nome_usuario") g->userName = v;
            else if (k == "title_id") parseHex8(v, g->titleId);
            else if (k == "media_id") parseHex8(v, g->mediaId);
            else if (k == "versao") parseHex8(v, g->version);
            else if (k == "disco") std::sscanf(v.c_str(), "%d/%d", &g->discNumber, &g->discCount);
            else if (k == "tipo") g->kind = GameKind(std::max(0, std::min(3, toInt(v, 0))));
            else if (k == "caminho") g->path = v;
            else if (k == "pasta") g->folder = v;
            else if (k == "manual") g->manual = parseBool(v);
            else if (k == "favorito") g->favorite = parseBool(v);
            else if (k == "disponivel") g->available = parseBool(v);
            else if (k == "adicionado") g->added = toInt64(v);
            else if (k == "jogado") g->lastPlayed = toInt64(v);
            else if (k == "visto") g->lastSeen = toInt64(v);
            else if (k == "capa") g->cover = v.find("..") == std::string::npos && v.find('/') == std::string::npos ? v : "";
            else if (k == "capa_origem") g->coverSource = std::max(0, std::min(3, toInt(v, 0)));
        }
    }
    // descarta entradas invalidas (sem caminho/Title ID) e pastas vazias
    games_.erase(std::remove_if(games_.begin(), games_.end(),
                                [](const GameEntry& e) { return e.path.empty() || e.titleId == 0; }),
                 games_.end());
    for (GameEntry& e : games_)
        if (e.id.empty()) e.id = stableGameId(e.titleId, e.path);
    folders_.erase(std::remove_if(folders_.begin(), folders_.end(), [](const LibraryFolder& x) { return x.path.empty(); }),
                   folders_.end());
    recomputeDuplicates();
    dirty_ = false;
    ++version_;
    return true;
}

bool Library::save(const std::string& path) {
    if (!dirty_) return true;
    std::ostringstream o;
    o << "# Neutro X - biblioteca de jogos (arquivo proprio do Neutro X).\n";
    o << "# Remover uma entrada aqui NAO apaga o jogo. Pode ser apagado para recomecar a biblioteca.\n";
    o << "versao=1\n\n[opcoes]\n";
    o << "busca_ao_iniciar=" << (opts_.scanOnStart ? 1 : 0) << "\n";
    o << "profundidade_padrao=" << opts_.defaultDepth << "\n";
    o << "tipo_xex=" << (opts_.typeXex ? 1 : 0) << "\n";
    o << "tipo_god=" << (opts_.typeGod ? 1 : 0) << "\n";
    o << "tipo_xbla=" << (opts_.typeXbla ? 1 : 0) << "\n";
    o << "tipo_aplicativos=" << (opts_.typeApps ? 1 : 0) << "\n";
    o << "detectar_duplicatas=" << (opts_.detectDuplicates ? 1 : 0) << "\n";
    o << "capas_automaticas=" << (opts_.autoCovers ? 1 : 0) << "\n";
    o << "iniciar_pacotes=" << (opts_.launchPackages ? 1 : 0) << "\n";
    o << "visualizacao=" << opts_.view << "\n";
    o << "ultima_busca=" << lastScan_ << "\n";
    o << "resumo_ultima_busca=" << oneLine(lastScanSummary_) << "\n";
    for (const LibraryFolder& f : folders_)
        o << "\n[pasta]\ncaminho=" << oneLine(f.path) << "\nprofundidade=" << f.maxDepth << "\nativa=" << (f.enabled ? 1 : 0)
          << "\n";
    if (!opts_.ignore.empty()) {
        o << "\n[ignorar]\n";
        for (const std::string& s : opts_.ignore) o << "item=" << oneLine(s) << "\n";
    }
    for (const GameEntry& g : games_) {
        o << "\n[jogo]\nid=" << g.id << "\nnome=" << oneLine(g.name) << "\n";
        if (!g.userName.empty()) o << "nome_usuario=" << oneLine(g.userName) << "\n";
        o << "title_id=" << hex8(g.titleId) << "\nmedia_id=" << hex8(g.mediaId) << "\nversao=" << hex8(g.version) << "\n";
        if (g.discCount) o << "disco=" << g.discNumber << "/" << g.discCount << "\n";
        o << "tipo=" << int(g.kind) << "\ncaminho=" << oneLine(g.path) << "\npasta=" << oneLine(g.folder) << "\n";
        o << "manual=" << (g.manual ? 1 : 0) << "\nfavorito=" << (g.favorite ? 1 : 0) << "\ndisponivel=" << (g.available ? 1 : 0)
          << "\n";
        o << "adicionado=" << g.added << "\njogado=" << g.lastPlayed << "\nvisto=" << g.lastSeen << "\n";
        if (!g.cover.empty()) o << "capa=" << g.cover << "\ncapa_origem=" << g.coverSource << "\n";
    }
    if (!writeFileAtomic(path, o.str())) return false;
    dirty_ = false;
    return true;
}

GameEntry* Library::find(const std::string& id) {
    for (GameEntry& g : games_)
        if (g.id == id) return &g;
    return nullptr;
}

// ------------------------------------------------------------ edicao

bool Library::addFolder(const std::string& p, std::string* why) {
    const std::string path = normalizePath(p);
    if (path.empty()) { if (why) *why = "caminho vazio"; return false; }
    if (!dirExists(path)) { if (why) *why = "a pasta não existe: " + path; return false; }
    for (const LibraryFolder& f : folders_)
        if (pathKey(f.path) == pathKey(path)) { if (why) *why = "a pasta já está na lista"; return false; }
    LibraryFolder f;
    f.path = path;
    f.maxDepth = opts_.defaultDepth;
    folders_.push_back(f);
    markDirty();
    return true;
}

void Library::removeFolder(size_t i) {
    if (i >= folders_.size()) return;
    const std::string key = pathKey(folders_[i].path);
    // As entradas que vinham so desta pasta saem da biblioteca (os jogos ficam no disco).
    games_.erase(std::remove_if(games_.begin(), games_.end(),
                                [&](const GameEntry& g) { return !g.manual && pathKey(g.folder) == key; }),
                 games_.end());
    folders_.erase(folders_.begin() + long(i));
    recomputeDuplicates();
    markDirty();
}

void Library::addIgnore(const std::string& item) {
    const std::string v = item.find('/') != std::string::npos || item.find(':') != std::string::npos ? normalizePath(item) : trim(item);
    if (v.empty()) return;
    for (const std::string& s : opts_.ignore)
        if (toLowerAscii(s) == toLowerAscii(v)) return;
    opts_.ignore.push_back(v);
    markDirty();
}

void Library::removeIgnore(size_t i) {
    if (i >= opts_.ignore.size()) return;
    opts_.ignore.erase(opts_.ignore.begin() + long(i));
    markDirty();
}

bool Library::isIgnored(const std::string& path) const {
    const std::string key = pathKey(path), name = toLowerAscii(baseName(path));
    for (const std::string& s : opts_.ignore) {
        const std::string l = toLowerAscii(s);
        if (l.find('/') != std::string::npos || l.find(':') != std::string::npos) {
            const std::string pk = pathKey(l);
            if (key == pk || (key.size() > pk.size() && key.compare(0, pk.size(), pk) == 0 && key[pk.size()] == '/')) return true;
        } else if (name == l) {
            return true;
        }
    }
    return false;
}

bool Library::probeFile(const std::string& file, const LibraryOptions& opts, GameEntry& e, std::string* why) {
    e = GameEntry();
    std::vector<uint8_t> head;
    if (!readFileRange(file, 0, 4, head) || head.size() < 4) {
        if (why) *why = "arquivo ilegível ou vazio";
        return false;
    }
    e.path = normalizePath(file);
    if (head[0] == 'X' && head[1] == 'E' && head[2] == 'X') {
        XexMeta m;
        if (!readXexMeta(file, m)) { if (why) *why = m.error; return false; }
        if (m.titleId == kOwnTitleId) { if (why) *why = "é o próprio Neutro X"; return false; }
        const bool isDefault = toLowerAscii(baseName(file)) == "default.xex";
        e.kind = isDefault ? GameKind::Xex : GameKind::App;
        if (e.kind == GameKind::Xex && !opts.typeXex) { if (why) *why = "tipo desativado (jogos extraídos)"; return false; }
        if (e.kind == GameKind::App && !opts.typeApps) { if (why) *why = "tipo desativado (aplicativos)"; return false; }
        e.titleId = m.titleId;
        e.mediaId = m.mediaId;
        e.version = m.version;
        e.discNumber = m.discNumber;
        e.discCount = m.discCount;
        // Nome: pasta do jogo (a pasta "Disc N" usa a de cima).
        std::string dir = parentPath(e.path), name = baseName(dir);
        if (int dn = discFolderNumber(name)) {
            name = baseName(parentPath(dir));
            if (e.discCount < 2) e.discNumber = dn;
        }
        if (e.kind == GameKind::App) {
            name = baseName(e.path);
            if (endsWithNoCase(name, ".xex")) name.resize(name.size() - 4);
        }
        e.name = name.empty() || name.find(':') != std::string::npos ? hex8(e.titleId) : name;
        if (e.discCount > 1) e.name += " (disco " + std::to_string(e.discNumber) + ")";
    } else if (looksLikeStfs(head.data(), head.size())) {
        StfsMeta m;
        if (!readStfsMeta(file, m)) { if (why) *why = m.error; return false; }
        if (m.contentType == kContentGamesOnDemand) e.kind = GameKind::God;
        else if (m.contentType == kContentArcadeTitle) e.kind = GameKind::Xbla;
        else {
            if (why) *why = "pacote de tipo 0x" + hex8(m.contentType) + " (não é jogo GOD/XBLA)";
            return false;
        }
        if (e.kind == GameKind::God && !opts.typeGod) { if (why) *why = "tipo desativado (GOD)"; return false; }
        if (e.kind == GameKind::Xbla && !opts.typeXbla) { if (why) *why = "tipo desativado (XBLA)"; return false; }
        e.titleId = m.titleId;
        e.mediaId = m.mediaId;
        e.version = m.version;
        e.name = !m.displayName.empty() ? m.displayName : (!m.titleName.empty() ? m.titleName : hex8(m.titleId));
        e.thumb = std::move(m.thumbnail);
    } else {
        if (why) *why = "não é XEX2 nem pacote STFS";
        return false;
    }
    e.id = stableGameId(e.titleId, e.path);
    return true;
}

bool Library::addManual(const std::string& file, std::string* why, std::string* idOut) {
    LibraryOptions all = opts_;
    all.typeXex = all.typeGod = all.typeXbla = all.typeApps = true;  // escolha explicita do usuario
    GameEntry e;
    if (!probeFile(file, all, e, why)) return false;
    for (GameEntry& g : games_)
        if (pathKey(g.path) == pathKey(e.path)) {
            if (why) *why = "este título já está na biblioteca";
            if (idOut) *idOut = g.id;
            return false;
        }
    e.manual = true;
    e.added = e.lastSeen = int64_t(std::time(nullptr));
    games_.push_back(e);
    if (idOut) *idOut = e.id;
    recomputeDuplicates();
    markDirty();
    return true;
}

bool Library::remove(const std::string& id) {
    for (size_t i = 0; i < games_.size(); ++i) {
        if (games_[i].id != id) continue;
        if (!games_[i].manual) {
            // Para a proxima busca nao trazer de volta: ignora a pasta do jogo (ou o pacote).
            addIgnore(games_[i].kind == GameKind::Xex ? parentPath(games_[i].path) : games_[i].path);
        }
        games_.erase(games_.begin() + long(i));
        recomputeDuplicates();
        markDirty();
        return true;
    }
    return false;
}

void Library::toggleFavorite(const std::string& id) {
    if (GameEntry* g = find(id)) {
        g->favorite = !g->favorite;
        markDirty();
    }
}

// ------------------------------------------------------------ busca

std::vector<GameEntry> Library::scan(const std::vector<LibraryFolder>& folders, const LibraryOptions& opts, ScanStats& st,
                                     const std::atomic<bool>* cancel, std::function<void(const std::string&)> progress) {
    std::vector<GameEntry> out;
    st = ScanStats();
    Library ign;  // so para isIgnored com as opcoes atuais
    ign.opts_ = opts;
    auto note = [&](const std::string& s) { if (st.notes.size() < 40) st.notes.push_back(s); };
    for (const LibraryFolder& f : folders) {
        if (!f.enabled) continue;
        if (!dirExists(f.path)) { note("pasta indisponível: " + f.path); continue; }
        const int maxDepth = std::max(1, std::min(12, f.maxDepth));
        std::function<void(const std::string&, int)> rec = [&](const std::string& dir, int depth) {
            if ((cancel && cancel->load()) || st.folders >= kMaxDirs || int(out.size()) >= kMaxTitles) return;
            if (ign.isIgnored(dir)) return;
            ++st.folders;
            if (progress) progress(dir);
            std::vector<DirEntry> list;
            std::string err;
            if (!listDir(dir, list, &err)) {
                note("não foi possível ler " + dir + (err.empty() ? "" : ": " + err));
                return;
            }
            bool hasDefault = false;
            for (const DirEntry& de : list) {
                if (de.isDir) continue;
                const std::string lname = toLowerAscii(de.name);
                const std::string full = joinPath(dir, de.name);
                const bool isXex = endsWithNoCase(lname, ".xex");
                const bool noExt = lname.find('.') == std::string::npos;
                if (isXex && lname == "default.xex") hasDefault = true;
                const bool wantXex = isXex && (lname == "default.xex" ? opts.typeXex : opts.typeApps);
                const bool wantPkg = noExt && de.size >= 0x971A && (opts.typeGod || opts.typeXbla);
                if (!wantXex && !wantPkg) continue;
                if (ign.isIgnored(full)) continue;
                ++st.candidates;
                GameEntry e;
                std::string why;
                if (probeFile(full, opts, e, &why)) {
                    e.folder = normalizePath(f.path);
                    out.push_back(std::move(e));
                    ++st.found;
                } else if (wantXex) {
                    ++st.rejected;
                    note(full + ": " + why);
                }
                // pacotes STFS que nao sao jogos (perfis, DLC, temas) sao normais: sem aviso
            }
            if (hasDefault || depth >= maxDepth) return;  // nao entra na pasta de um jogo extraido
            for (const DirEntry& de : list) {
                if (!de.isDir || endsWithNoCase(de.name, ".data")) continue;  // .data = partes de um GOD
                rec(joinPath(dir, de.name), depth + 1);
            }
        };
        rec(normalizePath(f.path), 0);
    }
    if (cancel && cancel->load()) note("busca cancelada");
    if (st.folders >= kMaxDirs) note("limite de pastas atingido (" + std::to_string(kMaxDirs) + ")");
    return out;
}

void Library::merge(const std::vector<GameEntry>& found, const ScanStats& st, int64_t now, int* addedOut) {
    std::map<std::string, size_t> byPath;
    for (size_t i = 0; i < games_.size(); ++i) byPath[pathKey(games_[i].path)] = i;
    std::vector<bool> seen(games_.size(), false);
    int added = 0;
    for (const GameEntry& f : found) {
        auto it = byPath.find(pathKey(f.path));
        if (it != byPath.end()) {
            GameEntry& g = games_[it->second];
            seen[it->second] = true;
            g.name = f.name;
            g.titleId = f.titleId;
            g.mediaId = f.mediaId;
            g.version = f.version;
            g.discNumber = f.discNumber;
            g.discCount = f.discCount;
            g.kind = f.kind;
            if (!g.manual) g.folder = f.folder;
            g.available = true;
            g.lastSeen = now;
            if (g.id.empty()) g.id = f.id;
        } else {
            GameEntry e = f;
            e.added = e.lastSeen = now;
            e.available = true;
            byPath[pathKey(e.path)] = games_.size();
            games_.push_back(e);
            seen.push_back(true);
            ++added;
        }
    }
    // Entradas de pastas monitoradas que nao apareceram: ficam, marcadas como indisponiveis.
    std::vector<std::string> scanned;
    for (const LibraryFolder& fo : folders_)
        if (fo.enabled) scanned.push_back(pathKey(fo.path));
    for (size_t i = 0; i < games_.size(); ++i) {
        GameEntry& g = games_[i];
        if (seen[i]) continue;
        if (g.manual) { g.available = fileExists(g.path); continue; }
        if (std::find(scanned.begin(), scanned.end(), pathKey(g.folder)) != scanned.end()) g.available = false;
        else g.available = fileExists(g.path);
    }
    recomputeDuplicates();
    lastScan_ = now;
    char b[160];
    std::snprintf(b, sizeof b, "%d pastas, %d títulos encontrados (%d novos), %d recusados", st.folders, st.found, added,
                  st.rejected);
    lastScanSummary_ = b;
    if (addedOut) *addedOut = added;
    markDirty();
}

void Library::refreshAvailability() {
    bool changed = false;
    for (GameEntry& g : games_) {
        bool a = fileExists(g.path);
        if (a != g.available) { g.available = a; changed = true; }
    }
    recomputeDuplicates();
    if (changed) markDirty();
}

void Library::recomputeDuplicates() {
    std::map<std::string, std::string> first;  // chave do titulo -> id da primeira copia
    std::vector<size_t> order(games_.size());
    for (size_t i = 0; i < order.size(); ++i) order[i] = i;
    std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) {
        if (games_[a].available != games_[b].available) return games_[a].available;
        return games_[a].added < games_[b].added;
    });
    for (size_t i : order) {
        GameEntry& g = games_[i];
        g.duplicateOf.clear();
        if (!opts_.detectDuplicates || g.kind == GameKind::App) continue;
        const std::string key = hex8(g.titleId) + hex8(g.mediaId) + std::to_string(g.discNumber) +
                                (g.kind == GameKind::Xbla ? "x" : "");
        auto it = first.find(key);
        if (it == first.end()) first[key] = g.id;
        else g.duplicateOf = it->second;
    }
}

std::vector<int> Library::view(Filter f, const std::string& query) const {
    std::vector<int> out;
    for (size_t i = 0; i < games_.size(); ++i) {
        const GameEntry& g = games_[i];
        if (!g.duplicateOf.empty()) continue;  // duplicatas nao aparecem (ficam nos detalhes da original)
        switch (f) {
            case Filter::All: break;
            case Filter::Favorites: if (!g.favorite) continue; break;
            case Filter::Recent: break;
            case Filter::Unavailable: if (g.available) continue; break;
            case Filter::Search: if (!matchesQuery(g.displayName(), query)) continue; break;
        }
        out.push_back(int(i));
    }
    if (f == Filter::Recent) {
        std::stable_sort(out.begin(), out.end(), [&](int a, int b) { return games_[size_t(a)].added > games_[size_t(b)].added; });
        if (out.size() > 24) out.resize(24);
    } else {
        std::stable_sort(out.begin(), out.end(), [&](int a, int b) {
            return foldKey(games_[size_t(a)].displayName()) < foldKey(games_[size_t(b)].displayName());
        });
    }
    return out;
}

// ------------------------------------------------------------ busca em segundo plano

LibraryScanner::~LibraryScanner() {
    cancel_ = true;
    if (th_.joinable()) th_.join();
}

bool LibraryScanner::start(const std::vector<LibraryFolder>& folders, const LibraryOptions& opts) {
    if (running_) return false;
    if (th_.joinable()) th_.join();
    running_ = true;
    done_ = false;
    cancel_ = false;
    {
        std::lock_guard<std::mutex> lk(m_);
        result_.clear();
        current_.clear();
        visited_ = 0;
    }
    th_ = std::thread([this, folders, opts]() {
        ScanStats st;
        std::vector<GameEntry> r = Library::scan(folders, opts, st, &cancel_, [this](const std::string& d) {
            std::lock_guard<std::mutex> lk(m_);
            current_ = d;
            ++visited_;
        });
        {
            std::lock_guard<std::mutex> lk(m_);
            result_ = std::move(r);
            stats_ = st;
        }
        done_ = true;
        running_ = false;
    });
    return true;
}

bool LibraryScanner::take(std::vector<GameEntry>& out, ScanStats& stats) {
    if (!done_) return false;
    if (th_.joinable()) th_.join();
    std::lock_guard<std::mutex> lk(m_);
    out = std::move(result_);
    stats = stats_;
    result_.clear();
    done_ = false;
    return true;
}

std::string LibraryScanner::progress() {
    std::lock_guard<std::mutex> lk(m_);
    return current_;
}

int LibraryScanner::foldersVisited() {
    std::lock_guard<std::mutex> lk(m_);
    return visited_;
}

}  // namespace nx
