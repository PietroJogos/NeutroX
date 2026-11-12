// Neutro X - cliente HTTP minimo. Ver http.h.
#include "net/http.h"

#include <cstdio>
#include <cstring>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mutex>
using SockT = SOCKET;
static const SockT kBadSock = INVALID_SOCKET;
static void closeSock(SockT s) { closesocket(s); }
#else
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
using SockT = int;
static const SockT kBadSock = -1;
static void closeSock(SockT s) { close(s); }
#endif

namespace nx {

namespace {

#ifdef _WIN32
bool netInit() {
    static std::once_flag once;
    static bool ok = false;
    std::call_once(once, []() {
        WSADATA wd;
        ok = WSAStartup(MAKEWORD(2, 2), &wd) == 0;
    });
    return ok;
}
#else
bool netInit() { return true; }  // o xsdk inicia a rede no primeiro socket
#endif

std::string lower(std::string s) {
    for (char& c : s)
        if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
    return s;
}

std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

bool dechunk(const std::vector<uint8_t>& in, std::vector<uint8_t>& out, size_t maxBytes) {
    out.clear();
    size_t p = 0;
    for (;;) {
        size_t eol = p;
        while (eol + 1 < in.size() && !(in[eol] == '\r' && in[eol + 1] == '\n')) ++eol;
        if (eol + 1 >= in.size()) return false;
        unsigned long n = std::strtoul(std::string(in.begin() + long(p), in.begin() + long(eol)).c_str(), nullptr, 16);
        p = eol + 2;
        if (n == 0) return true;
        if (p + n > in.size() || out.size() + n > maxBytes) return false;
        out.insert(out.end(), in.begin() + long(p), in.begin() + long(p + n));
        p += n + 2;
    }
}

bool getOnce(const std::string& url, HttpResponse& out, const HttpOptions& opt, const std::atomic<bool>* cancel) {
    out = HttpResponse();
    std::string host, path;
    int port = 80;
    if (!parseHttpUrl(url, host, port, path, &out.error)) return false;
    if (!netInit()) { out.error = "rede indisponível (inicialização falhou)"; return false; }
    addrinfo hints;
    std::memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* res = nullptr;
    char portStr[8];
    std::snprintf(portStr, sizeof portStr, "%d", port);
    if (getaddrinfo(host.c_str(), portStr, &hints, &res) != 0 || !res) {
        out.error = "não foi possível resolver " + host + " (sem internet ou DNS)";
        return false;
    }
    SockT s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (s == kBadSock) {
        freeaddrinfo(res);
        out.error = "não foi possível criar o socket";
        return false;
    }
#ifdef _WIN32
    DWORD tv = DWORD(opt.timeoutMs);
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&tv), sizeof tv);
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&tv), sizeof tv);
#else
    timeval tv;
    tv.tv_sec = opt.timeoutMs / 1000;
    tv.tv_usec = (opt.timeoutMs % 1000) * 1000;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv);
