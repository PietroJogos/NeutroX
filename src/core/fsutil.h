// Neutro X - utilidades de arquivo portaveis (PC e Xbox 360/xsdk).
// Caminhos usam '/' (o xsdk aceita '/' e '\'); unidades do console como "hdd:/".
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace nx {

struct DirEntry {
    std::string name;
    bool isDir = false;
    uint64_t size = 0;
};

bool fileExists(const std::string& path);  // arquivo comum
bool dirExists(const std::string& path);
// Le ate maxBytes; false se nao existir/erro ou se for maior que maxBytes.
bool readFileBytes(const std::string& path, std::vector<uint8_t>& out, size_t maxBytes);
// Le os primeiros 'count' bytes a partir de 'offset' (sem ler o arquivo inteiro).
bool readFileRange(const std::string& path, uint64_t offset, size_t count, std::vector<uint8_t>& out);
uint64_t fileSize(const std::string& path);
// Grava de forma segura: <path>.tmp -> fflush -> rename sobre o destino.
bool writeFileAtomic(const std::string& path, const void* data, size_t bytes);
bool writeFileAtomic(const std::string& path, const std::string& text);
// Lista uma pasta (sem "." e ".."); false se nao for pasta. Ordena por nome (sem diferenciar maiusculas).
bool listDir(const std::string& path, std::vector<DirEntry>& out, std::string* error = nullptr);
bool makeDirs(const std::string& path);

std::string toLowerAscii(std::string s);
bool endsWithNoCase(const std::string& s, const std::string& suffix);
// Junta "a/" + "b" com exatamente um separador.
std::string joinPath(const std::string& dir, const std::string& name);
// Normaliza separadores para '/', remove '/' duplicados e o '/' final (exceto na raiz "hdd:/").
std::string normalizePath(const std::string& p);
std::string parentPath(const std::string& p);  // "hdd:/Games/X" -> "hdd:/Games"
std::string baseName(const std::string& p);    // "hdd:/Games/X/default.xex" -> "default.xex"

}  // namespace nx
