// Neutro X - configuracoes do usuario.
//
// Salvas SOMENTE no arquivo proprio do Neutro X (neutrox.ini), dentro da
// pasta de dados do aplicativo informada pela plataforma. Nenhuma
// configuracao do sistema/dashboard original e lida ou alterada.
#pragma once
#include <map>
#include <string>

namespace nx {

enum class AnimIntensity : int { Low = 0, Medium = 1, High = 2 };

struct Settings {
    // Dashboard aberto ao iniciar: "" = menu do Neutro X; senao chave do dashboard.
    std::string defaultDashboard;
    int sfxVolume = 80;  // 0..100
    bool animations = true;
    AnimIntensity animIntensity = AnimIntensity::Medium;
    int accentTheme = 0;  // indice em kAccentThemes
    std::string language = "pt-BR";
    bool showFps = false;
    // Sons dos dashboards: 0 = originais (pasta local), 1 = recriados pelo Neutro X.
    int dashboardSounds = 0;
    // Opcoes de cada interface (fundo, avatar, cores...), chave "<dashboard>.<opcao>".
    // Gravadas como linhas "opt.<chave>=<inteiro>".
    std::map<std::string, int> dashOptions;

    int opt(const std::string& key, int def, int lo, int hi) const {
        auto it = dashOptions.find(key);
        if (it == dashOptions.end()) return def;
        return it->second < lo ? lo : (it->second > hi ? hi : it->second);
    }
    void setOpt(const std::string& key, int value) { dashOptions[key] = value; }

    static Settings defaults() { return Settings(); }
    void sanitize();  // corrige valores fora da faixa
    float animSpeed() const;
};

struct AccentTheme {
    const char* name;
    unsigned rgb;
};
extern const AccentTheme kAccentThemes[];
extern const int kAccentThemeCount;

enum class LoadResult { Ok, Missing, Corrupt };

// Serializacao em texto chave=valor.
std::string serializeSettings(const Settings& s);
// Retorna Corrupt se alguma linha invalida for encontrada (valores validos
// continuam aplicados; os demais ficam no padrao).
LoadResult parseSettings(const std::string& text, Settings& out);

// Arquivo: escrita atomica (arquivo temporario + renomeacao).
LoadResult loadSettingsFile(const std::string& path, Settings& out);
bool saveSettingsFile(const std::string& path, const Settings& s);

}  // namespace nx
