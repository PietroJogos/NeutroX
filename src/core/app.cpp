// Neutro X - aplicacao. Ver app.h.
#include "core/app.h"

#include <cstdio>
#include <exception>
#include <filesystem>
#include "core/log.h"
#include "graphics/font.h"
#include "graphics/soft_renderer.h"
#include "graphics/ui.h"
#include "platform/platform.h"
#include "shell/screens.h"

namespace nx {

AnimConfig& animConfig() {
    static AnimConfig cfg;
    return cfg;
}

Color Context::accent() const {
    int i = settings ? settings->accentTheme : 0;
    if (i < 0 || i >= kAccentThemeCount) i = 0;
    return Color::hex(kAccentThemes[i].rgb);
}

void Context::sfx(Sfx s, float vol) const {
    if (audio) audio->play(s, vol, 1.0f);
}

App::App(IPlatform& platform) : platform_(platform) {
    for (int i = 0; i < kDashboardCount; ++i) previewBig_[i] = previewSmall_[i] = kInvalidTexture;
}

App::~App() { shutdown(); }

bool App::init() {
    ctx_.app = this;
    ctx_.platform = &platform_;
    ctx_.renderer = &platform_.renderer();
    ctx_.audio = &platform_.audio();
    ctx_.settings = &settings_;

    std::string path = platform_.dataPath() + "neutrox.ini";
    LoadResult lr = loadSettingsFile(path, settings_);
    if (lr == LoadResult::Missing) {
        logInfo("Configuracoes nao encontradas; usando padroes (%s)", path.c_str());
    } else if (lr == LoadResult::Corrupt) {
        logWarn("Arquivo de configuracoes com valores invalidos; padroes aplicados onde necessario");
        toast("Configurações inválidas foram restauradas");
        saveSettings();
    }
    applySettings();
    reloadOriginalSounds();
    buildPreviews();
    stack_.push_back(createBootScreen());
    stack_.back()->onEnter(ctx_);
    initialized_ = true;
    logInfo("Neutro X %s iniciado (plataforma: %s, %dx%d)", kNeutroXVersion, platform_.name(),
            ctx_.renderer->pixelWidth(), ctx_.renderer->pixelHeight());
    return true;
}

void App::shutdown() {
    if (!initialized_) return;
    clearStack();
    pending_.clear();
    releasePreviews();
    initialized_ = false;
    logInfo("Neutro X encerrado");
}

void App::buildPreviews() {
    // Miniaturas desenhadas por codigo (sem imagens externas).
    // Grande: 640x360 (painel do menu). Pequena: 160x90 (lista).
    for (int i = 0; i < kDashboardCount; ++i) {
        const DashboardInfo& info = dashboardInfo(DashboardId(i));
        {
            SoftRenderer canvas(640, 360);
            canvas.beginFrame();
            info.drawPreview(canvas, 0.0f);
            previewBig_[i] = ctx_.renderer->createTexture(640, 360, canvas.pixels());
        }
        {
            SoftRenderer canvas(160, 90);
            canvas.beginFrame();
            info.drawPreview(canvas, 0.0f);
            previewSmall_[i] = ctx_.renderer->createTexture(160, 90, canvas.pixels());
        }
    }
}

void App::releasePreviews() {
    for (int i = 0; i < kDashboardCount; ++i) {
        if (previewBig_[i] != kInvalidTexture) ctx_.renderer->destroyTexture(previewBig_[i]);
        if (previewSmall_[i] != kInvalidTexture) ctx_.renderer->destroyTexture(previewSmall_[i]);
        previewBig_[i] = previewSmall_[i] = kInvalidTexture;
    }
}

TextureId App::previewTexture(DashboardId id, bool smallSize) const {
    int i = int(id);
    if (i < 0 || i >= kDashboardCount) return kInvalidTexture;
    return smallSize ? previewSmall_[i] : previewBig_[i];
}

void App::applySettings() {
    settings_.sanitize();
    animConfig().enabled = settings_.animations;
    animConfig().speed = settings_.animSpeed();
    if (ctx_.audio) {
        ctx_.audio->setVolume(settings_.sfxVolume / 100.0f);
        ctx_.audio->setSource(settings_.dashboardSounds == 1 ? SoundSource::Recreated : SoundSource::Originals);
    }
}

std::string App::originalSoundsDir() const {
    const std::string base = platform_.dataPath();
    // Pasta atual: local_audio/. A antiga sons_originais/ continua aceita quando e a unica existente.
    try {
        auto isDir = [](const std::string& p) { return std::filesystem::is_directory(std::filesystem::u8path(p)); };
        if (!isDir(base + "local_audio") && isDir(base + "sons_originais")) return base + "sons_originais/";
    } catch (const std::exception&) {
    }
    return base + "local_audio/";
}

void App::reloadOriginalSounds() {
    const std::string base = originalSoundsDir();
    // Prepara a estrutura de pastas (uma por versao) e um guia, sem tocar em
    // nada fora da pasta de dados do Neutro X.
    try {
        for (int t = 1; t < kSoundThemeCount; ++t)
            std::filesystem::create_directories(std::filesystem::u8path(base + soundThemeInfo(SoundTheme(t)).folder));
        std::string guide = base + "LEIA-ME.txt";
        if (!std::filesystem::exists(std::filesystem::u8path(guide))) {
            if (FILE* f = std::fopen(guide.c_str(), "wb")) {
                std::fputs("NEUTRO X - SONS ORIGINAIS DOS DASHBOARDS\r\n\r\n"
                           "Coloque aqui, por sua conta, os arquivos de audio originais que voce tem permissao de usar.\r\n"
                           "O Neutro X NAO inclui, NAO extrai e NAO redistribui audio proprietario.\r\n\r\n"
                           "Uma pasta por versao: blades_6717, nxe_9199, kinect_13604, metro_v1_15574, metro_v2_17559\r\n"
                           "Nomes esperados (WAV PCM 8/16/24 bits ou float, mono ou estereo, qualquer taxa):\r\n"
                           "  navegar.wav confirmar.wav voltar.wav categoria.wav abrir.wav erro.wav entrada.wav transicao.wav\r\n\r\n"
                           "Para usar outros nomes, crie mapeamento.ini na pasta da versao, por exemplo:\r\n"
                           "  navegar=meu_som_de_navegacao.wav\r\n\r\n"
                           "No PC, tools/sound_extract.py faz isso automaticamente a partir de XEX nao criptografados\r\n"
                           "(gera mapeamento.ini, ausentes.ini, manifesto.ini e relatorio.txt por versao).\r\n\r\n"
                           "XMA (formato do console) nao e suportado: converta para WAV PCM.\r\n"
                           "Em Configuracoes > Sons originais: estado, veja o que foi carregado e o motivo de cada falha.\r\n",
                           f);
                std::fclose(f);
            }
        }
    } catch (const std::exception& e) {
        logWarn("Nao foi possivel preparar a pasta de sons originais: %s", e.what());
    }
    if (!ctx_.audio) return;
    ctx_.audio->loadOriginals(base);
    OrigReport rep = ctx_.audio->originalsReport();
    for (int t = 1; t < kSoundThemeCount; ++t)
        logInfo("Sons originais %s: %d de %d validos; %d arquivos na pasta; nao confirmados %d; erros de conversao %d",
                soundThemeInfo(SoundTheme(t)).name, rep.loaded(SoundTheme(t)), kSfxCount, rep.filesInFolder[t],
                rep.unconfirmed[t], rep.convErrors[t]);
}

void App::openSoundStatus() { push(createSoundStatusScreen()); }

bool App::saveSettings() {
    std::string path = platform_.dataPath() + "neutrox.ini";
    bool ok = saveSettingsFile(path, settings_);
    if (!ok) {
        logError("Falha ao salvar configuracoes em %s", path.c_str());
        toast("Não foi possível salvar as configurações");
    }
    return ok;
}

void App::toast(const std::string& msg) {
    toastMsg_ = msg;
    toastT_ = 3.0f;
}

// ---------------------------------------------------------------- pilha

void App::push(std::unique_ptr<Screen> s) {
    Screen* raw = s.release();
    pending_.push_back([this, raw]() {
        stack_.emplace_back(raw);
        input_.reset();
        raw->onEnter(ctx_);
    });
}

void App::pop() {
    pending_.push_back([this]() {
        if (stack_.empty()) return;
        Screen* top = stack_.back().get();
        if (auto* d = dynamic_cast<Dashboard*>(top)) {
            d->unload(ctx_);
            activeDash_ = DashboardId::Count;
        }
        top->onExit(ctx_);
        stack_.pop_back();
        input_.reset();
    });
}

void App::replaceAll(std::unique_ptr<Screen> s) {
    Screen* raw = s.release();
    pending_.push_back([this, raw]() {
        clearStack();
        stack_.emplace_back(raw);
        input_.reset();
        raw->onEnter(ctx_);
    });
}

void App::clearStack() {
    while (!stack_.empty()) {
        Screen* top = stack_.back().get();
        if (auto* d = dynamic_cast<Dashboard*>(top)) d->unload(ctx_);
        top->onExit(ctx_);
        stack_.pop_back();
    }
    activeDash_ = DashboardId::Count;
}

void App::runPending() {
    // Acoes podem enfileirar novas acoes; processa em lotes.
    for (int guard = 0; guard < 8 && !pending_.empty(); ++guard) {
        std::vector<std::function<void()>> batch;
        batch.swap(pending_);
        for (auto& fn : batch) fn();
    }
}

const char* App::topScreenName() const { return stack_.empty() ? "" : stack_.back()->name(); }

bool App::hasActiveDashboard() const { return activeDash_ != DashboardId::Count; }

bool App::topDebugSelect(int group, int item) {
    if (stack_.empty()) return false;
    if (auto* d = dynamic_cast<Dashboard*>(stack_.back().get())) return d->debugSelect(group, item);
    return false;
}

std::string App::topDebugState() const {
    if (stack_.empty()) return "";
    if (auto* d = dynamic_cast<const Dashboard*>(stack_.back().get())) return d->debugState();
    return "";
}

// ---------------------------------------------------------- transicoes

void App::beginTransition(std::function<void()> action, bool loading, DashboardId target) {
    if (phase_ != Phase::Idle) return;  // ignora pedidos durante outra transicao
    transitionAction_ = std::move(action);
    transitionLoading_ = loading;
    loadingTarget_ = target;
    phase_ = Phase::FadeOut;
    phaseT_ = 0;
}

void App::openDashboard(DashboardId id) {
    ctx_.sfx(Sfx::Switch);
    beginTransition([this, id]() {
        // 1) descarrega a interface anterior (e quaisquer telas acima dela)
        clearStack();
        // 2) cria e carrega a nova
        const DashboardInfo& info = dashboardInfo(id);
        std::unique_ptr<Dashboard> dash;
        std::string err;
        bool ok = false;
        try {
            dash = factoryOverride_ ? factoryOverride_(id) : info.create();
            ok = dash && dash->load(ctx_, err);
        } catch (const std::exception& e) {
            err = e.what();
            ok = false;
        } catch (...) {
            err = "erro desconhecido";
            ok = false;
        }
        if (!ok) {
            if (dash) dash->unload(ctx_);
            logError("Falha ao carregar dashboard %s: %s", info.key, err.c_str());
            stack_.push_back(createErrorScreen("Não foi possível abrir " + std::string(info.name), err));
            stack_.back()->onEnter(ctx_);
            ctx_.sfx(Sfx::Error);
            return;
        }
        activeDash_ = id;
        stack_.push_back(std::move(dash));
        stack_.back()->onEnter(ctx_);
        // Efeitos sonoros passam a ser os da interface selecionada.
        const SoundTheme theme = soundThemeFor(id);
        ctx_.audio->setTheme(theme);
        ctx_.audio->play(Sfx::Startup, 1.0f, 1.0f);
        if (settings_.dashboardSounds == 0) {
            int n = ctx_.audio->originalsReport().loaded(theme);
            if (n < kSfxCount)
                toast("Sons originais do " + std::string(info.name) + ": " + std::to_string(n) + " de " +
                      std::to_string(kSfxCount) + " encontrados (veja Configurações)");
        }
        logInfo("Dashboard %s carregado; texturas: %d (%u KB)", info.key, ctx_.renderer->textureCount(),
                unsigned(ctx_.renderer->textureBytes() / 1024));
    }, true, id);
}

void App::openMainMenu() {
    beginTransition([this]() {
        clearStack();
        stack_.push_back(createMainMenuScreen());
        stack_.back()->onEnter(ctx_);
    }, false, DashboardId::Count);
}

void App::openGlobalMenu() { push(createGlobalMenuScreen()); }
void App::openSettings() { push(createSettingsScreen()); }
void App::openAbout() { push(createAboutScreen()); }

void App::requestQuit() { quit_ = true; }

// ---------------------------------------------------------------- quadro

void App::frame(const RawPadState& raw, float dt) {
    double t0 = platform_.timeSeconds();
    if (dt > 0.1f) dt = 0.1f;  // evita saltos apos travamentos/depuracao
    ctx_.time += dt;

    InputFrame in = input_.update(raw, dt);
    if (in.disconnectedNow) toast("Controle desconectado. Reconecte para continuar.");
    if (in.connectedNow && stats_.frames > 2) toast("Controle conectado");

    // Transicao em andamento: entrada bloqueada.
    const float kFade = 0.22f;
    if (phase_ != Phase::Idle) {
        phaseT_ += dt;
        if (phase_ == Phase::FadeOut && phaseT_ >= kFade) {
            if (transitionLoading_) {
                phase_ = Phase::Loading;
                phaseT_ = 0;
                loadStart_ = platform_.timeSeconds();
            } else {
                if (transitionAction_) transitionAction_();
                transitionAction_ = nullptr;
                phase_ = Phase::FadeIn;
                phaseT_ = 0;
            }
        } else if (phase_ == Phase::Loading && phaseT_ >= 0.05f) {
            // A tela de carregamento ja foi exibida pelo menos um quadro.
            if (transitionAction_) transitionAction_();
            transitionAction_ = nullptr;
            phase_ = Phase::FadeIn;
            phaseT_ = 0;
            input_.reset();
        } else if (phase_ == Phase::FadeIn && phaseT_ >= kFade) {
            phase_ = Phase::Idle;
        }
    } else if (!stack_.empty()) {
        Screen* top = stack_.back().get();
        if (in.press(Button::Back)) {
            settings_.showFps = !settings_.showFps;
        }
        if (in.press(Button::Start) && top->wantsGlobalMenu()) {
            ctx_.sfx(Sfx::Open);
            openGlobalMenu();
        } else {
            top->update(ctx_, in, dt);
        }
    }
    // Telas abaixo de overlays continuam animando (sem entrada).
    if (stack_.size() >= 2 && stack_.back()->isOverlay()) {
        InputFrame none;
        for (size_t i = 0; i + 1 < stack_.size(); ++i)
            if (i + 2 == stack_.size()) stack_[i]->update(ctx_, none, dt);
    }

    runPending();
    if (toastT_ > 0) toastT_ -= dt;

    // Tema sonoro segue a tela do topo: dashboard -> sons da versao;
    // telas do Neutro X (menus, configuracoes, overlays) -> sons do Neutro X.
    {
        SoundTheme want = SoundTheme::NeutroX;
        if (!stack_.empty())
            if (auto* d = dynamic_cast<Dashboard*>(stack_.back().get())) want = soundThemeFor(d->id());
        if (ctx_.audio && ctx_.audio->theme() != want) ctx_.audio->setTheme(want);
    }

    // ---- desenho
    IRenderer& r = *ctx_.renderer;
    r.beginFrame();
    if (phase_ == Phase::Loading) {
        drawLoadingScreen(r, dashboardInfo(loadingTarget_), float(ctx_.time));
    } else {
        renderScreens(r);
        float fade = 0;
        if (phase_ == Phase::FadeOut) fade = clampf(phaseT_ / kFade, 0, 1);
        else if (phase_ == Phase::FadeIn) fade = 1.0f - clampf(phaseT_ / kFade, 0, 1);
        if (fade > 0) {
            if (transitionLoading_ && phase_ == Phase::FadeOut) {
                // escurece em direcao a tela de carregamento
                r.pushAlpha(fade);
                drawLoadingScreen(r, dashboardInfo(loadingTarget_), float(ctx_.time));
                r.popAlpha();
            } else {
                r.fillRect(Rectf{0, 0, float(kLogicalW), float(kLogicalH)}, Color(0, 0, 0, uint8_t(fade * 255)));
            }
        }
    }
    renderOverlays(r);
    r.endFrame();
    platform_.present();

    // ---- estatisticas
    double t1 = platform_.timeSeconds();
    stats_.frames++;
    stats_.frameMs = float((t1 - t0) * 1000.0);
    if (stats_.frames > 30 && stats_.frameMs > stats_.worstFrameMs) stats_.worstFrameMs = stats_.frameMs;
    fpsAccum_ += dt;
    fpsFrames_++;
    if (fpsAccum_ >= 0.5f) {
        stats_.fps = fpsFrames_ / fpsAccum_;
        fpsAccum_ = 0;
        fpsFrames_ = 0;
    }
}

void App::renderScreens(IRenderer& r) {
    if (stack_.empty()) {
        r.clear(Color(0, 0, 0));
        return;
    }
    // Encontra a tela opaca mais alta e desenha dali para cima.
    size_t start = stack_.size() - 1;
    while (start > 0 && stack_[start]->isOverlay()) --start;
    for (size_t i = start; i < stack_.size(); ++i) stack_[i]->render(ctx_, r);
}

void App::renderOverlays(IRenderer& r) {
    if (toastT_ > 0 && !toastMsg_.empty()) {
        float a = clampf(toastT_ / 0.3f, 0, 1) * clampf((3.0f - toastT_) / 0.2f, 0, 1);
        TextStyle st;
        st.size = 22;
        float w = r.measureText(toastMsg_.c_str(), st) + 48;
        Rectf box{(kLogicalW - w) * 0.5f, 600 + (1 - a) * 20, w, 50};
        r.pushAlpha(a);
        r.fillRoundRect(box, 10, Color(18, 20, 24, 230));
        r.strokeRoundRect(box, 10, 1.5f, ctx_.accent().withAlpha(200));
        st.align = Align::Center;
        r.drawText(toastMsg_.c_str(), box.cx(), box.y + 13, st);
        r.popAlpha();
    }
    if (settings_.showFps) {
        char buf[96];
        std::snprintf(buf, sizeof(buf), "%.0f FPS  %.1f ms  tex %d/%uKB", stats_.fps, stats_.frameMs,
                      r.textureCount(), unsigned(r.textureBytes() / 1024));
        TextStyle st;
        st.size = 16;
        float w = r.measureText(buf, st) + 16;
        r.fillRoundRect(Rectf{8, 8, w, 26}, 6, Color(0, 0, 0, 170));
        st.color = Color(180, 255, 140);
        r.drawText(buf, 16, 13, st);
    }
}

int App::run() {
    const double target = 1.0 / 60.0;
    double last = platform_.timeSeconds();
    while (!quit_) {
        double start = platform_.timeSeconds();
        if (!platform_.pumpEvents()) break;
        RawPadState raw = platform_.pollInput();
        float dt = float(start - last);
        last = start;
        frame(raw, dt);
        platform_.waitForNextFrame(start, target);
    }
    shutdown();
    return 0;
}

}  // namespace nx
