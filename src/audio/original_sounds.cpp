// Neutro X - carregamento de sons ORIGINAIS a partir de pasta local. Ver audio.h.
//
// Somente le arquivos que o proprio usuario colocou na pasta de dados.
// Nao descriptografa, nao descomprime e nao extrai nada de executaveis.
#include <algorithm>
#include <cctype>
#include <cmath>
#include <exception>
#include <filesystem>
#include <cstdio>
#include <cstring>
#include "audio/audio.h"

namespace nx {

int OrigReport::loaded(SoundTheme t) const {
    int n = 0, ti = int(t);
    if (ti < 0 || ti >= kSoundThemeCount) return 0;
    for (int s = 0; s < kSfxCount; ++s) n += e[ti][s].status == OrigStatus::Loaded;
    return n;
}

int OrigReport::totalLoaded() const {
    int n = 0;
    for (int t = 1; t < kSoundThemeCount; ++t) n += loaded(SoundTheme(t));
    return n;
}

namespace {

uint16_t le16(const uint8_t* p) { return uint16_t(p[0] | (p[1] << 8)); }
uint32_t le32(const uint8_t* p) { return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24); }

bool readFile(const std::string& path, std::vector<uint8_t>& out, size_t maxBytes) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    out.clear();
    uint8_t buf[8192];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) {
        out.insert(out.end(), buf, buf + n);
        if (out.size() > maxBytes) break;
    }
    std::fclose(f);
    return true;
}

std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

}  // namespace

bool decodeWav(const std::vector<uint8_t>& d, std::vector<int16_t>& out, std::string& reason) {
    out.clear();
    if (d.size() < 12 || std::memcmp(d.data(), "RIFF", 4) != 0 || std::memcmp(d.data() + 8, "WAVE", 4) != 0) {
        reason = "não é um arquivo WAV (cabeçalho RIFF/WAVE ausente)";
        return false;
    }
    uint16_t fmt = 0, ch = 0, bits = 0;
    uint32_t rate = 0;
    const uint8_t* pcm = nullptr;
    size_t pcmBytes = 0;
    size_t p = 12;
    while (p + 8 <= d.size()) {
        uint32_t sz = le32(&d[p + 4]);
        const uint8_t* body = &d[p + 8];
        size_t avail = d.size() - (p + 8);
        if (std::memcmp(&d[p], "fmt ", 4) == 0 && sz >= 16 && avail >= 16) {
            fmt = le16(body);
            ch = le16(body + 2);
            rate = le32(body + 4);
            bits = le16(body + 14);
            if (fmt == 0xFFFE && sz >= 40 && avail >= 40) fmt = le16(body + 24);  // WAVE_FORMAT_EXTENSIBLE
        } else if (std::memcmp(&d[p], "data", 4) == 0) {
            pcm = body;
            pcmBytes = std::min<size_t>(sz, avail);
        }
        p += 8 + sz + (sz & 1);
    }
    if (fmt == 0x0165 || fmt == 0x0166) {
        reason = "formato XMA/XMA2 (codec proprietário do Xbox 360) não suportado; converta para WAV PCM";
        return false;
    }
    if (fmt == 0x0002 || fmt == 0x0011) {
        reason = "WAV ADPCM não suportado; converta para PCM 16 bits";
        return false;
    }
    if (!(fmt == 1 && (bits == 8 || bits == 16 || bits == 24)) && !(fmt == 3 && bits == 32)) {
        char b[96];
        std::snprintf(b, sizeof(b), "formato WAV não suportado (código 0x%04X, %u bits)", fmt, bits);
        reason = b;
        return false;
    }
    if (ch < 1 || ch > 8 || rate < 4000 || rate > 192000) {
        reason = "parâmetros de canal/taxa inválidos";
        return false;
    }
    if (!pcm || pcmBytes == 0) {
        reason = "bloco de dados de áudio ausente ou vazio";
        return false;
    }
    const size_t frameBytes = size_t(ch) * (bits / 8);
    const size_t frames = pcmBytes / frameBytes;
    if (double(frames) / rate > 12.0) {
        reason = "áudio longo demais para efeito sonoro (máximo 12 s)";
        return false;
    }
    // Decodifica para mono float.
    std::vector<float> mono(frames);
    for (size_t i = 0; i < frames; ++i) {
        const uint8_t* f = pcm + i * frameBytes;
        float acc = 0;
        for (int c = 0; c < ch; ++c) {
            const uint8_t* s = f + c * (bits / 8);
            float v;
            if (fmt == 3) { float x; std::memcpy(&x, s, 4); v = x; }
            else if (bits == 8) v = (s[0] - 128) / 128.0f;
            else if (bits == 16) v = int16_t(le16(s)) / 32768.0f;
            else { int32_t x = int32_t((uint32_t(s[0]) << 8) | (uint32_t(s[1]) << 16) | (uint32_t(s[2]) << 24)) >> 8; v = x / 8388608.0f; }
            acc += v;
        }
        mono[i] = acc / ch;
    }
    // Reamostragem linear para 44,1 kHz.
    size_t outN = size_t(double(frames) * kAudioRate / rate);
    out.resize(outN);
    for (size_t i = 0; i < outN; ++i) {
        double src = double(i) * rate / kAudioRate;
        size_t a = size_t(src);
        float fr = float(src - double(a));
        float v0 = mono[std::min(a, frames - 1)], v1 = mono[std::min(a + 1, frames - 1)];
        float v = std::max(-1.0f, std::min(1.0f, v0 + (v1 - v0) * fr));
        out[i] = int16_t(std::lround(v * 32767.0f));
    }
    return true;
}

