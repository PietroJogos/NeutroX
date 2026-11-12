// Neutro X - decodificadores PNG/JPEG. Ver image_decode.h.
#include "graphics/image_decode.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace nx {

namespace {

bool fail(std::string* why, const char* msg) {
    if (why) *why = msg;
    return false;
}

uint32_t be32(const uint8_t* p) { return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3]; }

// ------------------------------------------------------------------ inflate

struct BitIn {
    const uint8_t* d;
    size_t n, pos = 0;
    uint32_t buf = 0;
    int cnt = 0;
    bool over = false;
    int bit() {
        if (cnt == 0) {
            if (pos >= n) { over = true; return 0; }
            buf = d[pos++];
            cnt = 8;
        }
        int b = int(buf & 1);
        buf >>= 1;
        --cnt;
        return b;
    }
    uint32_t bits(int k) {
        uint32_t v = 0;
        for (int i = 0; i < k; ++i) v |= uint32_t(bit()) << i;
        return v;
    }
};

struct Huff {
    uint16_t counts[16];
    uint16_t symbols[320];
};

bool buildHuff(Huff& h, const uint8_t* lengths, int n) {
    std::memset(h.counts, 0, sizeof h.counts);
    for (int i = 0; i < n; ++i) h.counts[lengths[i]]++;
    h.counts[0] = 0;
    uint16_t offs[16];
    offs[1] = 0;
    for (int i = 1; i < 15; ++i) offs[i + 1] = uint16_t(offs[i] + h.counts[i]);
    for (int i = 0; i < n; ++i)
        if (lengths[i]) h.symbols[offs[lengths[i]]++] = uint16_t(i);
    return true;
}

int decodeSym(BitIn& in, const Huff& h) {
    int code = 0, first = 0, index = 0;
    for (int len = 1; len < 16; ++len) {
        code |= in.bit();
        int count = h.counts[len];
        if (code - count < first) return h.symbols[index + (code - first)];
        index += count;
        first += count;
        first <<= 1;
        code <<= 1;
        if (in.over) return -1;
    }
    return -1;
}

const uint16_t kLenBase[] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
const uint8_t kLenExtra[] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
const uint16_t kDistBase[] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
const uint8_t kDistExtra[] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

bool inflateBlock(BitIn& in, const Huff& lit, const Huff& dist, std::vector<uint8_t>& out, size_t maxOut, std::string* why) {
    for (;;) {
        int sym = decodeSym(in, lit);
        if (sym < 0) return fail(why, "dados compactados corrompidos");
        if (sym < 256) {
            if (out.size() >= maxOut) return fail(why, "imagem grande demais");
            out.push_back(uint8_t(sym));
        } else if (sym == 256) {
            return true;
        } else {
            sym -= 257;
            if (sym >= 29) return fail(why, "comprimento inválido");
            size_t len = kLenBase[sym] + in.bits(kLenExtra[sym]);
            int ds = decodeSym(in, dist);
            if (ds < 0 || ds >= 30) return fail(why, "distância inválida");
            size_t d = kDistBase[ds] + in.bits(kDistExtra[ds]);
            if (d > out.size()) return fail(why, "referência fora dos dados");
            if (out.size() + len > maxOut) return fail(why, "imagem grande demais");
            size_t start = out.size() - d;
            for (size_t i = 0; i < len; ++i) out.push_back(out[start + i]);
        }
        if (in.over) return fail(why, "dados compactados truncados");
    }
}

}  // namespace

