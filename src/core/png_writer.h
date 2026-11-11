// Neutro X - gravacao de capturas de tela em PNG (sem dependencias externas).
#pragma once
#include <cstdint>
#include <string>

namespace nx {

// pixels: w*h valores 0xAARRGGBB. Grava RGB de 8 bits (deflate sem compressao).
bool writePng(const std::string& path, int w, int h, const uint32_t* pixels);

}  // namespace nx
