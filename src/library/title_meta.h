// Neutro X - metadados reais de titulos do Xbox 360 (somente leitura).
//
// XEX2 (jogos extraidos, default.xex): so o cabecalho em texto claro e lido
// (Execution ID: Title ID, Media ID, versao, discos; flags do modulo; formato).
// Nada da imagem criptografada/comprimida e acessado e nada e executado.
//
// Pacotes STFS ("CON ", "LIVE", "PIRS"; GOD e XBLA): so o cabecalho publico do
// pacote (tipo de conteudo, Title ID, nome exibido, miniatura PNG).
//
// Um arquivo so vira jogo se o cabecalho for valido e tiver Title ID: um .xex
// qualquer (DLL, patch, modulo sem Execution ID) e recusado com o motivo.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace nx {

struct XexMeta {
    bool valid = false;
    std::string error;
    uint32_t moduleFlags = 0;
    bool titleModule = false;  // bit TITLE das flags do modulo
    bool dll = false, patch = false;
    bool hasExecutionId = false;
    uint32_t titleId = 0, mediaId = 0, version = 0, baseVersion = 0;
    uint8_t discNumber = 0, discCount = 0;
    uint16_t encryption = 0, compression = 0;  // FILE_FORMAT_INFO (0 = nenhuma)
    std::string originalName;                  // nome PE original (ex.: "halo3.exe")
};

// Analisa o cabecalho XEX2 em memoria (a partir do inicio do arquivo).
bool parseXexHeader(const uint8_t* data, size_t size, XexMeta& out);
// Le so o cabecalho do arquivo (no maximo 64 KB).
bool readXexMeta(const std::string& path, XexMeta& out);

// Tipos de conteudo STFS que a biblioteca reconhece.
enum : uint32_t {
    kContentGamesOnDemand = 0x00007000,
    kContentArcadeTitle = 0x000D0000,
    kContentInstalledGame = 0x00004000,
    kContentXboxOriginal = 0x00005000,
};

struct StfsMeta {
    bool valid = false;
    std::string error;
    std::string magic;  // "CON ", "LIVE" ou "PIRS"
    uint32_t contentType = 0;
    uint32_t titleId = 0, mediaId = 0, version = 0;
    std::string displayName;  // UTF-8
    std::string titleName;    // UTF-8
    std::vector<uint8_t> thumbnail;  // PNG (pode estar vazio)
};

bool parseStfsHeader(const uint8_t* data, size_t size, StfsMeta& out);
bool readStfsMeta(const std::string& path, StfsMeta& out);
// true se os 4 primeiros bytes forem de um pacote STFS.
bool looksLikeStfs(const uint8_t* data, size_t size);

std::string hex8(uint32_t v);  // "4D5307E6"
bool parseHex8(const std::string& s, uint32_t& out);

}  // namespace nx
