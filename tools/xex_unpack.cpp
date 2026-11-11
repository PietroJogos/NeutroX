// Neutro X - xex_unpack: extrai a imagem PE de um XEX2 NAO criptografado.
//
// Somente leitura e somente para arquivos sem criptografia:
//   - "nenhuma" compressao: copia direta;
//   - compressao "basica": blocos de dados + zeros;
//   - compressao "normal": blocos encadeados -> fluxo LZX -> imagem.
// Arquivos criptografados sao RECUSADOS: abri-los exigiria chaves do console,
// o que seria contornar uma protecao. Nenhuma chave e usada ou incluida.
//
// Saida: <pasta>/imagem.bin e <pasta>/recursos/<nome>.bin (tabela RESOURCE_INFO).
// Uso:   neutrox_xex_unpack <arquivo.xex> <pasta_saida>
// Codigos de saida: 0 ok, 2 erro de leitura/formato, 3 criptografado (recusado).
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace {

uint32_t be32(const uint8_t* p) { return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3]; }
uint16_t be16(const uint8_t* p) { return uint16_t((p[0] << 8) | p[1]); }

// ------------------------------------------------------------ LZX
// Decodificador LZX (formato publico usado em CAB/CHM e no XEX), escrito a
// partir da especificacao do formato. Quadros de 32 KB, sem reinicio.
class Lzx {
public:
    Lzx(const uint8_t* in, size_t inLen, unsigned windowBits) : in_(in), inLen_(inLen) {
        windowSize_ = 1u << windowBits;
        window_.assign(windowSize_, 0);
        static const int slots[] = {30, 32, 34, 36, 38, 42, 50};
        posSlots_ = (windowBits >= 15 && windowBits <= 21) ? slots[windowBits - 15] : 0;
        mainElements_ = 256 + posSlots_ * 8;
        for (int i = 0, j = 0; i < 51; i += 2) {
            extra_[i] = extra_[i + 1] = uint8_t(j);
            if (i != 0 && j < 17) ++j;
        }
        for (int i = 0, j = 0; i < 51; ++i) {
            base_[i] = uint32_t(j);
            j += 1 << extra_[i];
        }
        mainLen_.assign(656 + 8, 0);
        lenLen_.assign(250 + 8, 0);
    }

