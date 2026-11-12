// Neutro X - aplicacao: loop principal, pilha de telas, troca de dashboards.
#pragma once
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "core/anim.h"
#include "core/screen.h"
#include "dashboards/dashboard.h"
#include "library/covers.h"
#include "library/library.h"
#include "platform/platform.h"
#include "system/launcher.h"

namespace nx {

constexpr const char* kNeutroXVersion = "0.9.2";

struct FrameStats {
    uint64_t frames = 0;
    float fps = 0;
    float frameMs = 0;      // tempo de CPU do ultimo quadro (update+render+apresentacao)
    float worstFrameMs = 0;
    // Instrumentacao: partes do ultimo quadro (ms).
    float updateMs = 0;     // entrada + logica + acoes adiadas (carregamentos entram aqui)
    float renderMs = 0;     // desenho (gravacao + rasterizacao)
    float presentMs = 0;    // envio a tela (no console inclui a espera do vblank)
    uint64_t slowFrames = 0;  // quadros acima de 33 ms (abaixo de 30 FPS)
};

// Tempo por tela (para achar onde ocorrem as travadas).
struct ScreenTiming {
    uint64_t frames = 0, slow = 0;
    double sumMs = 0, maxMs = 0, maxUpdateMs = 0, maxRenderMs = 0;
};

class App {
public:
    explicit App(IPlatform& platform);
    ~App();

    bool init();
    // Executa um quadro completo (entrada -> atualizacao -> desenho -> apresentacao).
    void frame(const RawPadState& raw, float dt);
    int run();  // loop ate encerrar
    void shutdown();

    // --- Pilha de telas (operacoes adiadas ate o fim do quadro) ---
    void push(std::unique_ptr<Screen> s);
    void pop();
    void replaceAll(std::unique_ptr<Screen> s);

    // --- Fluxos de alto nivel ---
    void openDashboard(DashboardId id);  // com transicao e carregamento
    void openMainMenu();
    void openGlobalMenu();
    void openSettings();
    void openAbout();
    void requestQuit();
    bool quitRequested() const { return quit_; }
    // Saida controlada: Aurora / dashboard oficial / saida padrao do sistema.
    // Retorna false (com o motivo em 'why') se o destino nao estiver disponivel; nada e encerrado.
    bool requestExitTo(ExitTarget t, std::string* why = nullptr);
    LaunchTarget exitTargetInfo(ExitTarget t);
    // Iniciar outro titulo (.xex) ao encerrar. false se a plataforma nao suportar ou o arquivo nao existir.
    bool requestLaunchTitle(const std::string& path, std::string* why = nullptr);
    // Caminho a iniciar depois do encerramento (vazio = saida padrao do sistema).
    const std::string& launchOnExit() const { return launchOnExit_; }
    std::vector<StorageRoot> storageRoots();
    // Mesmas unidades, consultadas no maximo a cada 5 s (para telas que desenham caminhos).
    const std::vector<StorageRoot>& storageRootsCached();
    // Perfil conectado (consultado a cada 2 s); 'profileVersion' muda quando o perfil muda.
    const UserProfile& profile() const { return profile_; }
    unsigned profileVersion() const { return profileVersion_; }
    void refreshProfile();
    const std::map<std::string, ScreenTiming>& screenTimings() const { return timings_; }

    // --- Biblioteca de jogos (Meus Jogos)
    Library& library() { return lib_; }
    void saveLibrary(bool force = false);
    bool startLibraryScan(bool manual);
    bool libraryScanning() const { return scanner_.running(); }
    std::string libraryScanProgress() { return scanner_.progress(); }
    int libraryScanFolders() { return scanner_.foldersVisited(); }
    void openLibrary();
    void openGameDetails(const std::string& gameId);
    void openLibrarySettings();
    // Interface cuja aparencia a biblioteca usa (a ativa ou a ultima aberta; Count = Neutro X).
    DashboardId libraryStyle() const;
    CoverService& coverService() { return coverSvc_; }
    CoverTextures& coverTextures() { return coverTex_; }
    // Pede a capa (XboxUnity). manual: mostra o resultado; force: baixa de novo.
    void requestCover(const std::string& gameId, bool force, bool manual);
    int importCovers();  // de capas_importar/; retorna quantas foram aplicadas
    // Inicia um jogo da biblioteca (valida caminho/tipo; encerra o Neutro X).
    bool launchGame(const std::string& gameId, std::string* why);

    // --- Utilidades ---
    Context& ctx() { return ctx_; }
    Settings& settings() { return settings_; }
    void applySettings();  // aplica volume/animacoes apos alteracao
    bool saveSettings();
    void toast(const std::string& msg);
    // Sons originais: pasta local fora do repositorio.
    std::string originalSoundsDir() const;
    void reloadOriginalSounds();
    void openSoundStatus();
    const FrameStats& stats() const { return stats_; }
    TextureId previewTexture(DashboardId id, bool smallSize) const;
    int screenCount() const { return int(stack_.size()); }
    const char* topScreenName() const;
    std::string topDebugState() const;  // estado do dashboard no topo (testes)
    bool topDebugSelect(int group, int item);  // selecao direta no dashboard do topo (testes)
    bool hasActiveDashboard() const;
    DashboardId activeDashboard() const { return activeDash_; }
    bool busy() const { return phase_ != Phase::Idle || !pending_.empty(); }

    // Somente para testes: substitui a fabrica de dashboards (ex.: simular falha de carga).
    using DashboardFactory = std::function<std::unique_ptr<Dashboard>(DashboardId)>;
    void setDashboardFactoryForTests(DashboardFactory f) { factoryOverride_ = std::move(f); }

private:
    enum class Phase { Idle, FadeOut, Loading, FadeIn };
    void runPending();
    void clearStack();
    void renderScreens(IRenderer& r);
    void renderOverlays(IRenderer& r);
    void buildPreviews();
    void releasePreviews();
    void beginTransition(std::function<void()> action, bool loading, DashboardId target);

    IPlatform& platform_;
    Settings settings_;
    Context ctx_;
    InputMapper input_;
    std::vector<std::unique_ptr<Screen>> stack_;
    std::vector<std::function<void()>> pending_;
    TextureId previewBig_[kDashboardCount];
    TextureId previewSmall_[kDashboardCount];

    Phase phase_ = Phase::Idle;
    float phaseT_ = 0;
    std::function<void()> transitionAction_;
    bool transitionLoading_ = false;
    DashboardId loadingTarget_ = DashboardId::Blades;
    DashboardId activeDash_ = DashboardId::Count;
    double loadStart_ = 0;

    DashboardFactory factoryOverride_;
    std::string toastMsg_;
    float toastT_ = 0;
    bool quit_ = false;
    bool initialized_ = false;
    FrameStats stats_;
    float fpsAccum_ = 0;
    int fpsFrames_ = 0;
    std::string launchOnExit_;
    UserProfile profile_;
    unsigned profileVersion_ = 0;
    float profileTimer_ = 0;
    float audioCheckTimer_ = 0;
    uint32_t loggedMutes_[kSoundThemeCount] = {};
    std::map<std::string, ScreenTiming> timings_;
    double lastSpikeLog_ = -10;
    void checkAudioDiagnostics();
    void logTimings();
    void pollLibrary(float dt);
    void publishLibrary();
    Library lib_;
    LibraryScanner scanner_;
    bool scanManual_ = false;
    CoverService coverSvc_;
    CoverTextures coverTex_;
    float libSaveTimer_ = 0;
    DashboardId lastDash_ = DashboardId::Count;
    std::vector<StorageRoot> rootsCache_;
    double rootsAt_ = -100;
};

}  // namespace nx
