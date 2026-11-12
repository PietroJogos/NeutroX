// Neutro X - utilidades de arquivo. Ver fsutil.h.
#include "core/fsutil.h"

#include <algorithm>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <system_error>

namespace nx {

namespace fs = std::filesystem;

namespace {
fs::path P(const std::string& s) { return fs::u8path(s); }
}  // namespace

bool fileExists(const std::string& path) {
    std::error_code ec;
    return fs::is_regular_file(P(path), ec);
}

bool dirExists(const std::string& path) {
    std::error_code ec;
    return fs::is_directory(P(path), ec);
}

uint64_t fileSize(const std::string& path) {
    std::error_code ec;
    auto n = fs::file_size(P(path), ec);
    return ec ? 0 : uint64_t(n);
}

bool readFileBytes(const std::string& path, std::vector<uint8_t>& out, size_t maxBytes) {
    out.clear();
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    bool ok = true;
    uint8_t buf[16384];
    for (;;) {
        size_t n = std::fread(buf, 1, sizeof buf, f);
        if (n == 0) break;
        if (out.size() + n > maxBytes) { ok = false; break; }
        out.insert(out.end(), buf, buf + n);
    }
    if (std::ferror(f)) ok = false;
    std::fclose(f);
    if (!ok) out.clear();
    return ok;
}

bool readFileRange(const std::string& path, uint64_t offset, size_t count, std::vector<uint8_t>& out) {
    out.clear();
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    bool ok = std::fseek(f, long(offset), SEEK_SET) == 0;
    if (ok) {
        out.resize(count);
        size_t n = std::fread(out.data(), 1, count, f);
        out.resize(n);
        ok = n == count;
    }
    std::fclose(f);
    return ok;
}

bool writeFileAtomic(const std::string& path, const void* data, size_t bytes) {
    const std::string tmp = path + ".tmp";
    FILE* f = std::fopen(tmp.c_str(), "wb");
    if (!f) return false;
    bool ok = bytes == 0 || std::fwrite(data, 1, bytes, f) == bytes;
    ok = std::fflush(f) == 0 && ok;
    ok = std::fclose(f) == 0 && ok;
    if (!ok) { std::remove(tmp.c_str()); return false; }
    std::error_code ec;
    fs::rename(P(tmp), P(path), ec);  // substitui o destino (POSIX e xsdk; Windows via MoveFileEx)
    if (ec) {
        fs::remove(P(path), ec);
        ec.clear();
        fs::rename(P(tmp), P(path), ec);
    }
    if (ec) { std::remove(tmp.c_str()); return false; }
    return true;
}

bool writeFileAtomic(const std::string& path, const std::string& text) {
    return writeFileAtomic(path, text.data(), text.size());
}

bool listDir(const std::string& path, std::vector<DirEntry>& out, std::string* error) {
    out.clear();
    try {
        std::error_code ec;
        fs::directory_iterator it(P(path), fs::directory_options::skip_permission_denied, ec);
        if (ec) {
            if (error) *error = ec.message();
            return false;
        }
        for (const auto& de : it) {
            DirEntry e;
            e.name = de.path().filename().u8string();
            if (e.name == "." || e.name == "..") continue;
            std::error_code ec2;
            e.isDir = de.is_directory(ec2);
            if (!e.isDir) {
                auto sz = de.file_size(ec2);
                e.size = ec2 ? 0 : uint64_t(sz);
            }
            out.push_back(std::move(e));
        }
    } catch (const std::exception& ex) {
        if (error) *error = ex.what();
        return false;
    }
    std::sort(out.begin(), out.end(), [](const DirEntry& a, const DirEntry& b) {
        if (a.isDir != b.isDir) return a.isDir;
        return toLowerAscii(a.name) < toLowerAscii(b.name);
    });
    return true;
}

bool makeDirs(const std::string& path) {
    std::error_code ec;
    fs::create_directories(P(path), ec);
    return dirExists(path);
}

std::string toLowerAscii(std::string s) {
    for (char& c : s)
        if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
    return s;
}

bool endsWithNoCase(const std::string& s, const std::string& suffix) {
    if (suffix.size() > s.size()) return false;
    return toLowerAscii(s.substr(s.size() - suffix.size())) == toLowerAscii(suffix);
}

std::string normalizePath(const std::string& p) {
    std::string s;
    s.reserve(p.size());
    for (char c : p) {
        if (c == '\\') c = '/';
        if (c == '/' && !s.empty() && s.back() == '/') continue;
        s.push_back(c);
    }
    // remove '/' final, mas mantem raizes como "hdd:/", "C:/" e "/"
    while (s.size() > 1 && s.back() == '/' && s[s.size() - 2] != ':') s.pop_back();
    return s;
}

std::string joinPath(const std::string& dir, const std::string& name) {
    if (dir.empty()) return name;
    std::string d = normalizePath(dir);
    if (d.back() != '/') d.push_back('/');
    std::string n = name;
    while (!n.empty() && (n[0] == '/' || n[0] == '\\')) n.erase(0, 1);
    return d + n;
}

std::string parentPath(const std::string& p) {
    std::string s = normalizePath(p);
    size_t slash = s.find_last_of('/');
    if (slash == std::string::npos) return "";
    if (slash > 0 && s[slash - 1] == ':') return s.substr(0, slash + 1);  // "hdd:/X" -> "hdd:/"
    if (slash == 0) return "/";
    return s.substr(0, slash);
}

std::string baseName(const std::string& p) {
    std::string s = normalizePath(p);
    size_t slash = s.find_last_of('/');
    return slash == std::string::npos ? s : s.substr(slash + 1);
}

}  // namespace nx