#endif
    const int rc = connect(s, res->ai_addr, int(res->ai_addrlen));
    freeaddrinfo(res);
    if (rc != 0) {
        closeSock(s);
        out.error = "falha ao conectar em " + host + " (servidor fora do ar ou sem rede)";
        return false;
    }
    std::string req = "GET " + path + " HTTP/1.1\r\nHost: " + host + (port != 80 ? ":" + std::string(portStr) : "") +
                      "\r\nUser-Agent: " + opt.userAgent + "\r\nAccept: */*\r\nConnection: close\r\n\r\n";
    size_t sent = 0;
    while (sent < req.size()) {
        int n = int(send(s, req.data() + sent, int(req.size() - sent), 0));
        if (n <= 0) {
            closeSock(s);
            out.error = "falha ao enviar o pedido";
            return false;
        }
        sent += size_t(n);
    }
    std::vector<uint8_t> raw;
    char buf[16384];
    bool tooBig = false;
    for (;;) {
        if (cancel && cancel->load()) {
            closeSock(s);
            out.error = "cancelado";
            return false;
        }
        int n = int(recv(s, buf, int(sizeof buf), 0));
        if (n == 0) break;
        if (n < 0) {
            closeSock(s);
            out.error = raw.empty() ? "tempo esgotado sem resposta" : "conexão interrompida";
            return false;
        }
        raw.insert(raw.end(), buf, buf + n);
        if (raw.size() > opt.maxBytes + 64 * 1024) { tooBig = true; break; }
    }
    closeSock(s);
    if (tooBig) { out.error = "resposta grande demais"; return false; }
    // cabecalho
    size_t he = 0;
    for (; he + 3 < raw.size(); ++he)
        if (raw[he] == '\r' && raw[he + 1] == '\n' && raw[he + 2] == '\r' && raw[he + 3] == '\n') break;
    if (he + 3 >= raw.size()) { out.error = "resposta HTTP inválida"; return false; }
    const std::string head(raw.begin(), raw.begin() + long(he));
    size_t lineEnd = head.find("\r\n");
    const std::string status = head.substr(0, lineEnd);
    if (status.compare(0, 5, "HTTP/") != 0 || status.size() < 12) { out.error = "resposta HTTP inválida"; return false; }
    out.status = std::atoi(status.c_str() + 9);
    size_t pos = lineEnd == std::string::npos ? head.size() : lineEnd + 2;
    while (pos < head.size()) {
        size_t e = head.find("\r\n", pos);
        if (e == std::string::npos) e = head.size();
        const std::string line = head.substr(pos, e - pos);
        size_t c = line.find(':');
        if (c != std::string::npos) out.headers[lower(trim(line.substr(0, c)))] = trim(line.substr(c + 1));
        pos = e + 2;
    }
    std::vector<uint8_t> body(raw.begin() + long(he + 4), raw.end());
    auto te = out.headers.find("transfer-encoding");
    if (te != out.headers.end() && lower(te->second).find("chunked") != std::string::npos) {
        if (!dechunk(body, out.body, opt.maxBytes)) { out.error = "resposta em partes inválida"; return false; }
    } else {
        out.body.swap(body);
        auto cl = out.headers.find("content-length");
        if (cl != out.headers.end()) {
            const size_t want = size_t(std::strtoul(cl->second.c_str(), nullptr, 10));
            if (out.body.size() < want) { out.error = "resposta incompleta"; return false; }
            out.body.resize(want);
        }
    }
    if (out.body.size() > opt.maxBytes) { out.error = "resposta grande demais"; return false; }
    return true;
}

}  // namespace

bool parseHttpUrl(const std::string& url, std::string& host, int& port, std::string& path, std::string* why) {
    const std::string l = lower(url.substr(0, 8));
    if (l.compare(0, 8, "https://") == 0) {
        if (why) *why = "HTTPS não é suportado por este cliente";
        return false;
    }
    if (l.compare(0, 7, "http://") != 0) {
        if (why) *why = "URL inválida";
        return false;
    }
    std::string rest = url.substr(7);
    size_t slash = rest.find('/');
    std::string hp = rest.substr(0, slash);
    path = slash == std::string::npos ? "/" : rest.substr(slash);
    port = 80;
    size_t colon = hp.find(':');
    if (colon != std::string::npos) {
        port = std::atoi(hp.c_str() + colon + 1);
        hp = hp.substr(0, colon);
    }
    host = hp;
    if (host.empty() || port <= 0 || port > 65535) {
        if (why) *why = "URL inválida";
        return false;
    }
    return true;
}

std::string urlEncode(const std::string& s) {
    std::string o;
    for (unsigned char c : s) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~')
            o.push_back(char(c));
        else {
            char b[4];
            std::snprintf(b, sizeof b, "%%%02X", c);
            o += b;
        }
    }
    return o;
}

bool httpGet(const std::string& url, HttpResponse& out, const HttpOptions& opt, const std::atomic<bool>* cancel) {
    std::string cur = url;
    for (int hop = 0; hop <= opt.maxRedirects; ++hop) {
        if (!getOnce(cur, out, opt, cancel)) return false;
        if (out.status == 301 || out.status == 302 || out.status == 303 || out.status == 307 || out.status == 308) {
            auto loc = out.headers.find("location");
            if (loc == out.headers.end()) { out.error = "redirecionamento sem destino"; return false; }
            std::string next = loc->second;
            if (!next.empty() && next[0] == '/') {
                std::string host, path;
                int port = 80;
                parseHttpUrl(cur, host, port, path);
                next = "http://" + host + (port != 80 ? ":" + std::to_string(port) : "") + next;
            }
            cur = next;
            continue;
        }
        return true;
    }
    out.error = "redirecionamentos demais";
    return false;
}

}  // namespace nx
