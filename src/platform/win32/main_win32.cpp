// Neutro X - plataforma de pre-visualizacao no Windows.
//
// Janela Win32 + GDI (StretchDIBits) para video, XInput para o controle do
// Xbox 360 (com teclado como reserva), waveOut para audio.
// Esta camada NAO roda no console; ela existe para desenvolver e testar a
// interface no PC. Ver docs/TOOLCHAIN.md para a porta ao Xbox 360.
//
// Opcoes de linha de comando:
//   --res LxA          resolucao interna (padrao 1280x720)
//   --data PASTA       pasta de dados (padrao %LOCALAPPDATA%\NeutroX)
//   --frames N         encerra apos N quadros (teste automatizado)
//   --screenshot ARQ   grava o ultimo quadro em PNG ao encerrar
//   --mute             sem audio
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mmsystem.h>
#include <xinput.h>
#include <shellapi.h>
#include <shlobj.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>

#include "core/app.h"
#include "core/log.h"
#include "core/png_writer.h"
#include "graphics/soft_renderer.h"
#include "platform/platform.h"

using namespace nx;

namespace {

std::wstring widen(const std::string& s) {
    if (s.empty()) return std::wstring();
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(size_t(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], n);
    if (!w.empty() && w.back() == L'\0') w.pop_back();
    return w;
}

std::string narrow(const std::wstring& w) {
    if (w.empty()) return std::string();
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string s(size_t(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, &s[0], n, nullptr, nullptr);
    if (!s.empty() && s.back() == '\0') s.pop_back();
    return s;
}

// ---------------------------------------------------------------- XInput
typedef DWORD(WINAPI* XInputGetStateFn)(DWORD, XINPUT_STATE*);

class XInputLoader {
public:
    XInputLoader() {
        const wchar_t* dlls[] = {L"xinput1_4.dll", L"xinput1_3.dll", L"xinput9_1_0.dll"};
        for (const wchar_t* d : dlls) {
            lib_ = LoadLibraryW(d);
            if (lib_) {
                get_ = reinterpret_cast<XInputGetStateFn>(reinterpret_cast<void*>(GetProcAddress(lib_, "XInputGetState")));
                if (get_) { logInfo("XInput carregado (%s)", narrow(d).c_str()); break; }
                FreeLibrary(lib_);
                lib_ = nullptr;
            }
        }
        if (!get_) logWarn("XInput indisponivel; apenas teclado");
    }
    ~XInputLoader() { if (lib_) FreeLibrary(lib_); }
    XInputGetStateFn get() const { return get_; }

private:
    HMODULE lib_ = nullptr;
    XInputGetStateFn get_ = nullptr;
};

// ----------------------------------------------------------------- Audio
class WaveOutAudio {
public:
    explicit WaveOutAudio(Mixer& m) : mixer_(m) {}
    ~WaveOutAudio() { stop(); }

    bool start() {
        WAVEFORMATEX fmt = {};
        fmt.wFormatTag = WAVE_FORMAT_PCM;
        fmt.nChannels = 2;
        fmt.nSamplesPerSec = kAudioRate;
        fmt.wBitsPerSample = 16;
        fmt.nBlockAlign = 4;
        fmt.nAvgBytesPerSec = kAudioRate * 4;
        event_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (waveOutOpen(&wo_, WAVE_MAPPER, &fmt, DWORD_PTR(event_), 0, CALLBACK_EVENT) != MMSYSERR_NOERROR) {
            logWarn("waveOutOpen falhou; audio desativado");
            CloseHandle(event_);
            event_ = nullptr;
            return false;
        }
        for (int i = 0; i < kBuffers; ++i) {
            std::memset(&hdr_[i], 0, sizeof(WAVEHDR));
            hdr_[i].lpData = reinterpret_cast<LPSTR>(data_[i]);
            hdr_[i].dwBufferLength = kFrames * 4;
            waveOutPrepareHeader(wo_, &hdr_[i], sizeof(WAVEHDR));
            hdr_[i].dwFlags |= WHDR_DONE;
        }
        running_ = true;
        thread_ = std::thread([this]() { loop(); });
        return true;
    }

    void stop() {
        if (!running_) return;
        running_ = false;
        SetEvent(event_);
        if (thread_.joinable()) thread_.join();
        waveOutReset(wo_);
        for (int i = 0; i < kBuffers; ++i) waveOutUnprepareHeader(wo_, &hdr_[i], sizeof(WAVEHDR));
        waveOutClose(wo_);
        CloseHandle(event_);
    }

private:
    void loop() {
        while (running_) {
            for (int i = 0; i < kBuffers; ++i) {
                if (hdr_[i].dwFlags & WHDR_DONE) {
                    hdr_[i].dwFlags &= ~DWORD(WHDR_DONE);
                    mixer_.mix(data_[i], kFrames);
                    waveOutWrite(wo_, &hdr_[i], sizeof(WAVEHDR));
                }
            }
            WaitForSingleObject(event_, 50);
        }
    }

    static constexpr int kBuffers = 3;
    static constexpr int kFrames = 1024;  // ~23 ms por buffer
    Mixer& mixer_;
    HWAVEOUT wo_ = nullptr;
    HANDLE event_ = nullptr;
    WAVEHDR hdr_[kBuffers];
    int16_t data_[kBuffers][kFrames * 2];
    std::atomic<bool> running_{false};
    std::thread thread_;
};

// ------------------------------------------------------------- Plataforma
class Win32Platform : public IPlatform {
public:
    Win32Platform(int w, int h, const std::string& dataDir) : renderer_(w, h), audio_(mixer_), dataDir_(dataDir) {
        LARGE_INTEGER f;
        QueryPerformanceFrequency(&f);
        freq_ = double(f.QuadPart);
        timeBeginPeriod(1);
    }
    ~Win32Platform() override {
        if (wave_) wave_->stop();
        delete wave_;
        timeEndPeriod(1);
    }

    bool createWindow(HINSTANCE inst) {
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = &Win32Platform::wndProc;
        wc.hInstance = inst;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
        wc.lpszClassName = L"NeutroXWindow";
        wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
        RegisterClassExW(&wc);
        RECT rc = {0, 0, 1280, 720};
        AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
        hwnd_ = CreateWindowExW(0, wc.lpszClassName, L"Neutro X — Xbox 360 Dashboard Experience (pré-visualização PC)",
                                WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top,
                                nullptr, nullptr, inst, this);
        if (!hwnd_) return false;
        SetWindowLongPtrW(hwnd_, GWLP_USERDATA, LONG_PTR(this));
        ShowWindow(hwnd_, SW_SHOW);
        return true;
    }

    void startAudio(bool mute) {
        if (mute) return;
        wave_ = new WaveOutAudio(mixer_);
        if (!wave_->start()) { delete wave_; wave_ = nullptr; }
    }

    const char* name() const override { return "win32"; }

    bool pumpEvents() override {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) closed_ = true;
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        return !closed_;
    }

    RawPadState pollInput() override {
        RawPadState s;
        // Controle: verifica slots desconectados no maximo 1x por segundo
        // (XInputGetState em slot vazio e lento).
        double now = timeSeconds();
        XInputGetStateFn get = xinput_.get();
        if (get) {
            for (DWORD i = 0; i < 4 && !s.connected; ++i) {
                if (!padConnected_[i] && now < nextProbe_[i]) continue;
                XINPUT_STATE st = {};
                if (get(i, &st) == ERROR_SUCCESS) {
                    padConnected_[i] = true;
                    s.connected = true;
                    const XINPUT_GAMEPAD& g = st.Gamepad;
                    auto map = [&](WORD xb, Button b) { if (g.wButtons & xb) s.buttons |= bit(b); };
                    map(XINPUT_GAMEPAD_DPAD_UP, Button::Up);
                    map(XINPUT_GAMEPAD_DPAD_DOWN, Button::Down);
                    map(XINPUT_GAMEPAD_DPAD_LEFT, Button::Left);
                    map(XINPUT_GAMEPAD_DPAD_RIGHT, Button::Right);
                    map(XINPUT_GAMEPAD_A, Button::A);
                    map(XINPUT_GAMEPAD_B, Button::B);
                    map(XINPUT_GAMEPAD_X, Button::X);
                    map(XINPUT_GAMEPAD_Y, Button::Y);
                    map(XINPUT_GAMEPAD_LEFT_SHOULDER, Button::LB);
                    map(XINPUT_GAMEPAD_RIGHT_SHOULDER, Button::RB);
                    map(XINPUT_GAMEPAD_START, Button::Start);
                    map(XINPUT_GAMEPAD_BACK, Button::Back);
                    auto axis = [](SHORT v) {
                        const float dz = 7849.0f;  // zona morta padrao do analogico esquerdo
                        float f = float(v);
                        if (f > -dz && f < dz) return 0.0f;
                        return f > 0 ? (f - dz) / (32767.0f - dz) : (f + dz) / (32768.0f - dz);
                    };
                    s.lx = axis(g.sThumbLX);
                    s.ly = axis(g.sThumbLY);
                    s.lt = g.bLeftTrigger / 255.0f;
                    s.rt = g.bRightTrigger / 255.0f;
                } else {
                    padConnected_[i] = false;
                    nextProbe_[i] = now + 1.0;
                }
            }
        }
        // Teclado (somente com a janela em foco).
        if (GetForegroundWindow() == hwnd_) {
            uint32_t kb = 0;
            auto key = [&](int vk, Button b) { if (keys_[vk & 0xFF]) kb |= bit(b); };
            key(VK_UP, Button::Up); key(VK_DOWN, Button::Down); key(VK_LEFT, Button::Left); key(VK_RIGHT, Button::Right);
            key('W', Button::Up); key('S', Button::Down); key('A', Button::Left); key('D', Button::Right);
            key(VK_RETURN, Button::A); key(VK_SPACE, Button::A);
            key(VK_ESCAPE, Button::B); key(VK_BACK, Button::B);
            key('X', Button::X); key('Y', Button::Y);
            key('Q', Button::LB); key('E', Button::RB);
            key('Z', Button::LT); key('C', Button::RT);
            key(VK_TAB, Button::Start); key(VK_F1, Button::Back);
            if (kb) { s.buttons |= kb; s.keyboardActive = true; }
        }
        return s;
    }

    IRenderer& renderer() override { return renderer_; }
    IAudio& audio() override { return audio_; }

    void present() override {
        if (!hwnd_) return;
        RECT rc;
        GetClientRect(hwnd_, &rc);
        int cw = rc.right - rc.left, ch = rc.bottom - rc.top;
        if (cw <= 0 || ch <= 0) return;
        int fw = renderer_.pixelWidth(), fh = renderer_.pixelHeight();
        // Mantem a proporcao (barras pretas se necessario).
        double s = std::min(double(cw) / fw, double(ch) / fh);
        int dw = int(fw * s), dh = int(fh * s);
        int dx = (cw - dw) / 2, dy = (ch - dh) / 2;
        HDC dc = GetDC(hwnd_);
        BITMAPINFO bi = {};
        bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bi.bmiHeader.biWidth = fw;
        bi.bmiHeader.biHeight = -fh;  // de cima para baixo
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 32;
        bi.bmiHeader.biCompression = BI_RGB;
        SetStretchBltMode(dc, s < 1.0 ? HALFTONE : COLORONCOLOR);
        if (dx > 0) { PatBlt(dc, 0, 0, dx, ch, BLACKNESS); PatBlt(dc, dx + dw, 0, cw - dx - dw, ch, BLACKNESS); }
        if (dy > 0) { PatBlt(dc, 0, 0, cw, dy, BLACKNESS); PatBlt(dc, 0, dy + dh, cw, ch - dy - dh, BLACKNESS); }
        StretchDIBits(dc, dx, dy, dw, dh, 0, 0, fw, fh, renderer_.pixels(), &bi, DIB_RGB_COLORS, SRCCOPY);
        ReleaseDC(hwnd_, dc);
        if (screenshotRequested_) {
            screenshotRequested_ = false;
            SYSTEMTIME t;
            GetLocalTime(&t);
            char name[64];
            std::snprintf(name, sizeof(name), "captura_%04d%02d%02d_%02d%02d%02d.png", t.wYear, t.wMonth, t.wDay, t.wHour,
                          t.wMinute, t.wSecond);
            std::string path = dataDir_ + name;
            if (writePng(path, fw, fh, renderer_.pixels())) logInfo("Captura salva: %s", path.c_str());
        }
    }

    double timeSeconds() override {
        LARGE_INTEGER c;
        QueryPerformanceCounter(&c);
        return double(c.QuadPart) / freq_;
    }

    void waitForNextFrame(double frameStart, double target) override {
        for (;;) {
            double left = target - (timeSeconds() - frameStart);
            if (left <= 0) break;
            if (left > 0.002) Sleep(DWORD((left - 0.0015) * 1000.0));
            else Sleep(0);
        }
    }

    std::string dataPath() override { return dataDir_; }

    void localTime(int& h, int& m, int& d, int& mo, int& y) override {
        SYSTEMTIME t;
        GetLocalTime(&t);
        h = t.wHour; m = t.wMinute; d = t.wDay; mo = t.wMonth; y = t.wYear;
    }

    SoftRenderer& soft() { return renderer_; }

private:
    static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
        auto* self = reinterpret_cast<Win32Platform*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        switch (msg) {
            case WM_CLOSE:
                if (self) self->closed_ = true;
                return 0;
            case WM_KEYDOWN:
            case WM_SYSKEYDOWN:
                if (self) {
                    self->keys_[wp & 0xFF] = true;
                    if (wp == VK_F12) self->screenshotRequested_ = true;
                }
                if (wp == VK_F10) return 0;  // evita menu do sistema
                break;
            case WM_KEYUP:
            case WM_SYSKEYUP:
                if (self) self->keys_[wp & 0xFF] = false;
                break;
            case WM_KILLFOCUS:
                if (self) std::memset(self->keys_, 0, sizeof(self->keys_));
                break;
            case WM_ERASEBKGND:
                return 1;
        }
        return DefWindowProcW(hwnd, msg, wp, lp);
    }

