// Neutro X - plataforma Xbox 360 (RGH/JTAG), sobre o SDK homebrew xsdk (MIT).
//
// Video: o SoftRenderer desenha 1280x720 na memoria da CPU; a cada quadro o
//   quadro e enviado para uma textura linear A8R8G8B8 e um quad de tela cheia
//   (shaders blit.vert/blit.frag) o copia para o eDRAM, que e resolvido no
//   front buffer e mostrado no vblank (xsdk_present_*).
// Controle: XamInputGetState (controles 1 a 4; o primeiro conectado manda).
// Audio: o Mixer (44,1 kHz estereo) e reamostrado para 48 kHz e entregue ao
//   driver de audio do console por uma thread propria (xsdk_audio_*).
// Arquivos: tudo do Neutro X fica em game:\ (a pasta de onde foi iniciado).
// Sair (ver docs/SAIDA_E_INICIALIZACAO.md): o Neutro X libera audio, GPU e
//   tela e entao:
//   - "Voltar ao Aurora"/jogos: xsdk_launch(caminho) -> exit -> XamLoaderLaunchTitle
//     (troca de aplicativo; nao reinicia o console);
//   - "Voltar ao Xbox Dashboard": o mesmo com \SystemRoot\dash.xex;
//   - padrao: main retorna -> exit -> XamLoaderTerminateTitle (dashboard do sistema,
//     que o DashLaunch pode redirecionar).
//   Nenhuma funcao de reinicializacao (HalReturnToFirmware etc.) e usada.
//
// Nada aqui grava na NAND, altera o sistema ou mexe na inicializacao.
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <thread>
#include <vector>

extern "C" {
#include "xsdk/audio.h"
#include "xsdk/gpu.h"
#include "xsdk/kernel.h"
#include "xsdk/log.h"
#include "xsdk/present.h"
#include "xsdk/shader.h"
#include "xsdk/splash.h"
#include "xsdk/texture.h"
#include "xsdk/thread.h"
#include "xsdk/xam.h"
}
#include <sys/stat.h>
#include <pthread.h>
#include "NeutroX_shaders.h"

#include "core/app.h"
#include "core/fsutil.h"
#include "core/log.h"
#include "graphics/soft_renderer.h"
#include "platform/platform.h"

using namespace nx;

namespace {

constexpr uint32_t kBuffers = 3;
constexpr uint32_t kBeginTimeoutMs = 5000;
constexpr uint32_t kFenceTimeoutMs = 1000;

struct QuadVertex {
    float x, y, u, v;
};

class Xbox360Platform : public IPlatform {
public:
    Xbox360Platform() : renderer_(1280, 720), audio_(mixer_) {}
    ~Xbox360Platform() override { shutdown(); }