void Mixer::setSource(SoundSource s) {
    std::lock_guard<std::mutex> lock(mtx_);
    source_ = s;
}

SoundSource Mixer::source() {
    std::lock_guard<std::mutex> lock(mtx_);
    return source_;
}

OrigReport Mixer::originalsReport() {
    std::lock_guard<std::mutex> lock(mtx_);
    return report_;
}

void Mixer::loadOriginals(const std::string& baseDir) {
    OrigReport rep;
    rep.baseDir = baseDir;
    std::vector<std::vector<int16_t>> loaded(bank_.size());
    for (int t = 1; t < kSoundThemeCount; ++t) {  // tema 0 (Neutro X) e sempre proprio
        const SoundThemeInfo& ti = soundThemeInfo(SoundTheme(t));
        std::string dir = baseDir + ti.folder + "/";
        // mapeamento opcional: efeito=arquivo
        std::string names[kSfxCount];
        for (int s = 0; s < kSfxCount; ++s) names[s] = std::string(sfxFileName(Sfx(s))) + ".wav";
        std::vector<uint8_t> map;
        if (readFile(dir + "mapeamento.ini", map, 64 * 1024)) {
            std::string text(map.begin(), map.end());
            size_t pos = 0;
            while (pos < text.size()) {
                size_t nl = text.find('\n', pos);
                std::string line = trim(text.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos));
                pos = nl == std::string::npos ? text.size() : nl + 1;
                if (line.empty() || line[0] == '#') continue;
                size_t eq = line.find('=');
                if (eq == std::string::npos) continue;
                std::string k = trim(line.substr(0, eq)), v = trim(line.substr(eq + 1));
                if (v.empty() || v.find("..") != std::string::npos) continue;  // so arquivos dentro da pasta
                for (int s = 0; s < kSfxCount; ++s)
                    if (k == sfxFileName(Sfx(s))) names[s] = v;
            }
        }
        // ausentes.ini (opcional, gerado pelo extrator): efeito=causa da ausencia
        std::string causes[kSfxCount];
        std::vector<uint8_t> aus;
        if (readFile(dir + "ausentes.ini", aus, 64 * 1024)) {
            std::string text(aus.begin(), aus.end());
            size_t pos = 0;
            while (pos < text.size()) {
                size_t nl = text.find('\n', pos);
                std::string line = trim(text.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos));
                pos = nl == std::string::npos ? text.size() : nl + 1;
                if (line.empty() || line[0] == '#') continue;
                size_t eq = line.find('=');
                if (eq == std::string::npos) continue;
                std::string k = trim(line.substr(0, eq));
                for (int s = 0; s < kSfxCount; ++s)
                    if (k == sfxFileName(Sfx(s))) causes[s] = trim(line.substr(eq + 1));
            }
        }
        // conta todos os .wav da pasta (originais instalados, com ou sem funcao)
        try {
            std::filesystem::path p = std::filesystem::u8path(dir);
            if (std::filesystem::is_directory(p))
                for (const auto& de : std::filesystem::directory_iterator(p)) {
                    std::string ext = de.path().extension().u8string();
                    for (char& ch : ext) ch = char(std::tolower(static_cast<unsigned char>(ch)));
                    if (de.is_regular_file() && ext == ".wav") rep.filesInFolder[t]++;
                }
        } catch (const std::exception&) {
        }
        for (int s = 0; s < kSfxCount; ++s) {
            OrigEntry& en = rep.e[t][s];
            en.file = dir + names[s];
            std::vector<uint8_t> bytes;
            if (!readFile(en.file, bytes, 16u * 1024u * 1024u)) {
                en.status = OrigStatus::Missing;
                en.reason = causes[s].empty() ? "arquivo não encontrado" : causes[s];
                continue;
            }
            if (bytes.size() > 16u * 1024u * 1024u) {
                en.status = OrigStatus::Invalid;
                en.reason = "arquivo grande demais (máximo 16 MB)";
                continue;
            }
            std::vector<int16_t> pcm;
            std::string why;
            if (!decodeWav(bytes, pcm, why)) {
                en.status = why.find("XMA") != std::string::npos || why.find("suportado") != std::string::npos
                                ? OrigStatus::Unsupported : OrigStatus::Invalid;
                en.reason = why;
                continue;
            }
            en.status = OrigStatus::Loaded;
            en.seconds = float(pcm.size()) / kAudioRate;
            loaded[size_t(t * kSfxCount + s)] = std::move(pcm);
        }
    }
    for (int s = 0; s < kSfxCount; ++s) {
        rep.e[0][s].status = OrigStatus::Missing;
        rep.e[0][s].reason = "tema próprio do Neutro X (não se aplica)";
    }
    std::lock_guard<std::mutex> lock(mtx_);
    voices_.clear();  // nenhuma voz pode apontar para o banco antigo
    orig_.swap(loaded);
    report_ = rep;
}

}  // namespace nx
