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
//   <pasta de dados>/local_audio/<tema>/<efeito>.wav
// (ex.: local_audio/blades_6717/navegar.wav; a pasta antiga sons_originais/
// ainda e aceita). Um arquivo opcional "mapeamento.ini" na pasta do tema
// permite usar outros nomes (ex.: navegar=meu_arquivo.wav); "manifesto.ini"
// (gerado por tools/sound_extract.py) informa quantos audios existem, quantos
// nao tem funcao confirmada e quantos falharam na conversao.
// Com a fonte "Originais" selecionada, um som original ausente NAO e
// substituido por recriacao: o efeito fica mudo e o motivo e informado.
// Somente o conjunto do dashboard ativo fica na memoria: ao trocar de
// dashboard o conjunto anterior e descarregado e o novo e carregado.
// ------------------------------------------------------------------------
// OriginalsWithRecreated: originais onde existirem e recriados SOMENTE nos efeitos
// sem original - opcao explicita do usuario (nunca aplicada automaticamente).
enum class SoundSource : int { Originals = 0, Recreated = 1, OriginalsWithRecreated = 2 };

// Contadores de reproducao (diagnostico: distinguir som ausente de falha de saida).
struct MixerStats {
    uint64_t requests = 0;          // pedidos de efeito
    uint64_t started = 0;           // vozes iniciadas (algo foi para o mixer)
    uint64_t mutedMissing = 0;      // pedidos mudos: original ausente com fonte "Originais"
    uint64_t mutedNotResident = 0;  // pedidos mudos: conjunto do tema ainda nao carregado
    uint64_t recreatedFallback = 0; // pedidos atendidos por recriado (modo originais + recriados)
    uint64_t framesMixed = 0;       // amostras estereo produzidas pelo mixer
    uint64_t nonSilentFrames = 0;   // amostras com sinal (|x| > 0)
    int peak = 0;                   // pico absoluto desde o inicio
    int lastVoices = 0;
    // Efeitos que ja ficaram mudos ao menos uma vez (bit = Sfx), por tema.
    uint32_t mutedMask[kSoundThemeCount] = {};
    // Vozes originais que contribuíram pelo menos uma amostra PCM com sinal.
    uint64_t mixedOriginal[kSoundThemeCount][kSfxCount] = {};
};

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
    // Total de arquivos .wav originais presentes na pasta da versao (inclui os
    // que nao tem funcao identificada), p.ex. instalados por tools/sound_extract.py.
    int filesInFolder[kSoundThemeCount] = {};
    // Dados do manifesto.ini da versao (-1 = sem manifesto).
    int manifestFiles[kSoundThemeCount] = {-1, -1, -1, -1, -1, -1};
    int unconfirmed[kSoundThemeCount] = {-1, -1, -1, -1, -1, -1};
    int convErrors[kSoundThemeCount] = {-1, -1, -1, -1, -1, -1};
    int externalRefs[kSoundThemeCount] = {-1, -1, -1, -1, -1, -1};
    std::string situation[kSoundThemeCount];  // ex.: "parcial: 7 de 8 efeitos confirmados"
    std::string xexState[kSoundThemeCount];   // aberto / criptografado / falha
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
    // (Re)verifica a pasta de sons originais (termina com separador): valida
    // todas as versoes para o relatorio, mas so mantem na memoria o conjunto
    // do tema ativo. Thread-safe.
    void loadOriginals(const std::string& baseDir);
    // Tema cujos sons originais estao na memoria (-1 = nenhum).
    int residentOriginals();
    size_t originalBytes();
    OrigReport originalsReport();
    void play(Sfx s, float volume = 1.0f, float pitch = 1.0f);                // tema atual
    void playThemed(SoundTheme t, Sfx s, float volume = 1.0f, float pitch = 1.0f);
    MixerStats stats();
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
        bool originalPending = false;
    };
    std::vector<std::vector<int16_t>> bank_;  // recriados [tema * kSfxCount + sfx]
    std::vector<std::vector<int16_t>> orig_;  // originais do tema residente [sfx] (vazio = ausente)
    int origResident_ = -1;
    std::string origBase_;
    OrigReport report_;
    // Le e decodifica os efeitos de um tema (fora do mutex).
    static void readTheme(const std::string& baseDir, int t, OrigReport& rep, std::vector<std::vector<int16_t>>* keep);
    void makeResident(int t);
    std::vector<Voice> voices_;
    std::mutex mtx_;
    float master_ = 0.8f;
    MixerStats stats_;
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
    virtual MixerStats stats() { return {}; }
    // Toca um efeito de um tema especifico sem trocar o tema ativo (teste de som).
    virtual void playThemed(SoundTheme t, Sfx s, float volume = 1.0f) { (void)t; play(s, volume, 1.0f); }
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
    MixerStats stats() override { return m_.stats(); }
    void playThemed(SoundTheme t, Sfx s, float volume) override { m_.playThemed(t, s, volume, 1.0f); }

private:
    Mixer& m_;
};

// Gera as amostras mono (44,1 kHz) de um efeito de um tema.
std::vector<int16_t> synthesize(SoundTheme t, Sfx s);

// Grava um WAV PCM 16 bits mono.
bool writeWav(const std::string& path, const std::vector<int16_t>& pcm, int rate = kAudioRate);

}  // namespace nx