    bool init() {
        NTSTATUS st = xsdk_splash_show(1200);
        if (!NT_SUCCESS(st)) xsdk_log_write("neutrox: xsdk_splash_show 0x%08X", unsigned(st));
        st = xsdk_gpu_open();
        if (!NT_SUCCESS(st)) { xsdk_log_write("neutrox: xsdk_gpu_open falhou: 0x%08X", unsigned(st)); return false; }
        gpuOpen_ = true;
        uint32_t w = 0, h = 0;
        st = xsdk_present_open(XSDK_DISPLAY_720P, kBuffers, 1, &w, &h);
        if (!NT_SUCCESS(st)) { xsdk_log_write("neutrox: xsdk_present_open falhou: 0x%08X", unsigned(st)); return false; }
        presentOpen_ = true;
        width_ = w;
        height_ = h;
        if (w != 1280 || h != 720) renderer_.resize(int(w), int(h));  // ex.: 640x480 em saida entrelacada
        xsdk_log_write("neutrox: tela %ux%u", unsigned(w), unsigned(h));
        // Rasterizacao em 6 faixas: a thread principal (hardware 0) e uma auxiliar em
        // cada thread de hardware 1..5 do Xenon (docs/GUIDE.md do xsdk, "Threads").
        renderer_.setThreads(6, [](int i) {
            const NTSTATUS ms = xsdk_thread_move(uint32_t(i));
            if (!NT_SUCCESS(ms)) xsdk_log_write("neutrox: xsdk_thread_move(%d) 0x%08X", i, unsigned(ms));
        });

        st = xsdk_texture_create(&frameTex_, XSDK_FORMAT_A8R8G8B8, w, h, 1, false);
        if (!NT_SUCCESS(st)) { xsdk_log_write("neutrox: xsdk_texture_create falhou: 0x%08X", unsigned(st)); return false; }
        texOpen_ = true;
        const XsdkSampler sampler = {.mag_filter = XSDK_FILTER_POINT,
                                     .min_filter = XSDK_FILTER_POINT,
                                     .mip_filter = XSDK_FILTER_POINT,
                                     .address_u = XSDK_ADDRESS_CLAMP,
                                     .address_v = XSDK_ADDRESS_CLAMP};
        xsdk_texture_sampler_state(&samplerState_, &sampler);

        st = xsdk_shader_load(&vs_, blit_vert.container, blit_vert.container_bytes);
        if (NT_SUCCESS(st)) st = xsdk_shader_load(&ps_, blit_frag.container, blit_frag.container_bytes);
        static const XsdkVertexElement layout[] = {
            {.usage = XSDK_USAGE_POSITION, .format = XSDK_VERTEX_FLOAT2, .offset_bytes = 0},
            {.usage = XSDK_USAGE_TEXCOORD, .format = XSDK_VERTEX_FLOAT2, .offset_bytes = 8},
        };
        const uint32_t stride = sizeof(QuadVertex);
        if (NT_SUCCESS(st)) st = xsdk_shader_set_vertex_layout(&vs_, layout, 2, &stride);
        if (!NT_SUCCESS(st)) { xsdk_log_write("neutrox: shaders falharam: 0x%08X", unsigned(st)); return false; }
        shadersLoaded_ = true;

        // Quad de tela cheia; bordas meio pixel acima/esquerda (centros de pixel do D3D9),
        // para cada texel cair exatamente num pixel com amostragem por ponto.
        const float hx = 1.0f / float(w), hy = 1.0f / float(h);
        const QuadVertex quad[6] = {
            {-1 - hx, 1 + hy, 0, 0}, {1 - hx, 1 + hy, 1, 0}, {-1 - hx, -1 + hy, 0, 1},
            {1 - hx, 1 + hy, 1, 0},  {1 - hx, -1 + hy, 1, 1}, {-1 - hx, -1 + hy, 0, 1},
        };
        vertices_ = xsdk_gpu_alloc(sizeof quad);
        if (!vertices_) { xsdk_log_write("neutrox: xsdk_gpu_alloc falhou"); return false; }
        for (uint32_t i = 0; i < sizeof quad / 4; ++i) {
            uint32_t word;
            std::memcpy(&word, reinterpret_cast<const uint8_t*>(quad) + i * 4, 4);
            vertices_[i] = word;  // escritas volateis de 32 bits (PITFALLS do xsdk)
        }
        xsdk_gpu_invalidate(xsdk_gpu_address(vertices_), sizeof quad);
        verticesBytes_ = sizeof quad;

        startAudio();
        return true;
    }

    void shutdown() {
        stopAudio();
        if (lastFence_) xsdk_gpu_wait(lastFence_, kFenceTimeoutMs);
        if (presentOpen_) { xsdk_present_close(); presentOpen_ = false; }
        if (vertices_) { xsdk_gpu_free(vertices_); vertices_ = nullptr; }
        if (texOpen_) { xsdk_texture_free(&frameTex_); texOpen_ = false; }
        if (shadersLoaded_) { xsdk_shader_free(&ps_); xsdk_shader_free(&vs_); shadersLoaded_ = false; }
        if (gpuOpen_) {
            NTSTATUS st = xsdk_gpu_close();
            if (!NT_SUCCESS(st)) xsdk_log_write("neutrox: xsdk_gpu_close 0x%08X", unsigned(st));
            gpuOpen_ = false;
        }
    }

    const char* name() const override { return "xbox360"; }
    bool pumpEvents() override { return true; }

