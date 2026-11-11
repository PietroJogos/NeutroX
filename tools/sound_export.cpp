// Neutro X - exporta os temas sonoros RECRIADOS (criados pelo Neutro X) em WAV,
// organizados em uma pasta por versao. Nao envolve nenhum audio proprietario.
//
// Uso: neutrox_sound_export <pasta_saida>
//   -> <pasta_saida>/<tema>/<efeito>.wav  (ex.: blades_6717/navegar.wav)
#include <cstdio>
#include <filesystem>
#include <string>
#include "audio/audio.h"

int main(int argc, char** argv) {
    using namespace nx;
    std::string out = argc > 1 ? argv[1] : "assets/sounds/recriados";
    if (out.back() != '/' && out.back() != '\\') out += '/';
    int files = 0, failed = 0;
    for (int t = 0; t < kSoundThemeCount; ++t) {
        const SoundThemeInfo& ti = soundThemeInfo(SoundTheme(t));
        std::string dir = out + ti.folder + "/";
        std::filesystem::create_directories(std::filesystem::u8path(dir));
        for (int s = 0; s < kSfxCount; ++s) {
            std::string path = dir + sfxFileName(Sfx(s)) + ".wav";
            if (writeWav(path, synthesize(SoundTheme(t), Sfx(s)))) ++files;
            else { ++failed; std::fprintf(stderr, "falha: %s\n", path.c_str()); }
        }
        std::printf("%-16s %s\n", ti.folder, ti.character);
    }
    std::printf("%d arquivos WAV gravados em %s (%d falhas)\n", files, out.c_str(), failed);
    return failed ? 1 : 0;
}