    bool decompress(std::vector<uint8_t>& out, size_t outLen, std::string& err) {
        if (!posSlots_) { err = "tamanho de janela LZX nao suportado"; return false; }
        out.assign(outLen, 0);
        size_t outPos = 0;
        const unsigned kFrame = 32768;
        if (!headerRead_) {
            int e8 = readBits(1);
            if (e8) { intelSize_ = (readBits(16) << 16) | readBits(16); }
            headerRead_ = true;
        }
        while (outPos < outLen) {
            unsigned frameSize = unsigned(std::min<size_t>(kFrame, outLen - outPos));
            long bytesTodo = long(framePosn_ + frameSize - windowPosn_);
            while (bytesTodo > 0) {
                if (blockRemaining_ == 0) {
                    if (blockType_ == 3 && (blockLength_ & 1)) { if (inPos_ < inLen_) ++inPos_; }  // alinhamento
                    if (!readBlockHeader(err)) return false;
                }
                long thisRun = long(std::min<uint32_t>(blockRemaining_, uint32_t(bytesTodo)));
                bytesTodo -= thisRun;
                blockRemaining_ -= uint32_t(thisRun);
                if (blockType_ == 1 || blockType_ == 2) {
                    while (thisRun > 0) {
                        int sym = huff(mainTable_, mainLen_);
                        if (sym < 0) { err = "codigo Huffman invalido (arvore principal)"; return false; }
                        if (sym < 256) {
                            window_[windowPosn_++] = uint8_t(sym);
                            --thisRun;
                            continue;
                        }
                        sym -= 256;
                        unsigned len = sym & 7;
                        if (len == 7) {
                            int f = huff(lenTable_, lenLen_);
                            if (f < 0) { err = "codigo Huffman invalido (comprimentos)"; return false; }
                            len += unsigned(f);
                        }
                        len += 2;
                        unsigned slot = unsigned(sym) >> 3;
                        uint32_t off;
                        if (slot > 2) {
                            unsigned ex = extra_[slot];
                            if (blockType_ == 1) {
                                off = slot != 3 ? base_[slot] - 2 + readMany(ex) : 1;
                            } else {
                                off = base_[slot] - 2;
                                if (ex > 3) {
                                    off += readMany(ex - 3) << 3;
                                    int a = huff(alignTable_, alignLen_);
                                    if (a < 0) { err = "codigo Huffman invalido (alinhado)"; return false; }
                                    off += uint32_t(a);
                                } else if (ex == 3) {
                                    int a = huff(alignTable_, alignLen_);
                                    if (a < 0) { err = "codigo Huffman invalido (alinhado)"; return false; }
                                    off += uint32_t(a);
                                } else if (ex > 0) {
                                    off += readMany(ex);
                                } else {
                                    off = 1;
                                }
                            }
                            r2_ = r1_; r1_ = r0_; r0_ = off;
                        } else if (slot == 0) {
                            off = r0_;
                        } else if (slot == 1) {
                            off = r1_; r1_ = r0_; r0_ = off;
                        } else {
                            off = r2_; r2_ = r0_; r0_ = off;
                        }
                        if (windowPosn_ + len > windowSize_) { err = "correspondencia ultrapassa a janela"; return false; }
                        for (unsigned k = 0; k < len; ++k) {
                            window_[windowPosn_] = window_[(windowPosn_ - off) & (windowSize_ - 1)];
                            ++windowPosn_;
                        }
                        thisRun -= long(len);
                    }
                    if (thisRun < 0) {
                        if (uint32_t(-thisRun) > blockRemaining_) { err = "sobreposicao de bloco invalida"; return false; }
                        blockRemaining_ -= uint32_t(-thisRun);
                    }
                } else if (blockType_ == 3) {
                    if (inPos_ + size_t(thisRun) > inLen_) { err = "bloco sem compressao truncado"; return false; }
                    std::memcpy(&window_[windowPosn_], in_ + inPos_, size_t(thisRun));
                    inPos_ += size_t(thisRun);
                    windowPosn_ += unsigned(thisRun);
                } else {
                    err = "tipo de bloco LZX invalido";
                    return false;
                }
            }
            // copia o quadro para a saida (com traducao E8, se ativada)
            std::memcpy(&out[outPos], &window_[framePosn_], frameSize);
            if (intelSize_ && frameNum_ < 32768 && frameSize > 10) e8(&out[outPos], frameSize, uint32_t(outPos));
            outPos += frameSize;
            ++frameNum_;
            framePosn_ += frameSize;
            if (windowPosn_ >= windowSize_) windowPosn_ = 0;
            if (framePosn_ >= windowSize_) framePosn_ = 0;
            // realinha o fluxo de bits em 16 bits
            if (bitsLeft_ > 0) ensure(16);
            if (bitsLeft_ & 15) remove(bitsLeft_ & 15);
        }
        return true;
    }

private:
    void ensure(int n) {
        while (bitsLeft_ < n) {
            uint32_t w = 0;
            if (inPos_ + 1 < inLen_) w = in_[inPos_] | (uint32_t(in_[inPos_ + 1]) << 8);
            inPos_ += 2;
            buf_ |= uint64_t(w) << (48 - bitsLeft_);
            bitsLeft_ += 16;
        }
    }
    uint32_t peek(int n) const { return n ? uint32_t(buf_ >> (64 - n)) : 0; }
    void remove(int n) { buf_ <<= n; bitsLeft_ -= n; }
    uint32_t readBits(int n) {
        if (!n) return 0;
        ensure(n);
        uint32_t v = peek(n);
        remove(n);
        return v;
    }
    // Leitura de muitos bits como no decodificador de referencia (buffer de 32 bits).
    uint32_t readMany(int n) {
        uint32_t v = 0;
        while (n > 0) {
            if (bitsLeft_ <= 16) ensure(bitsLeft_ + 16);
            int run = bitsLeft_ < n ? bitsLeft_ : n;
            v = (v << run) | peek(run);
            remove(run);
            n -= run;
        }
        return v;
    }

