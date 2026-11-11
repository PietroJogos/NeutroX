// Neutro X - configuracoes. Ver settings.h.
#include "settings/settings.h"

#include <cstdio>
#include <cstdlib>
#include <sstream>

namespace nx {

const AccentTheme kAccentThemes[] = {
    {"Verde Neutro", 0x5DC21E},
    {"Azul Glaciar", 0x2E9BE0},
    {"Laranja Solar", 0xF08A24},
    {"Roxo Aurora", 0x9A5CD6},
    {"Vermelho Rubi", 0xD9363E},
};
const int kAccentThemeCount = int(sizeof(kAccentThemes) / sizeof(kAccentThemes[0]));

namespace {
const char* const kValidDashboards[] = {"", "blades", "nxe", "kinect", "metro_v1", "metro_v2"};

bool validDashboard(const std::string& k) {
    for (const char* v : kValidDashboards)
        if (k == v) return true;
    return false;
}

std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

bool parseInt(const std::string& v, int& out) {
    if (v.empty()) return false;
    char* end = nullptr;
    long n = std::strtol(v.c_str(), &end, 10);
    if (!end || *end != '\0') return false;
    out = int(n);
    return true;
}

bool parseBool(const std::string& v, bool& out) {
    if (v == "1" || v == "true" || v == "sim") { out = true; return true; }
    if (v == "0" || v == "false" || v == "nao") { out = false; return true; }
    return false;
}
}  // namespace

void Settings::sanitize() {
    if (sfxVolume < 0) sfxVolume = 0;
    if (sfxVolume > 100) sfxVolume = 100;
    if (int(animIntensity) < 0 || int(animIntensity) > 2) animIntensity = AnimIntensity::Medium;
    if (accentTheme < 0 || accentTheme >= kAccentThemeCount) accentTheme = 0;
    if (!validDashboard(defaultDashboard)) defaultDashboard.clear();
    if (dashboardSounds != 0 && dashboardSounds != 1) dashboardSounds = 0;
    language = "pt-BR";  // unico idioma disponivel nesta versao
}

float Settings::animSpeed() const {
    switch (animIntensity) {
        case AnimIntensity::Low: return 0.7f;
        case AnimIntensity::High: return 1.4f;
        default: return 1.0f;
    }
}

std::string serializeSettings(const Settings& s) {
    std::ostringstream o;
    o << "# Neutro X - configuracoes do usuario\n";
    o << "# Arquivo proprio do Neutro X. Pode ser apagado com seguranca para restaurar os padroes.\n";
    o << "version=1\n";
    o << "default_dashboard=" << s.defaultDashboard << "\n";
    o << "sfx_volume=" << s.sfxVolume << "\n";
    o << "animations=" << (s.animations ? 1 : 0) << "\n";
    o << "anim_intensity=" << int(s.animIntensity) << "\n";
    o << "accent_theme=" << s.accentTheme << "\n";
    o << "language=" << s.language << "\n";
    o << "show_fps=" << (s.showFps ? 1 : 0) << "\n";
    o << "dashboard_sounds=" << (s.dashboardSounds == 1 ? "recriados" : "originais") << "\n";
    return o.str();
}

LoadResult parseSettings(const std::string& text, Settings& out) {
    Settings s = Settings::defaults();
    bool bad = false;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;
        size_t eq = line.find('=');
        if (eq == std::string::npos) { bad = true; continue; }
        std::string k = trim(line.substr(0, eq)), v = trim(line.substr(eq + 1));
        int n = 0;
        bool b = false;
        if (k == "version") { if (!parseInt(v, n)) bad = true; }
        else if (k == "default_dashboard") { if (validDashboard(v)) s.defaultDashboard = v; else bad = true; }
        else if (k == "sfx_volume") { if (parseInt(v, n) && n >= 0 && n <= 100) s.sfxVolume = n; else bad = true; }
        else if (k == "animations") { if (parseBool(v, b)) s.animations = b; else bad = true; }
        else if (k == "anim_intensity") { if (parseInt(v, n) && n >= 0 && n <= 2) s.animIntensity = AnimIntensity(n); else bad = true; }
        else if (k == "accent_theme") { if (parseInt(v, n) && n >= 0 && n < kAccentThemeCount) s.accentTheme = n; else bad = true; }
        else if (k == "language") { if (v != "pt-BR") bad = true; }
        else if (k == "show_fps") { if (parseBool(v, b)) s.showFps = b; else bad = true; }
        else if (k == "dashboard_sounds") { if (v == "originais") s.dashboardSounds = 0; else if (v == "recriados") s.dashboardSounds = 1; else bad = true; }
        // chaves desconhecidas sao ignoradas (compatibilidade futura)
    }
    s.sanitize();
    out = s;
    return bad ? LoadResult::Corrupt : LoadResult::Ok;
}

LoadResult loadSettingsFile(const std::string& path, Settings& out) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) { out = Settings::defaults(); return LoadResult::Missing; }
    std::string text;
    char buf[1024];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) {
        text.append(buf, n);
        if (text.size() > 64 * 1024) break;  // arquivo absurdo: trata como corrompido
    }
    std::fclose(f);
    if (text.size() > 64 * 1024) { out = Settings::defaults(); return LoadResult::Corrupt; }
    for (unsigned char c : text)
        if (c == 0) { out = Settings::defaults(); return LoadResult::Corrupt; }  // binario
    return parseSettings(text, out);
}

bool saveSettingsFile(const std::string& path, const Settings& s) {
    std::string tmp = path + ".tmp";
    FILE* f = std::fopen(tmp.c_str(), "wb");
    if (!f) return false;
    std::string data = serializeSettings(s);
    bool ok = std::fwrite(data.data(), 1, data.size(), f) == data.size();
    ok = (std::fflush(f) == 0) && ok;
    std::fclose(f);
    if (!ok) { std::remove(tmp.c_str()); return false; }
    std::remove(path.c_str());  // rename nao sobrescreve em todas as plataformas
    if (std::rename(tmp.c_str(), path.c_str()) != 0) { std::remove(tmp.c_str()); return false; }
    return true;
}

}  // namespace nx
