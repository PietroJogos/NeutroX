// Neutro X - aplicacao: loop principal, pilha de telas, troca de dashboards.
#pragma once
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include "core/anim.h"
#include "core/screen.h"
#include "dashboards/dashboard.h"

namespace nx {

constexpr const char* kNeutroXVersion = "0.2.0";

struct FrameStats {
    uint64_t frames = 0;
    float fps = 0;
    float frameMs = 0;      // tempo de CPU do ultimo quadro (update+render)
    float worstFrameMs = 0;
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
};

}  // namespace nx