bool zlibInflate(const uint8_t* data, size_t size, std::vector<uint8_t>& out, size_t maxOut, std::string* why) {
    out.clear();
    if (size < 2 || (data[0] & 0x0F) != 8 || ((data[0] << 8) | data[1]) % 31 != 0) return fail(why, "cabeçalho zlib inválido");
    if (data[1] & 0x20) return fail(why, "dicionário zlib não suportado");
    BitIn in{data + 2, size - 2};
    int last = 0;
    do {
        last = in.bit();
        const int type = int(in.bits(2));
        if (type == 0) {
            in.cnt = 0;  // alinha no byte
            if (in.pos + 4 > in.n) return fail(why, "bloco sem compressão truncado");
            const uint16_t len = uint16_t(in.d[in.pos] | (in.d[in.pos + 1] << 8));
            in.pos += 4;
            if (in.pos + len > in.n || out.size() + len > maxOut) return fail(why, "bloco sem compressão inválido");
            out.insert(out.end(), in.d + in.pos, in.d + in.pos + len);
            in.pos += len;
        } else if (type == 1) {
            uint8_t l[288 + 30];
            for (int i = 0; i < 144; ++i) l[i] = 8;
            for (int i = 144; i < 256; ++i) l[i] = 9;
            for (int i = 256; i < 280; ++i) l[i] = 7;
            for (int i = 280; i < 288; ++i) l[i] = 8;
            for (int i = 0; i < 30; ++i) l[288 + i] = 5;
            Huff lit, dist;
            buildHuff(lit, l, 288);
            buildHuff(dist, l + 288, 30);
            if (!inflateBlock(in, lit, dist, out, maxOut, why)) return false;
        } else if (type == 2) {
            const int hlit = int(in.bits(5)) + 257, hdist = int(in.bits(5)) + 1, hclen = int(in.bits(4)) + 4;
            static const uint8_t order[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
            uint8_t cl[19] = {};
            for (int i = 0; i < hclen; ++i) cl[order[i]] = uint8_t(in.bits(3));
            Huff clh;
            buildHuff(clh, cl, 19);
            uint8_t lens[320] = {};
            int idx = 0;
            while (idx < hlit + hdist) {
                int sym = decodeSym(in, clh);
                if (sym < 0) return fail(why, "tabela Huffman corrompida");
                if (sym < 16) lens[idx++] = uint8_t(sym);
                else {
                    int rep = 0, val = 0;
                    if (sym == 16) { if (idx == 0) return fail(why, "repetição inválida"); val = lens[idx - 1]; rep = 3 + int(in.bits(2)); }
                    else if (sym == 17) rep = 3 + int(in.bits(3));
                    else rep = 11 + int(in.bits(7));
                    if (idx + rep > hlit + hdist) return fail(why, "tabela Huffman longa demais");
                    while (rep--) lens[idx++] = uint8_t(val);
                }
            }
            Huff lit, dist;
            buildHuff(lit, lens, hlit);
            buildHuff(dist, lens + hlit, hdist);
            if (!inflateBlock(in, lit, dist, out, maxOut, why)) return false;
        } else {
            return fail(why, "bloco deflate inválido");
        }
        if (in.over) return fail(why, "dados compactados truncados");
    } while (!last);
    return true;
}

ImageFormat sniffImage(const uint8_t* d, size_t n) {
    if (n >= 8 && d[0] == 0x89 && d[1] == 'P' && d[2] == 'N' && d[3] == 'G') return ImageFormat::Png;
    if (n >= 3 && d[0] == 0xFF && d[1] == 0xD8 && d[2] == 0xFF) return ImageFormat::Jpeg;
    return ImageFormat::Unknown;
}

// ------------------------------------------------------------------ PNG

bool decodePng(const uint8_t* d, size_t n, Image& img, std::string* why) {
    img = Image();
    if (sniffImage(d, n) != ImageFormat::Png) return fail(why, "não é PNG");
    size_t p = 8;
    uint32_t w = 0, h = 0;
    int depth = 0, ctype = 0, interlace = 0;
    std::vector<uint8_t> idat;
    uint32_t palette[256];
    int palN = 0;
    for (int i = 0; i < 256; ++i) palette[i] = 0xFF000000u;
    while (p + 8 <= n) {
        const uint32_t len = be32(d + p);
        const uint8_t* type = d + p + 4;
        if (len > n || p + 12 + len > n) return fail(why, "PNG truncado");
        const uint8_t* body = d + p + 8;
        if (!std::memcmp(type, "IHDR", 4) && len >= 13) {
            w = be32(body);
            h = be32(body + 4);
            depth = body[8];
            ctype = body[9];
            interlace = body[12];
        } else if (!std::memcmp(type, "PLTE", 4)) {
            palN = int(std::min<uint32_t>(256, len / 3));
            for (int i = 0; i < palN; ++i)
                palette[i] = 0xFF000000u | (uint32_t(body[i * 3]) << 16) | (uint32_t(body[i * 3 + 1]) << 8) | body[i * 3 + 2];
        } else if (!std::memcmp(type, "tRNS", 4) && ctype == 3) {
            for (uint32_t i = 0; i < len && i < 256; ++i) palette[i] = (palette[i] & 0x00FFFFFFu) | (uint32_t(body[i]) << 24);
        } else if (!std::memcmp(type, "IDAT", 4)) {
            idat.insert(idat.end(), body, body + len);
        } else if (!std::memcmp(type, "IEND", 4)) {
            break;
        }
        p += 12 + len;
    }
    if (w == 0 || h == 0 || w > 4096 || h > 4096) return fail(why, "dimensões PNG inválidas");
    if (depth != 8) return fail(why, "PNG com profundidade diferente de 8 bits não suportado");
    if (interlace) return fail(why, "PNG entrelaçado não suportado");
    int bpp = ctype == 0 ? 1 : ctype == 2 ? 3 : ctype == 3 ? 1 : ctype == 4 ? 2 : ctype == 6 ? 4 : 0;
    if (!bpp) return fail(why, "tipo de cor PNG inválido");
    const size_t stride = size_t(w) * bpp;
    std::vector<uint8_t> raw;
    if (!zlibInflate(idat.data(), idat.size(), raw, (stride + 1) * h + 16, why)) return false;
    if (raw.size() < (stride + 1) * h) return fail(why, "dados PNG incompletos");
    std::vector<uint8_t> prev(stride, 0), cur(stride);
    img.w = int(w);
    img.h = int(h);
    img.px.resize(size_t(w) * h);
    for (uint32_t y = 0; y < h; ++y) {
        const uint8_t* line = raw.data() + y * (stride + 1);
        const int f = line[0];
        for (size_t x = 0; x < stride; ++x) {
            const int a = x >= size_t(bpp) ? cur[x - bpp] : 0, b = prev[x], c = x >= size_t(bpp) ? prev[x - bpp] : 0;
            int v = line[1 + x];
            switch (f) {
                case 0: break;
                case 1: v += a; break;
                case 2: v += b; break;
                case 3: v += (a + b) / 2; break;
                case 4: {
                    int pp = a + b - c, pa = std::abs(pp - a), pb = std::abs(pp - b), pc = std::abs(pp - c);
                    v += (pa <= pb && pa <= pc) ? a : (pb <= pc ? b : c);
                    break;
                }
                default: return fail(why, "filtro PNG inválido");
            }
            cur[x] = uint8_t(v);
        }
        uint32_t* out = img.px.data() + size_t(y) * w;
        for (uint32_t x = 0; x < w; ++x) {
            const uint8_t* s = cur.data() + x * bpp;
            switch (ctype) {
                case 0: out[x] = 0xFF000000u | (uint32_t(s[0]) * 0x010101u); break;
                case 2: out[x] = 0xFF000000u | (uint32_t(s[0]) << 16) | (uint32_t(s[1]) << 8) | s[2]; break;
                case 3: out[x] = palette[s[0]]; break;
                case 4: out[x] = (uint32_t(s[1]) << 24) | (uint32_t(s[0]) * 0x010101u); break;
                case 6: out[x] = (uint32_t(s[3]) << 24) | (uint32_t(s[0]) << 16) | (uint32_t(s[1]) << 8) | s[2]; break;
            }
        }
        prev.swap(cur);
    }
    return true;
}

// ------------------------------------------------------------------ JPEG baseline

namespace {

struct JHuff {
    uint8_t vals[256];
    int maxcode[18];
    int valptr[17];
    int mincode[17];
    bool present = false;
};

void buildJHuff(JHuff& t, const uint8_t* counts, const uint8_t* vals, int nvals) {
    std::memcpy(t.vals, vals, size_t(std::min(nvals, 256)));
    int code = 0, k = 0;
    for (int l = 1; l <= 16; ++l) {
        t.valptr[l] = k;
        t.mincode[l] = code;
        code += counts[l - 1];
        k += counts[l - 1];
        t.maxcode[l] = counts[l - 1] ? code - 1 : -1;
        code <<= 1;
    }
    t.maxcode[17] = 0x7FFFFFFF;
    t.present = true;
}

struct JBits {
    const uint8_t* d;
    size_t n, pos;
    uint32_t buf = 0;
    int cnt = 0;
    bool marker = false;  // encontrou um marcador (fim dos dados de entropia)
    int bit() {
        if (cnt == 0) {
            uint8_t b = 0;
            if (!marker && pos < n) {
                b = d[pos];
                if (b == 0xFF) {
                    uint8_t nx = pos + 1 < n ? d[pos + 1] : 0;
                    if (nx == 0x00) pos += 2;
                    else { marker = true; b = 0; }
                } else {
                    ++pos;
                }
            }
            buf = b;
            cnt = 8;
        }
        --cnt;
        return int((buf >> cnt) & 1);
    }
    int bits(int k) {
        int v = 0;
        for (int i = 0; i < k; ++i) v = (v << 1) | bit();
        return v;
    }
    void resetAfterRst() {
        cnt = 0;
        marker = false;
        // pula ate o marcador RSTn
        while (pos + 1 < n && !(d[pos] == 0xFF && d[pos + 1] >= 0xD0 && d[pos + 1] <= 0xD7)) ++pos;
        if (pos + 1 < n) pos += 2;
    }
};

int jdecode(JBits& b, const JHuff& t) {
    int code = 0;
    for (int l = 1; l <= 16; ++l) {
        code = (code << 1) | b.bit();
        if (t.maxcode[l] >= 0 && code <= t.maxcode[l] && code >= t.mincode[l]) {
            int idx = t.valptr[l] + code - t.mincode[l];
            return idx < 256 ? t.vals[idx] : -1;
        }
    }
    return -1;
}

int extend(int v, int t) { return t == 0 ? 0 : (v < (1 << (t - 1)) ? v - (1 << t) + 1 : v); }

const uint8_t kZigzag[64] = {0,  1,  8,  16, 9,  2,  3,  10, 17, 24, 32, 25, 18, 11, 4,  5,  12, 19, 26, 33, 40, 48,
                             41, 34, 27, 20, 13, 6,  7,  14, 21, 28, 35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23,
                             30, 37, 44, 51, 58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63};

void idct8x8(const int* in, uint8_t* out, int outStride) {
    static float cosT[8][8];
    static bool init = false;
    if (!init) {
        for (int x = 0; x < 8; ++x)
            for (int u = 0; u < 8; ++u)
                cosT[x][u] = float((u == 0 ? std::sqrt(0.5) : 1.0) * std::cos((2 * x + 1) * u * 3.14159265358979 / 16.0));
        init = true;
    }
    float tmp[64];
    for (int y = 0; y < 8; ++y)
        for (int u = 0; u < 8; ++u) {
            float s = 0;
            for (int v = 0; v < 8; ++v) s += cosT[y][v] * float(in[v * 8 + u]);
            tmp[y * 8 + u] = s;
        }
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x) {
            float s = 0;
            for (int u = 0; u < 8; ++u) s += cosT[x][u] * tmp[y * 8 + u];
            int v = int(std::lround(s / 4.0f + 128.0f));
            out[y * outStride + x] = uint8_t(v < 0 ? 0 : v > 255 ? 255 : v);
        }
}

}  // namespace

