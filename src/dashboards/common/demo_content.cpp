// Neutro X - conteudo de demonstracao. Titulos e textos ficticios criados para o projeto.
#include "dashboards/common/demo_content.h"

namespace nx {

namespace {
#define NX_N(a) int(sizeof(a) / sizeof((a)[0]))

const DemoGame kGames[] = {
    {"Corrida Neutra", "Corrida", "2026", "Corridas em pistas que mudam de cor a cada volta.", 101,
     Color::hex(0xF0C020), Color::hex(0x6A4A10), 650, 1000, false},
    {"Planeta Azul", "Aventura", "2026", "Explore um planeta oceânico e catalogue suas criaturas.", 102,
     Color::hex(0x3AA0D8), Color::hex(0x10305A), 420, 1000, false},
    {"Torre de Blocos", "Quebra-cabeça", "2026", "Empilhe blocos sem deixar a torre cair.", 103,
     Color::hex(0xE85A2A), Color::hex(0x5A1A0A), 200, 200, true},
    {"Ecos do Abismo", "Ação", "2026", "Uma descida por cavernas guiada apenas pelo som.", 104,
     Color::hex(0x5A6070), Color::hex(0x15181E), 90, 1000, false},
    {"Pixel Fazenda", "Simulação", "2026", "Plante, colha e cuide de uma fazenda em pixels.", 105,
     Color::hex(0x8CC63A), Color::hex(0x2E5A10), 130, 200, true},
    {"Órbita Zero", "Arcade", "2026", "Desvie de asteroides em órbita usando a gravidade.", 106,
     Color::hex(0x9A5CD6), Color::hex(0x24103A), 160, 200, true},
    {"Lâmina de Vento", "Ação", "2026", "Duelos rápidos sobre telhados em dias de ventania.", 107,
     Color::hex(0xD9363E), Color::hex(0x3A0A0E), 310, 1000, false},
    {"Cidade Aberta", "Aventura", "2026", "Uma cidade inteira para explorar de bicicleta.", 108,
     Color::hex(0x60A040), Color::hex(0x203A20), 0, 1000, false},
};

const DemoFriend kFriends[] = {
    {"Amigo Exemplo 1", "Offline"},
    {"Amigo Exemplo 2", "Offline"},
    {"Amigo Exemplo 3", "Offline"},
    {"Amigo Exemplo 4", "Offline"},
};

const DemoMedia kMusic[] = {
    {"Faixa de Demonstração 01", "Neutro X · 3:12"},
    {"Faixa de Demonstração 02", "Neutro X · 2:47"},
    {"Faixa de Demonstração 03", "Neutro X · 4:05"},
};
const DemoMedia kVideos[] = {
    {"Vídeo de Demonstração A", "720p · 1:30"},
    {"Vídeo de Demonstração B", "720p · 0:45"},
};
const DemoMedia kPictures[] = {
    {"Paisagem 01", "1280 × 720"},
    {"Paisagem 02", "1280 × 720"},
    {"Paisagem 03", "1280 × 720"},
};

const DemoProfile kProfile = {"Jogador", "Jogando offline com o Neutro X", "Recreativa", 1910};
}  // namespace

const DemoGame* demoGames(int& n) { n = NX_N(kGames); return kGames; }
const DemoFriend* demoFriends(int& n) { n = NX_N(kFriends); return kFriends; }
const DemoMedia* demoMusic(int& n) { n = NX_N(kMusic); return kMusic; }
const DemoMedia* demoVideos(int& n) { n = NX_N(kVideos); return kVideos; }
const DemoMedia* demoPictures(int& n) { n = NX_N(kPictures); return kPictures; }
const DemoProfile& demoProfile() { return kProfile; }

}  // namespace nx