    SoftRenderer renderer_;
    Mixer mixer_;
    MixerAudio audio_;
    WaveOutAudio* wave_ = nullptr;
    XInputLoader xinput_;
    std::string dataDir_;
    HWND hwnd_ = nullptr;
    double freq_ = 1;
    bool closed_ = false;
    bool keys_[256] = {};
    bool padConnected_[4] = {};
    double nextProbe_[4] = {};
    bool screenshotRequested_ = false;
};

std::string defaultDataDir() {
    PWSTR p = nullptr;
    std::string dir;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &p))) {
        dir = narrow(p) + "\\NeutroX\\";
        CoTaskMemFree(p);
    } else {
        dir = ".\\";
    }
    return dir;
}

}  // namespace

int WINAPI WinMain(HINSTANCE inst, HINSTANCE, LPSTR, int) {
    int argc = 0;
    LPWSTR* wargv = CommandLineToArgvW(GetCommandLineW(), &argc);
    int w = 1280, h = 720;
    long maxFrames = -1;
    bool mute = false;
    std::string dataDir = defaultDataDir(), screenshot;
    for (int i = 1; i < argc; ++i) {
        std::string a = narrow(wargv[i]);
        std::string next = i + 1 < argc ? narrow(wargv[i + 1]) : "";
        if (a == "--res" && !next.empty()) { std::sscanf(next.c_str(), "%dx%d", &w, &h); ++i; }
        else if (a == "--data" && !next.empty()) { dataDir = next; if (dataDir.back() != '\\' && dataDir.back() != '/') dataDir += '\\'; ++i; }
        else if (a == "--frames" && !next.empty()) { maxFrames = std::atol(next.c_str()); ++i; }
        else if (a == "--screenshot" && !next.empty()) { screenshot = next; ++i; }
        else if (a == "--mute") mute = true;
    }
    LocalFree(wargv);
    if (w < 320 || h < 180 || w > 7680 || h > 4320) { w = 1280; h = 720; }

    CreateDirectoryW(widen(dataDir).c_str(), nullptr);
    logSetFile(dataDir + "neutrox.log");

    Win32Platform platform(w, h, dataDir);
    if (!platform.createWindow(inst)) {
        MessageBoxW(nullptr, L"Não foi possível criar a janela.", L"Neutro X", MB_ICONERROR);
        return 1;
    }
    platform.startAudio(mute);

    App app(platform);
    if (!app.init()) return 1;

    const double target = 1.0 / 60.0;
    double last = platform.timeSeconds();
    long frames = 0;
    while (!app.quitRequested()) {
        double start = platform.timeSeconds();
        if (!platform.pumpEvents()) break;
        RawPadState raw = platform.pollInput();
        float dt = float(start - last);
        last = start;
        app.frame(raw, dt);
        ++frames;
        if (maxFrames > 0 && frames >= maxFrames) break;
        platform.waitForNextFrame(start, target);
    }
    if (!screenshot.empty()) writePng(screenshot, platform.soft().pixelWidth(), platform.soft().pixelHeight(), platform.soft().pixels());
    const FrameStats& st = app.stats();
    logInfo("Quadros: %llu, pior quadro: %.2f ms", static_cast<unsigned long long>(st.frames), st.worstFrameMs);
    app.shutdown();
    logSetFile("");
    return 0;
}
