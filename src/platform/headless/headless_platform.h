// Neutro X - plataforma "sem cabeca" para testes automatizados.
// Sem janela e sem dispositivo de audio; entrada e relogio controlados pelo teste.
#pragma once
#include <chrono>
#include <string>
#include <vector>
#include "audio/audio.h"
#include "graphics/soft_renderer.h"
#include "platform/platform.h"

namespace nx {

class HeadlessPlatform : public IPlatform {
public:
    HeadlessPlatform(int w, int h, const std::string& dataDir)
        : renderer_(w, h), audio_(mixer_), dataDir_(dataDir) {}

    const char* name() const override { return "headless"; }
    bool pumpEvents() override { return true; }
    RawPadState pollInput() override { return input_; }
    IRenderer& renderer() override { return renderer_; }
    void present() override {
        ++presented_;
        // Consome audio como um dispositivo real faria (exercita o mixer).
        int16_t buf[2 * 735];
        mixer_.mix(buf, 735);
    }
    IAudio& audio() override { return audio_; }
    // Relogio simulado (padrao) ou real (medicao de tempo de quadro nos testes).
    double timeSeconds() override {
        if (!realClock_) return clock_;
        using namespace std::chrono;
        return duration<double>(steady_clock::now().time_since_epoch()).count();
    }
    void setRealClock(bool b) { realClock_ = b; }
    void waitForNextFrame(double, double) override {}
    std::string dataPath() override { return dataDir_; }
    void localTime(int& h, int& m, int& d, int& mo, int& y) override { h = 12; m = 34; d = 11; mo = 11; y = 2026; }

    std::vector<StorageRoot> storageRoots() override { return roots_; }
    bool canLaunchTitles() const override { return launchable_; }
    std::string systemDashboardPath() override { return dashPath_; }
    AudioDeviceStatus audioStatus() override {
        AudioDeviceStatus s;
        s.present = s.open = true;
        s.detail = "headless (mixer consumido a cada quadro)";
        s.framesPlayed = uint64_t(presented_);
        return s;
    }
    UserProfile userProfile() override { return profile_; }

    // Controle do teste
    void setRoots(std::vector<StorageRoot> r) { roots_ = std::move(r); }
    void setLaunchable(bool b, const std::string& dash = "") { launchable_ = b; dashPath_ = dash; }
    void setProfile(const UserProfile& p) { profile_ = p; }
    void setInput(const RawPadState& s) { input_ = s; }
    void advance(double dt) { clock_ += dt; }
    void setDataPath(const std::string& p) { dataDir_ = p; }
    SoftRenderer& soft() { return renderer_; }
    Mixer& mixer() { return mixer_; }
    long presented() const { return presented_; }

private:
    SoftRenderer renderer_;
    Mixer mixer_;
    MixerAudio audio_;
    std::string dataDir_;
    RawPadState input_;
    double clock_ = 0;
    long presented_ = 0;
    std::vector<StorageRoot> roots_;
    bool launchable_ = false;
    std::string dashPath_;
    UserProfile profile_;
    bool realClock_ = false;
};

}  // namespace nx
