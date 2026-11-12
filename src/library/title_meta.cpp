// Neutro X - metadados de titulos (XEX2 e STFS). Ver title_meta.h.
#include "library/title_meta.h"

#include <cstdio>
#include "core/fsutil.h"

namespace nx {

namespace {

uint32_t be32(const uint8_t* p) { return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3]; }
uint16_t be16(const uint8_t* p) { return uint16_t((p[0] << 8) | p[1]); }

// UTF-16 big-endian (terminada em zero) -> UTF-8.
std::string utf16beToUtf8(const uint8_t* p, size_t maxChars) {
    std::string out;
    for (size_t i = 0; i < maxChars; ++i) {
        uint32_t c = be16(p + i * 2);
        if (c == 0) break;
        if (c >= 0xD800 && c <= 0xDBFF && i + 1 < maxChars) {
            uint32_t lo = be16(p + (i + 1) * 2);
            if (lo >= 0xDC00 && lo <= 0xDFFF) { c = 0x10000 + ((c - 0xD800) << 10) + (lo - 0xDC00); ++i; }
        }
        if (c < 0x20) continue;
        if (c < 0x80) out.push_back(char(c));
        else if (c < 0x800) { out.push_back(char(0xC0 | (c >> 6))); out.push_back(char(0x80 | (c & 0x3F))); }
        else if (c < 0x10000) {
            out.push_back(char(0xE0 | (c >> 12)));
            out.push_back(char(0x80 | ((c >> 6) & 0x3F)));
            out.push_back(char(0x80 | (c & 0x3F)));
        } else {
            out.push_back(char(0xF0 | (c >> 18)));
            out.push_back(char(0x80 | ((c >> 12) & 0x3F)));
            out.push_back(char(0x80 | ((c >> 6) & 0x3F)));
            out.push_back(char(0x80 | (c & 0x3F)));
        }
    }
    // tira espacos nas pontas
    size_t a = out.find_first_not_of(' ');
    if (a == std::string::npos) return "";
    size_t b = out.find_last_not_of(' ');
    return out.substr(a, b - a + 1);
}

}  // namespace

std::string hex8(uint32_t v) {
    char b[12];
    std::snprintf(b, sizeof b, "%08X", unsigned(v));
    return b;
}

bool parseHex8(const std::string& s, uint32_t& out) {
    if (s.size() != 8) return false;
    uint32_t v = 0;
    for (char c : s) {
        int d = (c >= '0' && c <= '9') ? c - '0' : (c >= 'A' && c <= 'F') ? c - 'A' + 10 : (c >= 'a' && c <= 'f') ? c - 'a' + 10 : -1;
        if (d < 0) return false;
        v = (v << 4) | uint32_t(d);
    }
    out = v;
    return true;
}

bool parseXexHeader(const uint8_t* d, size_t n, XexMeta& m) {
    m = XexMeta();
    if (n < 0x18 || d[0] != 'X' || d[1] != 'E' || d[2] != 'X' || d[3] != '2') {
        m.error = n >= 4 && d[0] == 'X' && d[1] == 'E' && d[2] == 'X' ? "formato XEX antigo (não XEX2)" : "não é um arquivo XEX2";
        return false;
    }
    m.moduleFlags = be32(d + 4);
    m.titleModule = (m.moduleFlags & 0x01) != 0;
    m.dll = (m.moduleFlags & 0x08) != 0;
    m.patch = (m.moduleFlags & 0x70) != 0;  // MODULE_PATCH, PATCH_FULL, PATCH_DELTA
    const uint32_t count = be32(d + 0x14);
    if (count > 256) { m.error = "cabeçalho XEX2 inválido (muitos cabeçalhos opcionais)"; return false; }
    for (uint32_t i = 0; i < count; ++i) {
        const size_t o = 0x18 + size_t(i) * 8;
        if (o + 8 > n) { m.error = "cabeçalho XEX2 truncado"; return false; }
        const uint32_t key = be32(d + o), val = be32(d + o + 4);
        if (key == 0x00040006) {  // EXECUTION_INFO
            if (size_t(val) + 24 > n) { m.error = "Execution ID fora do cabeçalho"; return false; }
            const uint8_t* e = d + val;
            m.mediaId = be32(e);
            m.version = be32(e + 4);
            m.baseVersion = be32(e + 8);
            m.titleId = be32(e + 12);
            m.discNumber = e[18];
            m.discCount = e[19];
            m.hasExecutionId = true;
        } else if (key == 0x000003FF) {  // FILE_FORMAT_INFO
            if (size_t(val) + 8 <= n) {
                m.encryption = be16(d + val + 4);
                m.compression = be16(d + val + 6);
            }
        } else if (key == 0x000183FF) {  // ORIGINAL_PE_NAME
            if (size_t(val) + 4 <= n) {
                uint32_t sz = be32(d + val);
                if (sz > 4 && sz < 512 && size_t(val) + sz <= n) {
                    for (uint32_t k = 4; k < sz && d[val + k]; ++k) {
                        char c = char(d[val + k]);
                        if (c >= 0x20 && c < 0x7F) m.originalName.push_back(c);
                    }
                }
            }
        }
    }
    if (m.dll) { m.error = "módulo DLL (não é um título)"; return false; }
    if (m.patch) { m.error = "atualização/patch (não é um título)"; return false; }
    if (!m.titleModule) { m.error = "módulo sem a marca de título"; return false; }
    if (!m.hasExecutionId || m.titleId == 0) { m.error = "sem Execution ID (Title ID ausente)"; return false; }
    m.valid = true;
    return true;
}