bool decodeJpeg(const uint8_t* d, size_t n, Image& img, std::string* why) {
    img = Image();
    if (sniffImage(d, n) != ImageFormat::Jpeg) return fail(why, "não é JPEG");
    uint16_t qt[4][64] = {};
    JHuff hdc[4], hac[4];
    struct Comp { int id, h, v, tq, td, ta; std::vector<uint8_t> plane; int pw, ph; int pred; } comp[3];
    int ncomp = 0, width = 0, height = 0, restart = 0;
    size_t p = 2;
    while (p + 4 <= n) {
        if (d[p] != 0xFF) { ++p; continue; }
        const uint8_t m = d[p + 1];
        if (m == 0xD8 || m == 0x01 || (m >= 0xD0 && m <= 0xD7)) { p += 2; continue; }
        if (m == 0xD9) break;
        const size_t len = size_t((d[p + 2] << 8) | d[p + 3]);
        if (len < 2 || p + 2 + len > n) return fail(why, "JPEG truncado");
        const uint8_t* s = d + p + 4;
        const size_t sl = len - 2;
        if (m == 0xC2 || m == 0xC6 || m == 0xCA) return fail(why, "JPEG progressivo não suportado");
        if (m == 0xC3 || m == 0xC5 || m == 0xC7 || m == 0xC9 || m == 0xCB || m == 0xCD || m == 0xCF)
            return fail(why, "JPEG sem perdas/aritmético não suportado");
        if (m == 0xDB) {  // DQT
            size_t q = 0;
            while (q < sl) {
                const int pq = s[q] >> 4, tq = s[q] & 3;
                ++q;
                for (int i = 0; i < 64; ++i) {
                    if (q + (pq ? 2 : 1) > sl) return fail(why, "tabela de quantização truncada");
                    qt[tq][kZigzag[i]] = pq ? uint16_t((s[q] << 8) | s[q + 1]) : s[q];
                    q += pq ? 2 : 1;
                }
            }
        } else if (m == 0xC4) {  // DHT
            size_t q = 0;
            while (q + 17 <= sl) {
                const int tc = s[q] >> 4, th = s[q] & 3;
                const uint8_t* counts = s + q + 1;
                int tot = 0;
                for (int i = 0; i < 16; ++i) tot += counts[i];
                if (tot > 256 || q + 17 + size_t(tot) > sl) return fail(why, "tabela Huffman truncada");
                buildJHuff(tc ? hac[th] : hdc[th], counts, s + q + 17, tot);
                q += 17 + size_t(tot);
            }
        } else if (m == 0xC0 || m == 0xC1) {  // SOF0/SOF1
            if (sl < 6) return fail(why, "SOF inválido");
            height = (s[1] << 8) | s[2];
            width = (s[3] << 8) | s[4];
            ncomp = s[5];
            if (ncomp != 1 && ncomp != 3) return fail(why, "número de componentes não suportado");
            if (s[0] != 8) return fail(why, "JPEG de 12 bits não suportado");
            if (sl < size_t(6 + ncomp * 3)) return fail(why, "SOF truncado");
            for (int i = 0; i < ncomp; ++i) {
                comp[i].id = s[6 + i * 3];
                comp[i].h = s[7 + i * 3] >> 4;
                comp[i].v = s[7 + i * 3] & 15;
                comp[i].tq = s[8 + i * 3] & 3;
                if (comp[i].h < 1 || comp[i].h > 2 || comp[i].v < 1 || comp[i].v > 2) return fail(why, "amostragem de cor não suportada");
            }
            if (width <= 0 || height <= 0 || width > 4096 || height > 4096) return fail(why, "dimensões JPEG inválidas");
        } else if (m == 0xDD) {
            if (sl >= 2) restart = (s[0] << 8) | s[1];
        } else if (m == 0xDA) {  // SOS: dados de entropia a seguir
            if (!ncomp) return fail(why, "SOS antes do SOF");
            const int ns = s[0];
            if (ns != ncomp) return fail(why, "varredura parcial não suportada");
            for (int i = 0; i < ns; ++i) {
                const int cid = s[1 + i * 2], tables = s[2 + i * 2];
                for (int k = 0; k < ncomp; ++k)
                    if (comp[k].id == cid) { comp[k].td = tables >> 4; comp[k].ta = tables & 3; }
            }
            int hmax = 1, vmax = 1;
            for (int i = 0; i < ncomp; ++i) { hmax = std::max(hmax, comp[i].h); vmax = std::max(vmax, comp[i].v); }
            const int mcuW = 8 * hmax, mcuH = 8 * vmax;
            const int mcusX = (width + mcuW - 1) / mcuW, mcusY = (height + mcuH - 1) / mcuH;
            for (int i = 0; i < ncomp; ++i) {
                comp[i].pw = mcusX * comp[i].h * 8;
                comp[i].ph = mcusY * comp[i].v * 8;
                comp[i].plane.assign(size_t(comp[i].pw) * comp[i].ph, 0);
                comp[i].pred = 0;
                if (!hdc[comp[i].td].present || !hac[comp[i].ta].present) return fail(why, "tabela Huffman ausente");
            }
            JBits b{d, n, p + 2 + len};
            int coef[64];
            int mcuCount = 0;
            for (int my = 0; my < mcusY; ++my)
                for (int mx = 0; mx < mcusX; ++mx) {
                    if (restart && mcuCount && mcuCount % restart == 0) {
                        b.resetAfterRst();
                        for (int i = 0; i < ncomp; ++i) comp[i].pred = 0;
                    }
                    ++mcuCount;
                    for (int ci = 0; ci < ncomp; ++ci) {
                        Comp& c = comp[ci];
                        for (int by = 0; by < c.v; ++by)
                            for (int bx = 0; bx < c.h; ++bx) {
                                std::memset(coef, 0, sizeof coef);
                                int t = jdecode(b, hdc[c.td]);
                                if (t < 0 || t > 16) return fail(why, "dados JPEG corrompidos (DC)");
                                c.pred += extend(b.bits(t), t);
                                coef[0] = c.pred * qt[c.tq][0];
                                for (int k = 1; k < 64;) {
                                    int rs = jdecode(b, hac[c.ta]);
                                    if (rs < 0) return fail(why, "dados JPEG corrompidos (AC)");
                                    int r = rs >> 4, sz = rs & 15;
                                    if (sz == 0) {
                                        if (r != 15) break;
                                        k += 16;
                                        continue;
                                    }
                                    k += r;
                                    if (k > 63) return fail(why, "dados JPEG corrompidos (índice)");
                                    coef[kZigzag[k]] = extend(b.bits(sz), sz) * qt[c.tq][kZigzag[k]];
                                    ++k;
                                }
                                const int ox = (mx * c.h + bx) * 8, oy = (my * c.v + by) * 8;
                                idct8x8(coef, c.plane.data() + size_t(oy) * c.pw + ox, c.pw);
                            }
                    }
                }
            // Converte para ARGB (componentes subamostrados: vizinho mais proximo).
            img.w = width;
            img.h = height;
            img.px.resize(size_t(width) * height);
            for (int y = 0; y < height; ++y)
                for (int x = 0; x < width; ++x) {
                    auto sample = [&](const Comp& c) {
                        const int sx = x * c.h / hmax, sy = y * c.v / vmax;
                        return int(c.plane[size_t(sy) * c.pw + sx]);
                    };
                    uint32_t out;
                    if (ncomp == 1) {
                        const uint32_t g = uint32_t(sample(comp[0]));
                        out = 0xFF000000u | g * 0x010101u;
                    } else {
                        const float Y = float(sample(comp[0])), cb = float(sample(comp[1])) - 128.0f, cr = float(sample(comp[2])) - 128.0f;
                        auto cl = [](float v) { int i = int(std::lround(v)); return uint32_t(i < 0 ? 0 : i > 255 ? 255 : i); };
                        out = 0xFF000000u | (cl(Y + 1.402f * cr) << 16) | (cl(Y - 0.344136f * cb - 0.714136f * cr) << 8) |
                              cl(Y + 1.772f * cb);
                    }
                    img.px[size_t(y) * width + x] = out;
                }
            return true;
        }
        p += 2 + len;
    }
    return fail(why, "JPEG sem dados de imagem");
}