    // Tabela de decodificacao direta (16 bits) a partir dos comprimentos canonicos.
    static bool build(std::vector<uint16_t>& table, const std::vector<uint8_t>& lens, int n) {
        table.assign(65536, 0xFFFF);
        uint32_t code = 0;
        bool any = false;
        for (int bits = 1; bits <= 16; ++bits) {
            for (int s = 0; s < n; ++s) {
                if (lens[size_t(s)] != bits) continue;
                any = true;
                uint32_t start = code << (16 - bits), count = 1u << (16 - bits);
                if (start + count > 65536) return false;
                for (uint32_t k = 0; k < count; ++k) table[start + k] = uint16_t(s);
                ++code;
            }
            code <<= 1;
        }
        return true || any;
    }
    int huff(const std::vector<uint16_t>& table, const std::vector<uint8_t>& lens) {
        ensure(16);
        uint16_t s = table[peek(16)];
        if (s == 0xFFFF) return -1;
        remove(lens[s]);
        return s;
    }

    bool readLens(std::vector<uint8_t>& lens, int first, int last) {
        std::vector<uint8_t> pl(20);
        for (int i = 0; i < 20; ++i) pl[size_t(i)] = uint8_t(readBits(4));
        std::vector<uint16_t> pt;
        if (!build(pt, pl, 20)) return false;
        int x = first;
        while (x < last) {
            int z = huff(pt, pl);
            if (z < 0) return false;
            if (z == 17) {
                int y = int(readBits(4)) + 4;
                while (y-- && x < last) lens[size_t(x++)] = 0;
            } else if (z == 18) {
                int y = int(readBits(5)) + 20;
                while (y-- && x < last) lens[size_t(x++)] = 0;
            } else if (z == 19) {
                int y = int(readBits(1)) + 4;
                int zz = huff(pt, pl);
                if (zz < 0) return false;
                int v = lens[size_t(x)] - zz;
                if (v < 0) v += 17;
                while (y-- && x < last) lens[size_t(x++)] = uint8_t(v);
            } else {
                int v = lens[size_t(x)] - z;
                if (v < 0) v += 17;
                lens[size_t(x++)] = uint8_t(v);
            }
        }
        return true;
    }

    bool readBlockHeader(std::string& err) {
        blockType_ = readBits(3);
        uint32_t hi = readBits(16), lo = readBits(8);
        blockRemaining_ = blockLength_ = (hi << 8) | lo;
        if (blockType_ == 2) {
            alignLen_.assign(8, 0);
            for (int i = 0; i < 8; ++i) alignLen_[size_t(i)] = uint8_t(readBits(3));
            if (!build(alignTable_, alignLen_, 8)) { err = "arvore alinhada invalida"; return false; }
        }
        if (blockType_ == 1 || blockType_ == 2) {
            if (!readLens(mainLen_, 0, 256) || !readLens(mainLen_, 256, mainElements_)) { err = "arvore principal invalida"; return false; }
            if (!build(mainTable_, mainLen_, mainElements_)) { err = "arvore principal invalida"; return false; }
            if (!readLens(lenLen_, 0, 249)) { err = "arvore de comprimentos invalida"; return false; }
            if (!build(lenTable_, lenLen_, 249)) { err = "arvore de comprimentos invalida"; return false; }
        } else if (blockType_ == 3) {
            if (bitsLeft_ == 0) ensure(16);
            bitsLeft_ = 0;
            buf_ = 0;
            if (inPos_ + 12 > inLen_) { err = "cabecalho de bloco sem compressao truncado"; return false; }
            auto le = [this](size_t o) { return uint32_t(in_[o]) | (uint32_t(in_[o + 1]) << 8) | (uint32_t(in_[o + 2]) << 16) | (uint32_t(in_[o + 3]) << 24); };
            r0_ = le(inPos_); r1_ = le(inPos_ + 4); r2_ = le(inPos_ + 8);
            inPos_ += 12;
        } else {
            err = "tipo de bloco LZX invalido (" + std::to_string(blockType_) + ")";
            return false;
        }
        return true;
    }

