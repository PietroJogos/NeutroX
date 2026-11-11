// Neutro X - conteudo de demonstracao compartilhado pelos dashboards
// (jogos, amigos, midias). Tudo ficticio e original; nada e executado.
#pragma once
#include <string>
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

// Linha generica de painel/lista informativa (usada pelos dashboards).
struct InfoRow {
    int icon = 0;  // valor de nx::Icon
    std::string title, sub, desc;
    float progress = -1;  // 0..1 desenha barra
    int artSeed = 0;      // != 0: arte no painel lateral
    Color c1, c2;
    int action = 0;       // codigo especifico de cada dashboard
};

// Perfil local ficticio exibido nas interfaces.
struct DemoProfile {
    const char* gamertag;
    const char* motto;
    const char* zone;
    int points;
};
const DemoProfile& demoProfile();

}  // namespace nx
