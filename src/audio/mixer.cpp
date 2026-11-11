// Neutro X - mixer e registro dos temas sonoros. Ver audio.h.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include "audio/audio.h"
#include "audio/synth.h"

namespace nx {

namespace {
const SoundThemeInfo kThemes[kSoundThemeCount] = {
    {SoundTheme::NeutroX, "neutrox", "Neutro X", "senoides limpas e acorde maior brilhante"},
    {SoundTheme::Blades6717, "blades_6717", "Blades 6717", "vidro e metal: toques cristalinos e laminas cortando o ar"},
    {SoundTheme::Nxe9199, "nxe_9199", "NXE 9199", "leve e aerado: bolhas subindo e sopros suaves"},
    {SoundTheme::Kinect13604, "kinect_13604", "Kinect 13604", "brincalhao e de madeira: notas de marimba"},
    {SoundTheme::MetroV1_15574, "metro_v1_15574", "Metro V1 15574", "minimalista e nitido: cliques secos e tons agudos"},
    {SoundTheme::MetroV2_17559, "metro_v2_17559", "Metro V2 17559", "escuro e macio: toques graves abafados"},
};
}  // namespace

const SoundThemeInfo& soundThemeInfo(SoundTheme t) {
    int i = int(t);
    if (i < 0 || i >= kSoundThemeCount) i = 0;
    return kThemes[i];
}

const char* sfxFileName(Sfx s) {
    switch (s) {
        case Sfx::Navigate: return "navegar";
        case Sfx::Confirm: return "confirmar";
        case Sfx::Back: return "voltar";
        case Sfx::Tab: return "categoria";
        case Sfx::Open: return "abrir";
        case Sfx::Error: return "erro";
        case Sfx::Startup: return "entrada";
        case Sfx::Switch: return "transicao";
        default: return "desconhecido";
    }
}

std::vector<int16_t> synthesize(SoundTheme t, Sfx s) {
    switch (t) {
        case SoundTheme::Blades6717: return synthBlades6717(s);
        case SoundTheme::Nxe9199: return synthNxe9199(s);
        case SoundTheme::Kinect13604: return synthKinect13604(s);
        case SoundTheme::MetroV1_15574: return synthMetroV1_15574(s);
        case SoundTheme::MetroV2_17559: return synthMetroV2_17559(s);
        default: return synthNeutroX(s);
    }
}

bool writeWav(const std::string& path, const std::vector<int16_t>& pcm, int rate) {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    auto u32 = [f](uint32_t v) { uint8_t b[4] = {uint8_t(v), uint8_t(v >> 8), uint8_t(v >> 16), uint8_t(v >> 24)}; std::fwrite(b, 1, 4, f); };
    auto u16 = [f](uint16_t v) { uint8_t b[2] = {uint8_t(v), uint8_t(v >> 8)}; std::fwrite(b, 1, 2, f); };
    uint32_t dataBytes = uint32_t(pcm.size() * 2);
    std::fwrite("RIFF", 1, 4, f); u32(36 + dataBytes);
    std::fwrite("WAVE", 1, 4, f);
    std::fwrite("fmt ", 1, 4, f); u32(16); u16(1); u16(1); u32(uint32_t(rate)); u32(uint32_t(rate) * 2); u16(2); u16(16);
    std::fwrite("data", 1, 4, f); u32(dataBytes);
    for (int16_t v : pcm) u16(uint16_t(v));  // little-endian explicito (portavel)
    bool ok = std::ferror(f) == 0;
    std::fclose(f);
    return ok;
}

Mixer::Mixer() {
    bank_.resize(size_t(kSoundThemeCount) * kSfxCount);
    for (int t = 0; t < kSoundThemeCount; ++t)
        for (int s = 0; s < kSfxCount; ++s) bank_[size_t(t * kSfxCount + s)] = synthesize(SoundTheme(t), Sfx(s));
    voices_.reserve(16);
}

size_t Mixer::bankBytes() const {
    size_t n = 0;
    for (const auto& b : bank_) n += b.size() * sizeof(int16_t);
    return n;
}

void Mixer::setTheme(SoundTheme t) {
    const int ti = int(t);
    if (ti < 0 || ti >= kSoundThemeCount) return;
    bool load;
    {
        std::lock_guard<std::mutex> lock(mtx_);
        theme_ = t;
        // Outro dashboard: descarrega o conjunto original anterior e carrega o novo.
        // Voltar ao tema do Neutro X (menus) mantem o conjunto atual.
        load = ti != int(SoundTheme::NeutroX) && ti != origResident_ && !origBase_.empty();
    }
    if (load) makeResident(ti);
}

SoundTheme Mixer::theme() {
    std::lock_guard<std::mutex> lock(mtx_);
    return theme_;
}

void Mixer::play(Sfx s, float volume, float pitch) {
    SoundTheme t;
    {
        std::lock_guard<std::mutex> lock(mtx_);
        t = theme_;
    }
    playThemed(t, s, volume, pitch);
}

void Mixer::playThemed(SoundTheme t, Sfx s, float volume, float pitch) {
    int si = int(s), ti = int(t);
    if (si < 0 || si >= kSfxCount || ti < 0 || ti >= kSoundThemeCount) return;
    int key = ti * kSfxCount + si;
    std::lock_guard<std::mutex> lock(mtx_);
    // Temas de dashboard usam a fonte escolhida. Com "Originais", um som
    // ausente fica mudo (sem substituicao automatica por recriacao).
    const std::vector<int16_t>* data = &bank_[size_t(key)];
    if (t != SoundTheme::NeutroX && source_ == SoundSource::Originals) {
        if (ti != origResident_ || orig_.size() != size_t(kSfxCount) || orig_[size_t(si)].empty()) return;
        data = &orig_[size_t(si)];
    }
    // Limita vozes simultaneas: remove a mais antiga do mesmo efeito.
    int same = 0;
    for (const Voice& v : voices_) same += (v.key == key);
    if (same >= 3 || voices_.size() >= 12) {
        for (size_t i = 0; i < voices_.size(); ++i)
            if (voices_[i].key == key || voices_.size() >= 12) { voices_.erase(voices_.begin() + long(i)); break; }
    }
    Voice v;
    v.data = data;
    v.key = key;
    v.gain = std::max(0.0f, std::min(volume, 1.0f));
    v.step = std::max(0.25f, std::min(pitch, 4.0f));
    voices_.push_back(v);
}

void Mixer::setMasterVolume(float v) {
    std::lock_guard<std::mutex> lock(mtx_);
    master_ = std::max(0.0f, std::min(v, 1.0f));
}

int Mixer::activeVoices() {
    std::lock_guard<std::mutex> lock(mtx_);
    return int(voices_.size());
}

void Mixer::stopAll() {
    std::lock_guard<std::mutex> lock(mtx_);
    voices_.clear();
}

void Mixer::mix(int16_t* out, int frames) {
    std::lock_guard<std::mutex> lock(mtx_);
    for (int i = 0; i < frames; ++i) {
        float acc = 0;
        for (Voice& v : voices_) {
            const std::vector<int16_t>& d = *v.data;
            size_t p = size_t(v.pos);
            if (p + 1 < d.size()) {
                float frac = float(v.pos - double(p));
                acc += (d[p] * (1 - frac) + d[p + 1] * frac) * v.gain;
            }
            v.pos += v.step;
        }
        float o = acc * master_;
        o = std::max(-32767.0f, std::min(32767.0f, o));
        out[2 * i] = out[2 * i + 1] = int16_t(o);
    }
    voices_.erase(std::remove_if(voices_.begin(), voices_.end(),
                                 [](const Voice& v) { return size_t(v.pos) + 1 >= v.data->size(); }),
                  voices_.end());
}

}  // namespace nx
