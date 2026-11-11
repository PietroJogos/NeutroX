// Neutro X - conteudo de demonstracao compartilhado pelos dashboards
// (jogos, amigos, midias). Tudo ficticio e original; nada e executado.
#pragma once
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

const DemoGame* demoGames(int& count);

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

// Perfil local ficticio exibido nas interfaces.
struct DemoProfile {
    const char* gamertag;
    const char* motto;
    const char* zone;
    int points;
};
const DemoProfile& demoProfile();

}  // namespace nx
