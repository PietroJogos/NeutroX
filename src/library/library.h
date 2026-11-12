// Neutro X - biblioteca de jogos REAIS (Meus Jogos).
//
// Uma unica biblioteca e compartilhada por todas as interfaces; cada dashboard
// so muda a apresentacao. Nada e apagado do armazenamento: "remover" tira a
// entrada da biblioteca (e a pasta/arquivo vai para a lista de ignorados), e as
// capas ficam na pasta propria do Neutro X.
//
// Persistencia: <dados>/biblioteca.ini (texto, gravacao atomica, so quando algo
// mudou). Busca: em pastas monitoradas, ate a profundidade escolhida, lendo os
// cabecalhos reais (title_meta.h).
#pragma once
#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace nx {

enum class GameKind : int { Xex = 0, God = 1, Xbla = 2, App = 3 };
const char* gameKindName(GameKind k);  // "Jogo extraído (default.xex)" ...

struct GameEntry {
    std::string id;        // estavel: <TitleID>-<hash do caminho>
    std::string name;      // nome detectado (pacote STFS ou pasta)
    std::string userName;  // nome definido pelo usuario (opcional)
    uint32_t titleId = 0, mediaId = 0, version = 0;
    int discNumber = 0, discCount = 0;
    GameKind kind = GameKind::Xex;
    std::string path;    // o que e iniciado (default.xex ou cabecalho do pacote)
    std::string folder;  // pasta monitorada de origem ("" = adicionado manualmente)
    bool manual = false;
    bool favorite = false;
    bool available = true;  // o arquivo existia na ultima verificacao
    std::string duplicateOf;  // id da primeira copia do mesmo titulo (vazio = nao e duplicata)
    int64_t added = 0, lastPlayed = 0, lastSeen = 0;  // tempo Unix
    std::string cover;      // arquivo em <dados>/capas/ ("" = sem capa)
    int coverSource = 0;    // 0 nenhuma, 1 XboxUnity, 2 importada, 3 miniatura do pacote
    std::vector<uint8_t> thumb;  // miniatura PNG lida do pacote na busca (nao e gravada no .ini)
    std::string displayName() const { return userName.empty() ? name : userName; }
};

struct LibraryFolder {
    std::string path;
    int maxDepth = 4;
    bool enabled = true;
};

struct LibraryOptions {
    bool scanOnStart = true;
    int defaultDepth = 4;
    bool typeXex = true, typeGod = true, typeXbla = true, typeApps = false;
    bool detectDuplicates = true;
    bool autoCovers = true;      // baixar capas do XboxUnity quando faltarem
    bool launchPackages = false; // iniciar GOD/XBLA (experimental, desligado por padrao)
    int view = 0;                // 0 carrossel, 1 grade
    std::vector<std::string> ignore;  // nomes de pasta ou caminhos completos
};

struct ScanStats {
    int folders = 0;      // pastas visitadas
    int candidates = 0;   // arquivos analisados
    int found = 0;        // titulos validos
    int rejected = 0;     // .xex/pacotes recusados
    int duplicates = 0;
    std::vector<std::string> notes;  // motivos (limitado)
};

class Library {
public:
    // ---- persistencia
    bool load(const std::string& path);  // false se nao existir (biblioteca vazia)
    bool save(const std::string& path);  // so grava se mudou
    bool dirty() const { return dirty_; }
    void markDirty() { dirty_ = true; ++version_; }
    unsigned version() const { return version_; }  // muda a cada alteracao

    std::vector<GameEntry>& games() { return games_; }
    const std::vector<GameEntry>& games() const { return games_; }
    std::vector<LibraryFolder>& folders() { return folders_; }
    LibraryOptions& options() { return opts_; }
    const LibraryOptions& options() const { return opts_; }
    int64_t lastScan() const { return lastScan_; }
    const std::string& lastScanSummary() const { return lastScanSummary_; }

    GameEntry* find(const std::string& id);
    // ---- edicao
    bool addFolder(const std::string& path, std::string* why = nullptr);
    void removeFolder(size_t i);  // tambem marca como indisponiveis os jogos que so vinham dela
    void addIgnore(const std::string& pathOrName);
    void removeIgnore(size_t i);
    bool isIgnored(const std::string& path) const;
    // Analisa e adiciona um titulo escolhido manualmente (.xex ou pacote).
    bool addManual(const std::string& file, std::string* why, std::string* idOut = nullptr);
    // Tira da biblioteca (o jogo NAO e apagado). Jogo de pasta monitorada vai para os ignorados.
    bool remove(const std::string& id);
    void toggleFavorite(const std::string& id);

    // ---- busca (pode rodar numa thread; nao toca no estado da biblioteca)
    static std::vector<GameEntry> scan(const std::vector<LibraryFolder>& folders, const LibraryOptions& opts,
                                       ScanStats& stats, const std::atomic<bool>* cancel = nullptr,
                                       std::function<void(const std::string&)> progress = {});
    // Analisa um unico arquivo; false com o motivo se nao for um titulo valido.
    static bool probeFile(const std::string& file, const LibraryOptions& opts, GameEntry& out, std::string* why);
    // Junta o resultado de uma busca: preserva favoritos/capas/datas, adiciona novos,
    // marca como indisponiveis os que sumiram, recalcula duplicatas.
    void merge(const std::vector<GameEntry>& found, const ScanStats& st, int64_t now, int* added = nullptr);
    // Reverifica a existencia dos arquivos (rapido) e recalcula duplicatas.
    void refreshAvailability();
    void recomputeDuplicates();

    // ---- vistas
    enum class Filter { All = 0, Favorites, Recent, Unavailable, Search };
    // Indices de games() filtrados e ordenados por nome (Recent: por data de adicao).
    std::vector<int> view(Filter f, const std::string& query = "") const;

private:
    std::vector<GameEntry> games_;
    std::vector<LibraryFolder> folders_;
    LibraryOptions opts_;
    int64_t lastScan_ = 0;
    std::string lastScanSummary_;
    bool dirty_ = false;
    unsigned version_ = 1;
};

// Busca em segundo plano (uma por vez). O resultado e aplicado na thread principal.
class LibraryScanner {
public:
    ~LibraryScanner();
    bool start(const std::vector<LibraryFolder>& folders, const LibraryOptions& opts);
    bool running() const { return running_.load(); }
    bool finished() const { return done_.load(); }
    void cancel() { cancel_ = true; }
    // Retira o resultado (uma vez) quando finished().
    bool take(std::vector<GameEntry>& out, ScanStats& stats);
    std::string progress();  // pasta atual
    int foldersVisited();

private:
    std::thread th_;
    std::atomic<bool> running_{false}, done_{false}, cancel_{false};
    std::mutex m_;
    std::vector<GameEntry> result_;
    ScanStats stats_;
    std::string current_;
    int visited_ = 0;
};

// Utilidades
std::string stableGameId(uint32_t titleId, const std::string& path);
// Busca sem diferenciar maiusculas/acentos simples.
bool matchesQuery(const std::string& name, const std::string& query);

}  // namespace nx