    RawPadState pollInput() override {
        RawPadState s;
        for (uint32_t pad = 0; pad < 4; ++pad) {
            INPUT_STATE in;
            std::memset(&in, 0, sizeof in);
            if (XamInputGetState(pad, 0, &in) != ERROR_SUCCESS) continue;
            s.connected = true;
            const INPUT_GAMEPAD& g = in.gamepad;
            auto map = [&](uint16_t bitMask, Button b) { if (g.buttons & bitMask) s.buttons |= bit(b); };
            map(INPUT_GAMEPAD_DPAD_UP, Button::Up);
            map(INPUT_GAMEPAD_DPAD_DOWN, Button::Down);
            map(INPUT_GAMEPAD_DPAD_LEFT, Button::Left);
            map(INPUT_GAMEPAD_DPAD_RIGHT, Button::Right);
            map(INPUT_GAMEPAD_A, Button::A);
            map(INPUT_GAMEPAD_B, Button::B);
            map(INPUT_GAMEPAD_X, Button::X);
            map(INPUT_GAMEPAD_Y, Button::Y);
            map(INPUT_GAMEPAD_LEFT_SHOULDER, Button::LB);
            map(INPUT_GAMEPAD_RIGHT_SHOULDER, Button::RB);
            map(INPUT_GAMEPAD_START, Button::Start);
            map(INPUT_GAMEPAD_BACK, Button::Back);
            auto axis = [](int16_t v) {
                const float dz = 7849.0f;  // zona morta padrao do analogico esquerdo
                float f = float(v);
                if (f > -dz && f < dz) return 0.0f;
                return f > 0 ? (f - dz) / (32767.0f - dz) : (f + dz) / (32768.0f - dz);
            };
            s.lx = axis(g.thumb_lx);
            s.ly = axis(g.thumb_ly);
            s.lt = g.left_trigger / 255.0f;
            s.rt = g.right_trigger / 255.0f;
            break;  // o primeiro controle conectado manda
        }
        return s;
    }

    IRenderer& renderer() override { return renderer_; }
    IAudio& audio() override { return audio_; }

    void present() override {
        // O quadro anterior precisa ter terminado de ler a textura antes de reescreve-la.
        if (lastFence_) xsdk_gpu_wait(lastFence_, kFenceTimeoutMs);
        xsdk_texture_upload(&frameTex_, 0, renderer_.pixels(), width_ * 4);
        xsdk_gpu_invalidate(frameTex_.address, frameTex_.size_bytes);
        uint32_t address = 0;
        NTSTATUS st = xsdk_present_begin(&address, kBeginTimeoutMs);
        if (!NT_SUCCESS(st)) {
            if (++presentErrors_ <= 5) xsdk_log_write("neutrox: xsdk_present_begin falhou: 0x%08X", unsigned(st));
            return;
        }
        xsdk_gpu_set_target(width_, height_);
        xsdk_gpu_set_shaders(&vs_, &ps_);
        // Constantes literais que o compilador de shaders reservou (o resolve as apaga a cada quadro).
        xsdk_built_shader_set_literals(&blit_vert);
        xsdk_built_shader_set_literals(&blit_frag);
        xsdk_gpu_set_texture(BLIT_FRAG_SAMPLER_0_0, &frameTex_, &samplerState_);
        xsdk_gpu_set_vertices(0, xsdk_gpu_address(vertices_), verticesBytes_);
        xsdk_gpu_draw(XSDK_PRIMITIVE_TRIANGLE_LIST, 6);
        xsdk_gpu_resolve(address, width_, height_, 0xFF000000u);
        lastFence_ = xsdk_gpu_fence();
        xsdk_present_end();
    }

    double timeSeconds() override {
        using namespace std::chrono;
        return duration<double>(steady_clock::now().time_since_epoch()).count();
    }
    // xsdk_present_begin ja espera o vblank: nada a fazer aqui.
    void waitForNextFrame(double, double) override {}

    std::string dataPath() override { return "game:/"; }

    std::vector<StorageRoot> storageRoots() override {
        // Unidades nomeadas pelo xsdk (docs/GUIDE.md, "Files and paths"); so as presentes.
        static const struct { const char* path; const char* label; const char* name; } kDrives[] = {
            {"hdd:/", "Disco rígido (Hdd1:)", "Hdd1"}, {"usb0:/", "USB 0 (Usb0:)", "Usb0"},
            {"usb1:/", "USB 1 (Usb1:)", "Usb1"},       {"usb2:/", "USB 2 (Usb2:)", "Usb2"},
            {"mu0:/", "Unidade de memória 0 (Mu0:)", "Mu0"}, {"mu1:/", "Unidade de memória 1 (Mu1:)", "Mu1"},
        };
        std::vector<StorageRoot> out;
        for (const auto& d : kDrives) {
            struct stat st;
            if (stat(d.path, &st) == 0 && S_ISDIR(st.st_mode)) out.push_back({d.path, d.label, d.name});
        }
        return out;
    }
    bool canLaunchTitles() const override { return true; }
    // O kernel liga \SystemRoot a pasta do sistema (flash ou $SystemUpdate); o xsdk passa
    // caminhos iniciados por '/' ao kernel como estao. Somente leitura (stat) e inicializacao.
    std::string systemDashboardPath() override { return "/SystemRoot/dash.xex"; }

