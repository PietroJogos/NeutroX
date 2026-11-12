// Neutro X - conteudo compartilhado pelos dashboards.
//
// JOGOS: vem da biblioteca REAL (Meus Jogos). demoGames() devolve um retrato da
// biblioteca publicado pelo App ao abrir cada interface (pode ser vazio: nenhum
// jogo de teste e mostrado como instalado). Amigos e midias continuam sendo
// conteudo ilustrativo, sempre identificado como tal.
#pragma once
#include <string>
#include <vector>
#include "core/types.h"

namespace nx {

struct DemoGame {
    const char* title;
    const char* genre;
    const char* year;
    const char* description;
    int artSeed;
    Color c1, c2;
    int points;      // pontos obtidos (demonstracao)
    int maxPoints;
    bool arcade;
};

// Jogos da biblioteca real (retrato atual). count pode ser 0.
const DemoGame* demoGames(int& count);
// Id da entrada da biblioteca do i-esimo jogo de demoGames() ("" se fora do intervalo).
std::string libraryGameId(int index);
// Muda a cada publicacao (as interfaces refazem listas montadas a partir dos jogos).
unsigned libraryGamesVersion();

struct GameEntry;
// Publica o retrato da biblioteca usado pelos dashboards (chamado pelo App).
void publishLibraryGames(const std::vector<const GameEntry*>& games);

struct DemoFriend {
    const char* name;
    const char* status;
};
const DemoFriend* demoFriends(int& count);

struct DemoMedia {
    const char* title;
    const char* detail;
};
const DemoMedia* demoMusic(int& count);
const DemoMedia* demoVideos(int& count);
const DemoMedia* demoPictures(int& count);

// Linha generica de painel/lista informativa (usada pelos dashboards).
struct InfoRow {
    int icon = 0;  // valor de nx::Icon
    std::string title, sub, desc;
    float progress = -1;  // 0..1 desenha barra
    int artSeed = 0;      // != 0: arte no painel lateral
    Color c1, c2;
    int action = 0;       // codigo especifico de cada dashboard
    std::string gameId;   // entrada da biblioteca (linhas de jogo)
};

// Perfil exibido nas interfaces: o perfil REAL conectado no console (nome lido
// pelo XAM) ou, sem perfil, um aviso. Gamerscore, lema e zona NAO sao lidos
// (points = -1 => "nao lido").
struct DemoProfile {
    const char* gamertag;
    const char* motto;
    const char* zone;
    int points;
    bool real;  // true = perfil conectado lido do console
};
const DemoProfile& demoProfile();
// Publicado pelo App quando o perfil muda.
void publishProfile(bool supported, bool signedIn, const std::string& gamertag);
std::string profilePointsText(const DemoProfile& p, bool shortForm = false);

}  // namespace nx