    void e8(uint8_t* data, unsigned n, uint32_t curpos) {
        for (unsigned i = 0; i + 10 <= n; ++i) {
            if (data[i] != 0xE8) continue;
            int32_t abs = int32_t(data[i + 1] | (data[i + 2] << 8) | (data[i + 3] << 16) | (uint32_t(data[i + 4]) << 24));
            int32_t pos = int32_t(curpos + i);
            if (abs >= -pos && abs < int32_t(intelSize_)) {
                int32_t rel = abs >= 0 ? abs - pos : abs + int32_t(intelSize_);
                data[i + 1] = uint8_t(rel); data[i + 2] = uint8_t(rel >> 8);
                data[i + 3] = uint8_t(rel >> 16); data[i + 4] = uint8_t(rel >> 24);
            }
            i += 4;
        }
    }

    const uint8_t* in_;
    size_t inLen_, inPos_ = 0;
    uint64_t buf_ = 0;
    int bitsLeft_ = 0;
    std::vector<uint8_t> window_;
    unsigned windowSize_ = 0, windowPosn_ = 0, framePosn_ = 0, frameNum_ = 0;
    int posSlots_ = 0, mainElements_ = 0;
    uint8_t extra_[52] = {};
    uint32_t base_[52] = {};
    std::vector<uint8_t> mainLen_, lenLen_, alignLen_ = std::vector<uint8_t>(8, 0);
    std::vector<uint16_t> mainTable_, lenTable_, alignTable_;
    uint32_t blockType_ = 0, blockLength_ = 0, blockRemaining_ = 0;
    uint32_t r0_ = 1, r1_ = 1, r2_ = 1;
    bool headerRead_ = false;
    uint32_t intelSize_ = 0;
};

bool readFile(const std::string& p, std::vector<uint8_t>& d) {
    FILE* f = std::fopen(p.c_str(), "rb");
    if (!f) return false;
    std::fseek(f, 0, SEEK_END);
    long n = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    d.resize(size_t(n));
    bool ok = std::fread(d.data(), 1, d.size(), f) == d.size();
    std::fclose(f);
    return ok;
}

