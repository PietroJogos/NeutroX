// Neutro X - destinos de saida. Ver launcher.h.
#include "system/launcher.h"

#include "core/fsutil.h"

namespace nx {

namespace {

std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n\"");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n\"");
    return s.substr(a, b - a + 1);
}

// "Hdd1" e "Hdd" sao a mesma unidade; "Usb" e "Usb0" tambem.
std::string canonicalDrive(std::string d) {
    d = toLowerAscii(d);
    if (d == "hdd" || d == "hdd1" || d == "hdd:") return "hdd1";
    if (d == "usb") return "usb0";
    if (d == "mu") return "mu0";
    return d;
}

}  // namespace

std::string consolePathToLocal(const std::string& consolePath, const std::vector<StorageRoot>& roots) {
    std::string s = trim(consolePath);
    size_t colon = s.find(':');
    if (colon == std::string::npos || colon == 0) return "";
    const std::string drive = canonicalDrive(s.substr(0, colon));
    std::string rest = s.substr(colon + 1);
    for (char& c : rest)
        if (c == '\\') c = '/';
    for (const StorageRoot& r : roots) {
        if (r.consoleName.empty() || canonicalDrive(r.consoleName) != drive) continue;
        return normalizePath(joinPath(r.path, rest));
    }
    return "";
}

std::string localPathToConsole(const std::string& localPath, const std::vector<StorageRoot>& roots) {
    const std::string n = normalizePath(localPath);
    for (const StorageRoot& r : roots) {
        const std::string root = normalizePath(r.path);
        if (r.consoleName.empty() || toLowerAscii(n.substr(0, root.size())) != toLowerAscii(root)) continue;
        std::string rest = n.substr(root.size());
        for (char& c : rest)
            if (c == '/') c = '\\';
        while (!rest.empty() && rest[0] == '\\') rest.erase(0, 1);
        return r.consoleName + ":\\" + rest;
    }
    return localPath;
}

std::string iniValue(const std::string& text, const std::string& key) {
    const std::string want = toLowerAscii(key);
    size_t pos = 0;
    while (pos < text.size()) {
        size_t end = text.find('\n', pos);
        if (end == std::string::npos) end = text.size();
        std::string line = text.substr(pos, end - pos);
        pos = end + 1;
        size_t hash = line.find_first_of(";#");
        if (hash != std::string::npos) line.erase(hash);
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        if (toLowerAscii(trim(line.substr(0, eq))) == want) return trim(line.substr(eq + 1));
    }
    return "";
}

LaunchTarget findAurora(const std::vector<StorageRoot>& roots, const std::string& configured) {
    LaunchTarget t;
    if (!configured.empty()) {
        if (fileExists(configured)) {
            t.found = true;
            t.path = configured;
            t.source = "caminho configurado";
            return t;
        }
        t.reason = "o caminho configurado não existe: " + configured;
    }
    // launch.ini do DashLaunch: lido na raiz das unidades (somente leitura).
    for (const StorageRoot& r : roots) {
        std::vector<uint8_t> bytes;
        if (!readFileBytes(joinPath(r.path, "launch.ini"), bytes, 64 * 1024)) continue;
        const std::string text(bytes.begin(), bytes.end());
        const std::string def = iniValue(text, "Default");
        if (def.empty() || toLowerAscii(def).find("aurora") == std::string::npos) continue;
        const std::string local = consolePathToLocal(def, roots);
        if (!local.empty() && fileExists(local)) {
            t.found = true;
            t.path = local;
            t.source = "launch.ini do DashLaunch (Default = " + def + ")";
            return t;
        }
    }
    static const char* const kCandidates[] = {
        "Aurora/Aurora.xex",      "Apps/Aurora/Aurora.xex",  "Homebrew/Aurora/Aurora.xex",
        "Dashboards/Aurora/Aurora.xex", "Games/Aurora/Aurora.xex", "Aplicativos/Aurora/Aurora.xex",
    };
    for (const StorageRoot& r : roots)
        for (const char* c : kCandidates) {
            const std::string p = joinPath(r.path, c);
            if (fileExists(p)) {
                t.found = true;
                t.path = p;
                t.source = "local padrão";
                t.reason.clear();
                return t;
            }
        }
    if (t.reason.empty())
        t.reason = roots.empty() ? "nenhuma unidade de armazenamento disponível"
                                 : "Aurora.xex não encontrado (launch.ini e pastas comuns); defina o caminho em Configurações";
    return t;
}

}  // namespace nx
