// Neutro X - decodificadores de imagem proprios (sem bibliotecas externas):
//   PNG (8 bits por canal, tons de cinza/RGB/RGBA/paleta, sem entrelacamento)
//   JPEG baseline (Huffman, 4:4:4 / 4:2:2 / 4:2:0 / cinza, marcadores de reinicio)
// Usados para capas e miniaturas de pacotes. Entradas invalidas falham com o
// motivo (nunca leem fora do buffer). JPEG progressivo nao e suportado.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace nx {

struct Image {
    int w = 0, h = 0;
    std::vector<uint32_t> px;  // 0xAARRGGBB
};

enum class ImageFormat { Unknown, Png, Jpeg };
ImageFormat sniffImage(const uint8_t* data, size_t size);

bool decodePng(const uint8_t* data, size_t size, Image& out, std::string* why = nullptr);
bool decodeJpeg(const uint8_t* data, size_t size, Image& out, std::string* why = nullptr);
bool decodeImage(const uint8_t* data, size_t size, Image& out, std::string* why = nullptr);

// Reduz/amplia para w x h (media por area ao reduzir, bilinear ao ampliar).
Image resizeImage(const Image& src, int w, int h);
// Recorta para preencher w x h mantendo a proporcao (corta as sobras do centro).
Image coverFit(const Image& src, int w, int h);

// zlib (inflate). Exposto para os testes.
bool zlibInflate(const uint8_t* data, size_t size, std::vector<uint8_t>& out, size_t maxOut, std::string* why = nullptr);

}  // namespace nx