bool writeFile(const std::string& p, const uint8_t* d, size_t n) {
    FILE* f = std::fopen(p.c_str(), "wb");
    if (!f) return false;
    bool ok = std::fwrite(d, 1, n, f) == n;
    std::fclose(f);
    return ok;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "uso: neutrox_xex_unpack <arquivo.xex> <pasta_saida>\n");
        return 2;
    }
    std::vector<uint8_t> x;
    if (!readFile(argv[1], x) || x.size() < 0x18 || std::memcmp(x.data(), "XEX2", 4) != 0) {
        std::fprintf(stderr, "ERRO: nao e um XEX2 legivel\n");
        return 2;
    }
    uint32_t peOff = be32(&x[8]), secOff = be32(&x[0x10]), count = be32(&x[0x14]);
    uint32_t ffi = 0, resInfo = 0;
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t k = be32(&x[0x18 + i * 8]), v = be32(&x[0x18 + i * 8 + 4]);
        if (k == 0x000003FF) ffi = v;
        if (k == 0x000002FF) resInfo = v;
    }
    if (!ffi || secOff + 8 + 256 + 4 + 8 > x.size()) { std::fprintf(stderr, "ERRO: cabecalho incompleto\n"); return 2; }
    uint32_t imageSize = be32(&x[secOff + 4]);
    uint32_t loadAddr = be32(&x[secOff + 8 + 256 + 4 + 4]);
    uint16_t enc = be16(&x[ffi + 4]), comp = be16(&x[ffi + 6]);
    std::printf("arquivo: %s\nimagem: %u bytes em 0x%08X\ncriptografia: %u  compressao: %u\n", argv[1], imageSize, loadAddr, enc, comp);
    if (enc != 0) {
        std::printf("RECUSADO: o corpo do XEX esta criptografado. Abri-lo exigiria chaves do console "
                    "(contornar protecao), o que o Neutro X nao faz.\n");
        return 3;
    }
    std::vector<uint8_t> image;
    if (comp == 0) {
        image.assign(x.begin() + peOff, x.end());
        image.resize(imageSize, 0);
    } else if (comp == 1) {
        uint32_t infoSize = be32(&x[ffi]);
        uint32_t n = (infoSize - 8) / 8;
        size_t src = peOff;
        for (uint32_t i = 0; i < n; ++i) {
            uint32_t dsz = be32(&x[ffi + 8 + i * 8]), zsz = be32(&x[ffi + 8 + i * 8 + 4]);
            image.insert(image.end(), x.begin() + long(src), x.begin() + long(src + dsz));
            image.insert(image.end(), zsz, 0);
            src += dsz;
        }
        image.resize(imageSize, 0);
    } else if (comp == 2) {
        uint32_t windowSize = be32(&x[ffi + 8]);
        uint32_t blockSize = be32(&x[ffi + 12]);
        std::vector<uint8_t> lzx;
        size_t p = peOff;
        int blocks = 0;
        while (blockSize) {
            if (p + blockSize > x.size() || p + 24 > x.size()) { std::fprintf(stderr, "ERRO: bloco comprimido truncado\n"); return 2; }
            size_t next = p + blockSize;
            uint32_t nextSize = be32(&x[p]);
            size_t q = p + 24;  // pula (tamanho + SHA-1) do proximo bloco
            while (q + 2 <= next) {
                uint16_t chunk = be16(&x[q]);
                q += 2;
                if (!chunk) break;
                if (q + chunk > next) { std::fprintf(stderr, "ERRO: trecho comprimido invalido\n"); return 2; }
                lzx.insert(lzx.end(), x.begin() + long(q), x.begin() + long(q + chunk));
                q += chunk;
            }
            p = next;
            blockSize = nextSize;
            ++blocks;
        }
        unsigned wbits = 0;
        for (uint32_t t = windowSize; t > 1; t >>= 1) ++wbits;
        std::printf("blocos: %d  fluxo LZX: %zu bytes  janela: %u (2^%u)\n", blocks, lzx.size(), windowSize, wbits);
        Lzx dec(lzx.data(), lzx.size(), wbits);
        std::string err;
        if (!dec.decompress(image, imageSize, err)) { std::fprintf(stderr, "ERRO LZX: %s\n", err.c_str()); return 2; }
    } else {
        std::fprintf(stderr, "ERRO: compressao %u nao suportada (delta)\n", comp);
        return 2;
    }
    // Verificacao: imagem PE valida (MZ + assinatura PE)
    bool pe = image.size() > 0x40 && image[0] == 'M' && image[1] == 'Z';
    if (pe) {
        uint32_t lfanew = uint32_t(image[0x3C]) | (uint32_t(image[0x3D]) << 8) | (uint32_t(image[0x3E]) << 16) | (uint32_t(image[0x3F]) << 24);
        pe = lfanew + 4 < image.size() && std::memcmp(&image[lfanew], "PE\0\0", 4) == 0;
    }
    std::printf("verificacao PE: %s\n", pe ? "ok (MZ + PE)" : "FALHOU");
    if (!pe) return 2;
    std::filesystem::create_directories(std::filesystem::u8path(std::string(argv[2]) + "/recursos"));
    std::string out = std::string(argv[2]) + "/imagem.bin";
    if (!writeFile(out, image.data(), image.size())) { std::fprintf(stderr, "ERRO: nao foi possivel gravar %s\n", out.c_str()); return 2; }
    int nres = 0;
    if (resInfo) {
        uint32_t sz = be32(&x[resInfo]);
        for (uint32_t i = 0; i < (sz - 4) / 16; ++i) {
            const uint8_t* e = &x[resInfo + 4 + i * 16];
            char name[9] = {};
            std::memcpy(name, e, 8);
            uint32_t addr = be32(e + 8), rsz = be32(e + 12);
            if (addr < loadAddr || addr - loadAddr + rsz > image.size()) continue;
            std::string rp = std::string(argv[2]) + "/recursos/" + name + ".bin";
            if (writeFile(rp, &image[addr - loadAddr], rsz)) ++nres;
        }
    }
    std::printf("gravado: %s (%zu bytes), %d recursos\n", out.c_str(), image.size(), nres);
    return 0;
}