    AudioDeviceStatus audioStatus() override {
        AudioDeviceStatus s;
        s.present = true;
        s.open = audioOpen_;
        s.detail = audioDetail_;
        if (audioOpen_) {
            XsdkAudioStats st;
            std::memset(&st, 0, sizeof st);
            xsdk_audio_stats(&st);
            s.framesPlayed = st.played;
            s.framesSilent = st.silent;
        }
        return s;
    }

    UserProfile userProfile() override {
        UserProfile p;
        p.supported = true;
        uint32_t index = XSDK_XAM_USER_NONE;
        char tag[XSDK_XAM_GAMERTAG_BYTES];
        std::memset(tag, 0, sizeof tag);
        const uint32_t res = xsdk_xam_signed_in_user(&index, tag);
        if (res == ERROR_SUCCESS && index != XSDK_XAM_USER_NONE) {
            p.signedIn = true;
            p.userIndex = int(index);
            p.gamertag = tag;
            uint64_t xuid = 0;
            if (XamUserGetXUID(index, 1, &xuid) == ERROR_SUCCESS) p.xuid = xuid;
            p.detail = "XamUserGetSigninState / XamUserGetName";
        } else {
            p.detail = res == ERROR_NO_SUCH_USER ? "nenhum perfil conectado" : "XamUserGetName falhou";
        }
        return p;
    }

    void localTime(int& h, int& m, int& d, int& mo, int& y) override {
        std::time_t t = std::time(nullptr);
        std::tm tmv;
        std::memset(&tmv, 0, sizeof tmv);
        if (localtime_r(&t, &tmv)) {
            h = tmv.tm_hour; m = tmv.tm_min; d = tmv.tm_mday; mo = tmv.tm_mon + 1; y = tmv.tm_year + 1900;
        }
    }

private:
    // ---- audio: Mixer (44,1 kHz, estereo int16) -> driver (48 kHz, 6 canais float)
    void startAudio() {
        NTSTATUS st = xsdk_audio_open(0);
        if (!NT_SUCCESS(st)) {
            char b[96];
            std::snprintf(b, sizeof b, "xsdk_audio_open falhou: 0x%08X", unsigned(st));
            audioDetail_ = b;
            xsdk_log_write("neutrox: %s (sem som)", b);
            return;
        }
        XsdkAudioSpeakers spk = XSDK_AUDIO_SPEAKERS_STEREO;
        const bool spkOk = NT_SUCCESS(xsdk_audio_speakers(&spk));
        audioDetail_ = std::string("xsdk_audio aberto, 48 kHz, 6 canais; saida ") +
                       (spkOk ? (spk == XSDK_AUDIO_SPEAKERS_SURROUND ? "5.1" : "estereo") : "desconhecida");
        audioOpen_ = true;
        audioRun_ = true;
        audioThread_ = std::thread([this]() { audioLoop(); });
        xsdk_log_write("neutrox: %s", audioDetail_.c_str());
    }
    void stopAudio() {
        if (!audioOpen_) return;
        // 1) para o alimentador (ele sai em ate 200 ms, no proximo quadro livre ou no tempo limite),
        // 2) so entao fecha o cliente do driver: nenhum quadro fica pela metade.
        audioRun_ = false;
        if (audioThread_.joinable()) audioThread_.join();
        XsdkAudioStats st;
        std::memset(&st, 0, sizeof st);
        xsdk_audio_stats(&st);
        xsdk_log_write("neutrox: audio: %u quadros tocados, %u de silencio por atraso", unsigned(st.played), unsigned(st.silent));
        const NTSTATUS cs = xsdk_audio_close();
        if (!NT_SUCCESS(cs)) xsdk_log_write("neutrox: xsdk_audio_close 0x%08X", unsigned(cs));
        audioOpen_ = false;
    }
    void audioLoop() {
        const double step = double(kAudioRate) / double(XSDK_AUDIO_RATE_HZ);  // amostras de 44,1k por amostra de 48k
        std::vector<int16_t> src(2 * 512);
        double pos = 0;
        int have = 0;  // amostras validas em src (por canal)
        int16_t lastL = 0, lastR = 0;
        while (audioRun_) {
            float* frame = nullptr;
            NTSTATUS st = xsdk_audio_next_frame(200, &frame);
            if (st == STATUS_INVALID_HANDLE) break;
            if (!NT_SUCCESS(st) || !frame) continue;
            float* left = frame + XSDK_AUDIO_FRONT_LEFT * XSDK_AUDIO_FRAME_SAMPLES;
            float* right = frame + XSDK_AUDIO_FRONT_RIGHT * XSDK_AUDIO_FRAME_SAMPLES;
            for (int ch = 0; ch < XSDK_AUDIO_CHANNELS; ++ch)
                std::memset(frame + ch * XSDK_AUDIO_FRAME_SAMPLES, 0, XSDK_AUDIO_FRAME_SAMPLES * sizeof(float));
            for (int i = 0; i < XSDK_AUDIO_FRAME_SAMPLES; ++i) {
                // pos pode ficar em [-1, 0) logo apos trocar de bloco: usa floor (int() truncaria para 0)
                while (int(std::floor(pos)) + 1 >= have) {
                    // guarda a ultima amostra e mistura mais um bloco a 44,1 kHz
                    if (have > 0) { lastL = src[size_t(2 * (have - 1))]; lastR = src[size_t(2 * (have - 1) + 1)]; }
                    pos -= have;
                    mixer_.mix(src.data(), 256);
                    have = 256;
                }
                int p = int(std::floor(pos));
                float f = float(pos - p);
                float l0 = p < 0 ? lastL : src[size_t(2 * p)], r0 = p < 0 ? lastR : src[size_t(2 * p + 1)];
                float l1 = src[size_t(2 * (p + 1))], r1 = src[size_t(2 * (p + 1) + 1)];
                left[i] = (l0 + (l1 - l0) * f) / 32768.0f;
                right[i] = (r0 + (r1 - r0) * f) / 32768.0f;
                pos += step;
            }
            xsdk_audio_queue_frame();
        }
    }