bool readXexMeta(const std::string& path, XexMeta& m) {
    std::vector<uint8_t> head;
    // O cabecalho termina no inicio dos dados PE (offset em +8); le no maximo 64 KB.
    if (!readFileRange(path, 0, 0x18, head)) {
        m = XexMeta();
        m.error = "não foi possível ler o arquivo";
        return false;
    }
    uint32_t peOffset = be32(head.data() + 8);
    size_t want = peOffset > 0x18 && peOffset < 65536 ? peOffset : 65536;
    const uint64_t size = fileSize(path);
    if (want > size) want = size_t(size);
    if (!readFileRange(path, 0, want, head) && head.size() < 0x18) {
        m = XexMeta();
        m.error = "não foi possível ler o cabeçalho";
        return false;
    }
    return parseXexHeader(head.data(), head.size(), m);
}

bool looksLikeStfs(const uint8_t* d, size_t n) {
    if (n < 4) return false;
    return (d[0] == 'C' && d[1] == 'O' && d[2] == 'N' && d[3] == ' ') || (d[0] == 'L' && d[1] == 'I' && d[2] == 'V' && d[3] == 'E') ||
           (d[0] == 'P' && d[1] == 'I' && d[2] == 'R' && d[3] == 'S');
}

bool parseStfsHeader(const uint8_t* d, size_t n, StfsMeta& m) {
    m = StfsMeta();
    if (!looksLikeStfs(d, n)) { m.error = "não é um pacote STFS"; return false; }
    if (n < 0x1712 + 8) { m.error = "cabeçalho STFS truncado"; return false; }
    m.magic.assign(reinterpret_cast<const char*>(d), 4);
    m.contentType = be32(d + 0x344);
    m.mediaId = be32(d + 0x354);
    m.version = be32(d + 0x358);
    m.titleId = be32(d + 0x360);
    m.displayName = utf16beToUtf8(d + 0x411, 0x80);
    m.titleName = utf16beToUtf8(d + 0x1691, 0x40);
    const uint32_t thumbSize = be32(d + 0x1712);
    const uint32_t titleThumbSize = be32(d + 0x1716);
    // Miniatura do conteudo em 0x171A; se vazia, a do titulo em 0x571A (ambas PNG, ate 16 KB).
    auto take = [&](size_t off, uint32_t sz) {
        if (sz == 0 || sz > 0x4000 || off + sz > n) return false;
        const uint8_t* p = d + off;
        if (sz < 8 || p[0] != 0x89 || p[1] != 'P' || p[2] != 'N' || p[3] != 'G') return false;
        m.thumbnail.assign(p, p + sz);
        return true;
    };
    if (!take(0x171A, thumbSize)) take(0x571A, titleThumbSize);
    if (m.titleId == 0) { m.error = "pacote sem Title ID"; return false; }
    m.valid = true;
    return true;
}

bool readStfsMeta(const std::string& path, StfsMeta& m) {
    std::vector<uint8_t> head;
    const uint64_t size = fileSize(path);
    size_t want = size_t(size < 0x971A ? size : 0x971A);
    if (want < 4 || !readFileRange(path, 0, want, head)) {
        m = StfsMeta();
        m.error = "não foi possível ler o pacote";
        return false;
    }
    return parseStfsHeader(head.data(), head.size(), m);
}

}  // namespace nx
