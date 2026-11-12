// Neutro X - capas. Ver covers.h.
#include "library/covers.h"

#include <algorithm>
#include <cstdio>
#include "core/fsutil.h"
#include "core/log.h"
#include "library/title_meta.h"
#include "net/http.h"

namespace nx {

namespace {

// Valor de um campo "Chave":"valor" (ou numero/null) dentro de um objeto JSON simples.
std::string jsonField(const std::string& obj, const std::string& key) {
    const std::string k = "\"" + key + "\"";
    size_t p = obj.find(k);
    if (p == std::string::npos) return "";
    p = obj.find(':', p + k.size());
    if (p == std::string::npos) return "";
    ++p;
    while (p < obj.size() && (obj[p] == ' ' || obj[p] == '\t')) ++p;
    if (p < obj.size() && obj[p] == '"') {
        size_t e = obj.find('"', p + 1);
        return e == std::string::npos ? "" : obj.substr(p + 1, e - p - 1);
    }
    size_t e = obj.find_first_of(",}", p);
    return obj.substr(p, e == std::string::npos ? std::string::npos : e - p);
}

}  // namespace

std::string pickCoverId(const std::string& json, int* count) {
    // Percorre os objetos {...} do vetor "Covers".
    size_t p = json.find("\"Covers\"");
    if (count) *count = 0;
    if (p == std::string::npos) return "";
    std::string best;
    int bestScore = -1;
    while ((p = json.find('{', p)) != std::string::npos) {
        size_t e = json.find('}', p);
        if (e == std::string::npos) break;
        const std::string obj = json.substr(p, e - p + 1);
        const std::string id = jsonField(obj, "CoverID");
        bool digits = !id.empty() && id.size() < 10;
        for (char c : id)
            if (c < '0' || c > '9') digits = false;
        if (digits) {
            if (count) ++*count;
            const int official = jsonField(obj, "Official") == "1" ? 1 : 0;
            const int rating = std::atoi(jsonField(obj, "Rating").c_str());
            const int score = official * 100 + rating;
            if (score > bestScore) { bestScore = score; best = id; }
        }
        p = e + 1;
    }
    return best;
}

Image frontCover(const Image& full) {
    if (full.w <= 0 || full.h <= 0) return full;
    if (float(full.w) / float(full.h) < 0.95f) return full;  // ja e so a frente
    const int fw = std::min(full.w, int(full.h * 0.705f + 0.5f));
    Image out;
    out.w = fw;
    out.h = full.h;
    out.px.resize(size_t(fw) * full.h);
    for (int y = 0; y < full.h; ++y)
        std::copy(full.px.begin() + long(size_t(y) * full.w + size_t(full.w - fw)), full.px.begin() + long(size_t(y + 1) * full.w),
                  out.px.begin() + long(size_t(y) * fw));
    return out;
}

bool validateCoverBytes(const std::vector<uint8_t>& b, std::string* why, Image* decoded) {
    if (b.size() < 512) { if (why) *why = "arquivo pequeno demais para ser uma capa"; return false; }
    if (b.size() > 6u * 1024u * 1024u) { if (why) *why = "arquivo grande demais (máximo 6 MB)"; return false; }
    if (sniffImage(b.data(), b.size()) == ImageFormat::Unknown) {
        if (why) *why = "o conteúdo recebido não é uma imagem (PNG/JPEG)";
        return false;
    }
    Image img;
    std::string w;
    if (!decodeImage(b.data(), b.size(), img, &w)) { if (why) *why = "imagem inválida: " + w; return false; }
    if (img.w < 48 || img.h < 48) { if (why) *why = "imagem pequena demais"; return false; }
    if (decoded) *decoded = std::move(img);
    return true;
}

// ------------------------------------------------------------ CoverService

CoverService::~CoverService() { stop(); }

void CoverService::stop() {
    {
        std::lock_guard<std::mutex> lk(m_);
        stop_ = true;
        jobs_.clear();
    }
    cancel_ = true;
    cv_.notify_all();
    if (th_.joinable()) th_.join();
}

void CoverService::setDirs(const std::string& coverDir, const std::string& importDir) {
    coverDir_ = coverDir;
    importDir_ = importDir;
}

void CoverService::request(const std::string& gameId, uint32_t titleId, bool force, bool manual) {
    std::lock_guard<std::mutex> lk(m_);
    if (stop_ || coverDir_.empty()) return;
    for (const Job& j : jobs_)
        if (j.gameId == gameId) return;
    if (!force) {
        auto f = failures_.find(titleId);
        if (f != failures_.end() && f->second >= 1) return;  // ja falhou nesta sessao
    }
    jobs_.push_back(Job{gameId, titleId, force, manual});
    if (!th_.joinable()) th_ = std::thread([this]() { loop(); });
    cv_.notify_one();
}

bool CoverService::busy() {
    std::lock_guard<std::mutex> lk(m_);
    return working_ || !jobs_.empty();
}

int CoverService::pending() {
    std::lock_guard<std::mutex> lk(m_);
    return int(jobs_.size()) + (working_ ? 1 : 0);
}

std::vector<CoverResult> CoverService::takeResults() {
    std::lock_guard<std::mutex> lk(m_);
    std::vector<CoverResult> r;
    r.swap(results_);
    return r;
}

void CoverService::loop() {
    for (;;) {
        Job j;
        {
            std::unique_lock<std::mutex> lk(m_);
            cv_.wait(lk, [&]() { return stop_ || !jobs_.empty(); });
            if (stop_) return;
            j = jobs_.front();
            jobs_.pop_front();
            working_ = true;
        }
        CoverResult r = fetch(j);
        {
            std::lock_guard<std::mutex> lk(m_);
            if (!r.ok) failures_[j.titleId]++;
            results_.push_back(r);
            working_ = false;
        }
    }
}

CoverResult CoverService::fetch(const Job& j) {
    CoverResult r;
    r.gameId = j.gameId;
    r.titleId = j.titleId;
    r.manual = j.manual;
    const std::string tid = hex8(j.titleId);
    const std::string file = tid + ".jpg";
    const std::string dest = joinPath(coverDir_, file);
    if (!j.force && fileExists(dest)) {
        r.ok = true;
        r.file = file;
        r.source = 1;
        r.message = "capa já estava no cache";
        return r;
    }
    makeDirs(coverDir_);
    HttpOptions opt;
    opt.maxBytes = 256 * 1024;
    HttpResponse info;
    if (!httpGet(baseUrl_ + "/Resources/Lib/CoverInfo.php?titleid=" + tid, info, opt, &cancel_)) {
        r.message = "XboxUnity: " + info.error;
        return r;
    }
    if (info.status != 200) {
        r.message = "XboxUnity respondeu HTTP " + std::to_string(info.status);
        return r;
    }
    int count = 0;
    const std::string cid = pickCoverId(std::string(info.body.begin(), info.body.end()), &count);
    if (cid.empty()) {
        r.message = "nenhuma capa para o Title ID " + tid + " no XboxUnity";
        return r;
    }
    HttpResponse img;
    opt.maxBytes = 6u * 1024u * 1024u;
    opt.timeoutMs = 20000;
    // A grande primeiro (melhor na tela); se falhar, a pequena.
    if (!httpGet(baseUrl_ + "/Resources/Lib/Cover.php?size=large&cid=" + cid, img, opt, &cancel_) || img.status != 200) {
        HttpResponse small;
        if (!httpGet(baseUrl_ + "/Resources/Lib/Cover.php?size=small&cid=" + cid, small, opt, &cancel_) || small.status != 200) {
            r.message = "download da capa falhou: " + (small.error.empty() ? "HTTP " + std::to_string(small.status) : small.error);
            return r;
        }
        img = small;
    }
    std::string why;
    if (!validateCoverBytes(img.body, &why)) {
        r.message = "capa recusada: " + why;
        return r;
    }
    const std::string part = dest + ".part";
    std::remove(part.c_str());
    if (!writeFileAtomic(dest, img.body.data(), img.body.size())) {
        r.message = "não foi possível gravar a capa em " + dest;
        return r;
    }
    r.ok = true;
    r.file = file;
    r.source = 1;
    r.message = "capa baixada do XboxUnity (" + std::to_string(img.body.size() / 1024) + " KB, capa " + cid + " de " +
                std::to_string(count) + ")";
    return r;
}

std::vector<std::pair<uint32_t, std::string>> CoverService::importFolder(std::vector<std::string>* notes) {
    std::vector<std::pair<uint32_t, std::string>> out;
    std::vector<DirEntry> list;
    if (importDir_.empty() || !listDir(importDir_, list)) return out;
    makeDirs(coverDir_);
    for (const DirEntry& e : list) {
        if (e.isDir) continue;
        std::string stem = e.name, ext;
        size_t dot = stem.find_last_of('.');
        if (dot != std::string::npos) { ext = toLowerAscii(stem.substr(dot)); stem = stem.substr(0, dot); }
        uint32_t tid = 0;
        if ((ext != ".jpg" && ext != ".jpeg" && ext != ".png") || !parseHex8(stem, tid)) {
            if (notes) notes->push_back(e.name + ": nome deve ser o Title ID (ex.: 4D5307E6.jpg)");
            continue;
        }
        std::vector<uint8_t> bytes;
        std::string why;
        if (!readFileBytes(joinPath(importDir_, e.name), bytes, 6u * 1024u * 1024u) || !validateCoverBytes(bytes, &why)) {
            if (notes) notes->push_back(e.name + ": " + (why.empty() ? "não foi possível ler" : why));
            continue;
        }
        const std::string file = hex8(tid) + (ext == ".png" ? "_imp.png" : "_imp.jpg");
        if (writeFileAtomic(joinPath(coverDir_, file), bytes.data(), bytes.size())) out.push_back({tid, file});
    }
    return out;
}

// ------------------------------------------------------------ CoverTextures

CoverTextures::~CoverTextures() {
    {
        std::lock_guard<std::mutex> lk(m_);
        stop_ = true;
    }
    cv_.notify_all();
    if (th_.joinable()) th_.join();
}

TextureId CoverTextures::get(IRenderer& r, const std::string& file) {
    (void)r;
    if (file.empty()) return kInvalidTexture;
    auto it = tex_.find(file);
    if (it != tex_.end()) {
        lru_.remove(file);
        lru_.push_front(file);
        return it->second;
    }
    if (failed_.count(file)) return kInvalidTexture;
    std::lock_guard<std::mutex> lk(m_);
    if (std::find(want_.begin(), want_.end(), file) == want_.end()) {
        want_.push_back(file);
        if (!th_.joinable()) th_ = std::thread([this]() { loop(); });
        cv_.notify_one();
    }
    return kInvalidTexture;
}

void CoverTextures::loop() {
    for (;;) {
        std::string file;
        {
            std::unique_lock<std::mutex> lk(m_);
            cv_.wait(lk, [&]() { return stop_ || !want_.empty(); });
            if (stop_) return;
            file = want_.front();
            want_.pop_front();
        }
        std::vector<uint8_t> bytes;
        Image img;
        std::string why;
        if (readFileBytes(joinPath(dir_, file), bytes, 6u * 1024u * 1024u) && decodeImage(bytes.data(), bytes.size(), img, &why)) {
            img = coverFit(frontCover(img), kW, kH);
        } else {
            img = Image();  // falha: marcada abaixo
        }
        std::lock_guard<std::mutex> lk(m_);
        ready_.emplace_back(file, std::move(img));
    }
}

void CoverTextures::pump(IRenderer& r) {
    std::vector<std::pair<std::string, Image>> got;
    {
        std::lock_guard<std::mutex> lk(m_);
        got.swap(ready_);
    }
    for (auto& g : got) {
        if (g.second.px.empty()) {
            failed_[g.first] = true;
            continue;
        }
        auto old = tex_.find(g.first);
        if (old != tex_.end()) r.destroyTexture(old->second);
        tex_[g.first] = r.createTexture(g.second.w, g.second.h, g.second.px.data());
        lru_.remove(g.first);
        lru_.push_front(g.first);
    }
    while (lru_.size() > limit_) {
        const std::string victim = lru_.back();
        lru_.pop_back();
        auto it = tex_.find(victim);
        if (it != tex_.end()) {
            r.destroyTexture(it->second);
            tex_.erase(it);
        }
    }
}

void CoverTextures::invalidate(IRenderer& r, const std::string& file) {
    auto it = tex_.find(file);
    if (it != tex_.end()) {
        r.destroyTexture(it->second);
        tex_.erase(it);
    }
    lru_.remove(file);
    failed_.erase(file);
}

void CoverTextures::releaseAll(IRenderer& r) {
    for (auto& kv : tex_) r.destroyTexture(kv.second);
    tex_.clear();
    lru_.clear();
    failed_.clear();
    std::lock_guard<std::mutex> lk(m_);
    want_.clear();
    ready_.clear();
}

}  // namespace nx