    SoftRenderer renderer_;
    Mixer mixer_;
    MixerAudio audio_;
    uint32_t width_ = 1280, height_ = 720;
    XsdkTexture frameTex_{};
    XsdkSamplerState samplerState_{};
    XsdkShader vs_{}, ps_{};
    volatile uint32_t* vertices_ = nullptr;
    uint32_t verticesBytes_ = 0;
    uint32_t lastFence_ = 0;
    int presentErrors_ = 0;
    bool gpuOpen_ = false, presentOpen_ = false, texOpen_ = false, shadersLoaded_ = false;
    bool audioOpen_ = false;
    std::string audioDetail_ = "audio ainda nao iniciado";
    std::atomic<bool> audioRun_{false};
    std::thread audioThread_;
};

}  // namespace

namespace {

// Roda o Neutro X e devolve o .xex a iniciar depois (vazio = saida padrao).
// Toda a plataforma (audio, GPU, tela) e liberada antes de retornar.
std::string runNeutroX(int& code) {
    std::string launch;
    Xbox360Platform platform;
    if (!platform.init()) {
        xsdk_log_write("neutrox: falha ao iniciar a plataforma");
        code = 1;
        return launch;
    }
    {
        App app(platform);
        if (!app.init()) {
            code = 1;
        } else {
            double last = platform.timeSeconds();
            while (!app.quitRequested()) {
                double now = platform.timeSeconds();
                float dt = float(now - last);
                last = now;
                app.frame(platform.pollInput(), dt);
            }
            const FrameStats& st = app.stats();
            xsdk_log_write("neutrox: %llu quadros, pior quadro %.2f ms, %llu lentos", static_cast<unsigned long long>(st.frames),
                           double(st.worstFrameMs), static_cast<unsigned long long>(st.slowFrames));
            launch = app.launchOnExit();
            xsdk_log_write("neutrox: encerrando o aplicativo");
            app.shutdown();
        }
    }
    xsdk_log_write("neutrox: liberando audio, GPU e tela");
    platform.shutdown();
    return launch;
}

}  // namespace

int main() {
    xsdk_log_write("neutrox: Neutro X %s para Xbox 360 (xsdk)", kNeutroXVersion);
    int code = 0;
    const std::string launch = runNeutroX(code);
    if (!launch.empty()) {
        // Troca de aplicativo: o xsdk encerra o titulo (exit) e o XAM inicia o .xex pedido.
        xsdk_log_write("neutrox: iniciando %s", launch.c_str());
        xsdk_launch(launch.c_str(), nullptr, 0);
        // So volta aqui se falhou antes de encerrar (ex.: arquivo sumiu).
        xsdk_log_write("neutrox: xsdk_launch(%s) falhou: errno %d; saida padrao do sistema", launch.c_str(), errno);
    }
    xsdk_log_write("neutrox: encerrado (saida padrao do sistema)");
    return code;
}
