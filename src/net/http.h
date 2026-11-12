// Neutro X - cliente HTTP/1.1 minimo (GET), sobre sockets BSD.
// No console usa os sockets do xsdk (XAM); no Windows, Winsock.
// So http:// (sem TLS). Usado apenas para metadados e imagens de capas: o
// tamanho maximo da resposta e limitado e nada recebido e executado.
#pragma once
#include <atomic>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace nx {

struct HttpResponse {
    int status = 0;          // 0 = sem resposta (erro de rede)
    std::string error;       // motivo quando falhou
    std::map<std::string, std::string> headers;  // nomes em minusculas
    std::vector<uint8_t> body;
};

struct HttpOptions {
    size_t maxBytes = 4u * 1024u * 1024u;
    int timeoutMs = 10000;
    int maxRedirects = 3;
    std::string userAgent = "NeutroX/0.9 (Xbox 360 homebrew; capas)";
};

bool httpGet(const std::string& url, HttpResponse& out, const HttpOptions& opt = HttpOptions(),
             const std::atomic<bool>* cancel = nullptr);

// Partes de uma URL http://host[:porta]/caminho. false se nao for http.
bool parseHttpUrl(const std::string& url, std::string& host, int& port, std::string& path, std::string* why = nullptr);
// %XX para caracteres fora de [A-Za-z0-9-_.~]
std::string urlEncode(const std::string& s);

}  // namespace nx
