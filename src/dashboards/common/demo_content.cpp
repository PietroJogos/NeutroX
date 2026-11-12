// Neutro X - conteudo de demonstracao. Titulos e textos ficticios criados para o projeto.
#include "dashboards/common/demo_content.h"

#include <deque>
#include "library/library.h"
#include "library/title_meta.h"

namespace nx {

namespace {
#define NX_N(a) int(sizeof(a) / sizeof((a)[0]))

// Jogos ficticios antigos: mantidos so como referencia de estilo; NAO aparecem mais
// nas interfaces (a lista de jogos vem da biblioteca real).
[[maybe_unused]] const DemoGame kGames[] = {
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

std::string g_gamertag = "Sem perfil";
DemoProfile g_profile = {"Sem perfil", "não lido pelo Neutro X", "não lida", -1, false};
unsigned g_libVersion = 1;

std::vector<DemoGame> g_lib;
std::deque<std::string> g_strings;  // mantem os textos vivos enquanto o retrato existir
std::vector<std::string> g_ids;

const char* keep(const std::string& s) {
    g_strings.push_back(s);
    return g_strings.back().c_str();
}
}  // namespace

void publishLibraryGames(const std::vector<const GameEntry*>& games) {
    std::vector<DemoGame> lib;
    std::deque<std::string> strings;
    g_strings.swap(strings);  // os antigos so saem depois de montar os novos
    g_ids.clear();
    for (const GameEntry* g : games) {
        DemoGame d{};
        d.title = keep(g->displayName());
        d.genre = keep(g->kind == GameKind::Xbla ? "Arcade" : g->kind == GameKind::God ? "Games on Demand" : g->kind == GameKind::App ? "Aplicativo" : "Jogo");
        d.year = keep("Title ID " + hex8(g->titleId));
        d.description = keep(std::string(gameKindName(g->kind)) + (g->available ? "" : " · indisponível") + " · " + g->path);
        // Arte provisoria deterministica (a capa real, quando houver, aparece em Meus Jogos).
        const uint32_t h = g->titleId * 2654435761u;
        d.artSeed = int(1000 + (h % 90000));
        d.c1 = Color(uint8_t(60 + (h >> 8) % 160), uint8_t(60 + (h >> 16) % 160), uint8_t(60 + (h >> 24) % 160));
        d.c2 = Color(uint8_t(d.c1.r / 4), uint8_t(d.c1.g / 4), uint8_t(d.c1.b / 4));
        d.points = 0;
        d.maxPoints = 0;
        d.arcade = g->kind == GameKind::Xbla;
        lib.push_back(d);
        g_ids.push_back(g->id);
    }
    g_lib.swap(lib);
    ++g_libVersion;
}

unsigned libraryGamesVersion() { return g_libVersion; }

void publishProfile(bool supported, bool signedIn, const std::string& gamertag) {
    g_gamertag = !supported ? "Prévia (sem perfil)" : signedIn ? gamertag : "Nenhum perfil conectado";
    g_profile.gamertag = g_gamertag.c_str();
    g_profile.real = supported && signedIn;
}

std::string profilePointsText(const DemoProfile& p, bool shortForm) {
    if (p.points < 0) return shortForm ? "— P" : "Gamerscore não lido";
    return std::to_string(p.points) + (shortForm ? " P" : " pontos");
}

std::string libraryGameId(int i) { return i >= 0 && i < int(g_ids.size()) ? g_ids[size_t(i)] : ""; }

const DemoGame* demoGames(int& n) {
    n = int(g_lib.size());
    return g_lib.empty() ? nullptr : g_lib.data();
}
const DemoFriend* demoFriends(int& n) { n = NX_N(kFriends); return kFriends; }
const DemoMedia* demoMusic(int& n) { n = NX_N(kMusic); return kMusic; }
const DemoMedia* demoVideos(int& n) { n = NX_N(kVideos); return kVideos; }
const DemoMedia* demoPictures(int& n) { n = NX_N(kPictures); return kPictures; }
const DemoProfile& demoProfile() { return g_profile; }

}  // namespace nx
