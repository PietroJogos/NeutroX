// Neutro X - audio: efeitos sintetizados proceduralmente + mixer portavel.
//
// Nenhum som e extraido de arquivos originais: todos os efeitos sao gerados
// por codigo (osciladores, ruido filtrado e envelopes) ao iniciar o aplicativo.
// Cada dashboard tem o PROPRIO tema sonoro (audio/themes/<versao>.cpp); o
// tema ativo muda automaticamente com a interface exibida.
// A plataforma "puxa" amostras do Mixer em sua thread de audio.
#pragma once
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace nx {

enum class Sfx : int {
    Navigate = 0,   // mover selecao
    Confirm,        // confirmar (A)
    Back,           // voltar (B)
    Tab,            // trocar categoria/secao (LB/RB)
    Open,           // abrir menu/painel
    Error,          // acao invalida
    Startup,        // abertura (do app ou da interface)
    Switch,         // transicao
    Count
};
constexpr int kSfxCount = int(Sfx::Count);

// Temas sonoros: um para o shell do Neutro X e um por versao de dashboard.
enum class SoundTheme : int { NeutroX = 0, Blades6717, Nxe9199, Kinect13604, MetroV1_15574, MetroV2_17559, Count };
constexpr int kSoundThemeCount = int(SoundTheme::Count);

struct SoundThemeInfo {
    SoundTheme theme;
    const char* folder;  // pasta em assets/sounds/
    const char* name;
    const char* character;  // descricao do timbre
};
const SoundThemeInfo& soundThemeInfo(SoundTheme t);
const char* sfxFileName(Sfx s);  // nome do arquivo exportado (sem extensao)

constexpr int kAudioRate = 44100;

// ------------------------------------------------------------------------
// Sons ORIGINAIS dos dashboards (carregados de pasta local do usuario).
//
// O Neutro X nao inclui, nao extrai e nao redistribui audio proprietario.
// Se o usuario tiver os arquivos e permissao para usa-los, basta coloca-los em
//   <pasta de dados>/sons_originais/<tema>/<efeito>.wav
// (ex.: sons_originais/blades_6717/navegar.wav). Um arquivo opcional
// "mapeamento.ini" na pasta do tema permite usar outros nomes
// (ex.: navegar=meu_arquivo.wav).
// Com a fonte "Originais" selecionada, um som original ausente NAO e
// substituido por recriacao: o efeito fica mudo e o motivo e informado.
// ------------------------------------------------------------------------
enum class SoundSource : int { Originals = 0, Recreated = 1 };

enum class OrigStatus : int { Loaded, Missing, Unsupported, Invalid };

struct OrigEntry {
    OrigStatus status = OrigStatus::Missing;
    std::string file;    // caminho procurado/carregado
    std::string reason;  // motivo quando nao carregado
    float seconds = 0;
};

struct OrigReport {
    std::string baseDir;
    OrigEntry e[kSoundThemeCount][kSfxCount];
    int loaded(SoundTheme t) const;
    int totalLoaded() const;
};

// Decodifica WAV (PCM 8/16/24 bits ou float 32, mono/estereo, qualquer taxa)
// para mono 44,1 kHz. XMA e outros formatos proprietarios nao sao suportados.
bool decodeWav(const std::vector<uint8_t>& bytes, std::vector<int16_t>& out, std::string& reason);

class Mixer {
public:
    Mixer();
    void setTheme(SoundTheme t);
    SoundTheme theme();
    void setSource(SoundSource s);
    SoundSource source();
    // (Re)carrega sons originais da pasta (termina com separador). Thread-safe.
    void loadOriginals(const std::string& baseDir);
    OrigReport originalsReport();
    void play(Sfx s, float volume = 1.0f, float pitch = 1.0f);                // tema atual
    void playThemed(SoundTheme t, Sfx s, float volume = 1.0f, float pitch = 1.0f);
    void setMasterVolume(float v);  // 0..1
    float masterVolume() const { return master_; }
    // Mistura 'frames' amostras estereo intercaladas (L, R) em int16.
    void mix(int16_t* out, int frames);
    int activeVoices();
    void stopAll();
    size_t bankBytes() const;

private:
    struct Voice {
        const std::vector<int16_t>* data = nullptr;
        int key = -1;  // tema*kSfxCount + sfx
        double pos = 0;
        float step = 1;
        float gain = 1;
    };
    std::vector<std::vector<int16_t>> bank_;  // recriados [tema * kSfxCount + sfx]
    std::vector<std::vector<int16_t>> orig_;  // originais (vazio = ausente)
    OrigReport report_;
    std::vector<Voice> voices_;
    std::mutex mtx_;
    float master_ = 0.8f;
    SoundTheme theme_ = SoundTheme::NeutroX;
    SoundSource source_ = SoundSource::Originals;
};

// Interface usada pelo restante do aplicativo.
class IAudio {
public:
    virtual ~IAudio() = default;
    virtual void play(Sfx s, float volume = 1.0f, float pitch = 1.0f) = 0;
    virtual void setVolume(float v) = 0;
    virtual void setTheme(SoundTheme t) = 0;
    virtual SoundTheme theme() = 0;
    virtual void setSource(SoundSource s) = 0;
    virtual void loadOriginals(const std::string& baseDir) = 0;
    virtual OrigReport originalsReport() = 0;
};

// Implementacao baseada no Mixer (a plataforma consome as amostras).
class MixerAudio : public IAudio {
public:
    explicit MixerAudio(Mixer& m) : m_(m) {}
    void play(Sfx s, float volume, float pitch) override { m_.play(s, volume, pitch); }
    void setVolume(float v) override { m_.setMasterVolume(v); }
    void setTheme(SoundTheme t) override { m_.setTheme(t); }
    SoundTheme theme() override { return m_.theme(); }
    void setSource(SoundSource s) override { m_.setSource(s); }
    void loadOriginals(const std::string& baseDir) override { m_.loadOriginals(baseDir); }
    OrigReport originalsReport() override { return m_.originalsReport(); }

private:
    Mixer& m_;
};

// Gera as amostras mono (44,1 kHz) de um efeito de um tema.
std::vector<int16_t> synthesize(SoundTheme t, Sfx s);

// Grava um WAV PCM 16 bits mono.
bool writeWav(const std::string& path, const std::vector<int16_t>& pcm, int rate = kAudioRate);

}  // namespace nx
