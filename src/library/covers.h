// Neutro X - capas dos jogos.
//
// Fonte automatica: XboxUnity (o mesmo servico usado pelo Aurora), consultado
// so por Title ID, so para imagens/metadados, uma requisicao por vez:
//   GET http://xboxunity.net/Resources/Lib/CoverInfo.php?titleid=<TID>  (JSON)
//   GET http://xboxunity.net/Resources/Lib/Cover.php?size=large&cid=<id> (JPEG)
// (endpoints verificados em 11/11/2026; ver docs/BIBLIOTECA.md).
// Cada arquivo recebido e validado (tamanho, assinatura, decodificacao) antes
// de ir para <dados>/capas/<TID>.jpg (gravacao .part + rename). Sem rede, as
// capas ja salvas continuam funcionando.
// Importacao manual: <dados>/capas_importar/<TID>.jpg|png.
#pragma once
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <list>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include "graphics/image_decode.h"
#include "graphics/renderer.h"

namespace nx {

// Escolhe uma capa no JSON do CoverInfo.php: oficial primeiro, depois a mais bem avaliada.
// Retorna o CoverID ("" se nenhuma). 'count' recebe o total de capas listadas.
std::string pickCoverId(const std::string& json, int* count = nullptr);
// Frente da caixa: as capas do XboxUnity sao a arte completa (verso, lombada, frente);
// a frente e a parte direita. Imagens ja em pe sao usadas inteiras.
Image frontCover(const Image& full);
// Valida bytes de uma capa (assinatura + decodificacao + tamanho minimo).
bool validateCoverBytes(const std::vector<uint8_t>& bytes, std::string* why, Image* decoded = nullptr);

struct CoverResult {
    std::string gameId;
    uint32_t titleId = 0;
    bool ok = false;
    std::string file;     // nome em capas/ quando ok
    int source = 0;       // 1 XboxUnity, 2 importada
    std::string message;  // motivo ou resumo
    bool manual = false;  // pedido pelo usuario (mostra aviso)
};

class CoverService {
public:
    ~CoverService();
    void setDirs(const std::string& coverDir, const std::string& importDir);
    void setBaseUrl(const std::string& url) { baseUrl_ = url; }  // testes
    const std::string& coverDir() const { return coverDir_; }
    const std::string& importDir() const { return importDir_; }
    // Pede a capa de um jogo (force: baixa de novo mesmo se ja existir).
    void request(const std::string& gameId, uint32_t titleId, bool force, bool manual);
    bool busy();
    int pending();
    // Resultados prontos (thread principal).
    std::vector<CoverResult> takeResults();
    // Copia capas de capas_importar/ (nomes <TID>.jpg/.png) que passam na validacao.
    // Retorna Title IDs importados; 'notes' recebe os recusados e o motivo.
    std::vector<std::pair<uint32_t, std::string>> importFolder(std::vector<std::string>* notes);
    void stop();

private:
    struct Job { std::string gameId; uint32_t titleId; bool force, manual; };
    void loop();
    CoverResult fetch(const Job& j);

    std::string coverDir_, importDir_;
    std::string baseUrl_ = "http://xboxunity.net";
    std::mutex m_;
    std::condition_variable cv_;
    std::deque<Job> jobs_;
    std::vector<CoverResult> results_;
    std::thread th_;
    bool stop_ = false, working_ = false;
    std::atomic<bool> cancel_{false};
    std::map<uint32_t, int> failures_;  // nesta sessao (evita repetir pedidos que falharam)
};

// Texturas das capas: decodificacao em segundo plano + cache LRU limitado.
class CoverTextures {
public:
    static constexpr int kW = 256, kH = 360;
    ~CoverTextures();
    void setDir(const std::string& coverDir) { dir_ = coverDir; }
    // Textura pronta para o arquivo (ou kInvalidTexture, e agenda a decodificacao).
    TextureId get(IRenderer& r, const std::string& file);
    // Cria as texturas que ficaram prontas (chamar uma vez por quadro, thread principal).
    void pump(IRenderer& r);
    void invalidate(IRenderer& r, const std::string& file);  // arquivo trocado
    void releaseAll(IRenderer& r);
    int count() const { return int(tex_.size()); }

private:
    void loop();
    std::string dir_;
    std::map<std::string, TextureId> tex_;
    std::list<std::string> lru_;  // mais recente no inicio
    std::map<std::string, bool> failed_;
    std::mutex m_;
    std::condition_variable cv_;
    std::deque<std::string> want_;
    std::vector<std::pair<std::string, Image>> ready_;
    std::thread th_;
    bool stop_ = false;
    size_t limit_ = 40;
};

}  // namespace nx