bool decodeImage(const uint8_t* d, size_t n, Image& out, std::string* why) {
    switch (sniffImage(d, n)) {
        case ImageFormat::Png: return decodePng(d, n, out, why);
        case ImageFormat::Jpeg: return decodeJpeg(d, n, out, why);
        default: return fail(why, "formato de imagem não reconhecido (esperado PNG ou JPEG)");
    }
}

Image resizeImage(const Image& src, int w, int h) {
    Image out;
    if (src.w <= 0 || src.h <= 0 || w <= 0 || h <= 0) return out;
    out.w = w;
    out.h = h;
    out.px.resize(size_t(w) * h);
    for (int y = 0; y < h; ++y) {
        const float sy0 = float(y) * src.h / h, sy1 = float(y + 1) * src.h / h;
        for (int x = 0; x < w; ++x) {
            const float sx0 = float(x) * src.w / w, sx1 = float(x + 1) * src.w / w;
            // media por area (com no minimo 1 amostra)
            int ix0 = int(sx0), ix1 = std::max(ix0 + 1, int(std::ceil(sx1))), iy0 = int(sy0), iy1 = std::max(iy0 + 1, int(std::ceil(sy1)));
            ix1 = std::min(ix1, src.w);
            iy1 = std::min(iy1, src.h);
            uint32_t a = 0, r = 0, g = 0, b = 0, cnt = 0;
            for (int yy = iy0; yy < iy1; ++yy)
                for (int xx = ix0; xx < ix1; ++xx) {
                    const uint32_t p = src.px[size_t(yy) * src.w + xx];
                    a += p >> 24; r += (p >> 16) & 255; g += (p >> 8) & 255; b += p & 255;
                    ++cnt;
                }
            if (!cnt) cnt = 1;
            out.px[size_t(y) * w + x] = ((a / cnt) << 24) | ((r / cnt) << 16) | ((g / cnt) << 8) | (b / cnt);
        }
    }
    return out;
}

Image coverFit(const Image& src, int w, int h) {
    if (src.w <= 0 || src.h <= 0) return Image();
    // recorta a sobra (proporcao alvo w:h) do centro e redimensiona
    const float target = float(w) / float(h), have = float(src.w) / float(src.h);
    int cw = src.w, ch = src.h, cx = 0, cy = 0;
    if (have > target * 1.02f) { cw = int(src.h * target); cx = (src.w - cw) / 2; }
    else if (have < target / 1.02f) { ch = int(src.w / target); cy = (src.h - ch) / 2; }
    Image crop;
    crop.w = std::max(1, cw);
    crop.h = std::max(1, ch);
    crop.px.resize(size_t(crop.w) * crop.h);
    for (int y = 0; y < crop.h; ++y)
        std::memcpy(&crop.px[size_t(y) * crop.w], &src.px[size_t(y + cy) * src.w + cx], size_t(crop.w) * 4);
    return resizeImage(crop, w, h);
}

}  // namespace nx
