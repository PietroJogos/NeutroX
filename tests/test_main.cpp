// Neutro X - testes automatizados (plataforma headless, sem janela).
//
// Cobre os testes obrigatorios executaveis no PC: inicializacao, controle,
// selecao das cinco interfaces, navegacao, troca repetida, memoria, taxa de
// quadros (tempo de CPU), integridade das configuracoes, recuperacao de erros
// e encerramento seguro. Tambem grava capturas PNG para inspecao visual.
//
// Uso: neutrox_tests [pasta_de_saida]
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <stdexcept>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <new>
#include <string>
#include <vector>

#include "audio/audio.h"
#include "core/app.h"
#include "core/log.h"
#include "core/png_writer.h"
#include "graphics/font.h"
#include "graphics/soft_renderer.h"
#include "graphics/ui.h"
#include "input/input.h"
#include "platform/headless/headless_platform.h"
#include "settings/settings.h"

// ------------------------------------------------ contador de memoria
static std::atomic<long long> g_liveBytes{0};
static std::atomic<long long> g_liveAllocs{0};
void* operator new(std::size_t n) {
    void* p = std::malloc(n + 16);
    if (!p) throw std::bad_alloc();
    *static_cast<std::size_t*>(p) = n;
    g_liveBytes += (long long)n;
    g_liveAllocs++;
    return static_cast<char*>(p) + 16;
}
void operator delete(void* p) noexcept {
    if (!p) return;
    char* b = static_cast<char*>(p) - 16;
    g_liveBytes -= (long long)*reinterpret_cast<std::size_t*>(b);
    g_liveAllocs--;
    std::free(b);
}
void operator delete(void* p, std::size_t) noexcept { operator delete(p); }
void* operator new[](std::size_t n) { return operator new(n); }
void operator delete[](void* p) noexcept { operator delete(p); }
void operator delete[](void* p, std::size_t) noexcept { operator delete(p); }

using namespace nx;

// ------------------------------------------------------- mini framework
static int g_pass = 0, g_fail = 0;
static std::string g_current;
#define CHECK(cond)                                                                              \
    do {                                                                                         \
        if (cond) ++g_pass;                                                                      \
        else { ++g_fail; std::printf("  FALHOU [%s] %s:%d: %s\n", g_current.c_str(), __FILE__, __LINE__, #cond); } \
    } while (0)

static void section(const char* name) {
    g_current = name;
    std::printf("== %s\n", name);
}

static std::string g_out = "test_output/";

// ------------------------------------------------------------ helpers
struct Harness {
    HeadlessPlatform plat;
    App app;
    explicit Harness(const std::string& dataDir, int w = 1280, int h = 720) : plat(w, h, dataDir), app(plat) {}
    void step(uint32_t buttons = 0, int frames = 1, float dt = 1.0f / 60.0f) {
        for (int i = 0; i < frames; ++i) {
            RawPadState s;
            s.connected = true;
            s.buttons = buttons;
            plat.setInput(s);
            plat.advance(dt);
            app.frame(s, dt);
        }
    }
    // Pressiona e solta um botao.
    void tap(Button b) {
        step(bit(b), 1);
        step(0, 1);
    }
    void idle(float seconds) { step(0, int(seconds * 60)); }
    void waitIdle() {
        for (int i = 0; i < 600 && app.busy(); ++i) step(0, 1);
        step(0, 2);
    }
    void shot(const std::string& name) {
        SoftRenderer& r = plat.soft();
        writePng(g_out + name + ".png", r.pixelWidth(), r.pixelHeight(), r.pixels());
    }
};

static std::string tempDir(const char* name) {
    std::string d = g_out + name + "/";
#ifdef _WIN32
    std::string cmd = "mkdir \"" + d + "\" >nul 2>nul";
    for (char& c : cmd) if (c == '/') c = '\\';
#else
    std::string cmd = "mkdir -p '" + d + "'";
#endif
    std::system(cmd.c_str());
    std::remove((d + "neutrox.ini").c_str());
    return d;
}

static double nowMs() {
    using namespace std::chrono;
    return duration<double, std::milli>(steady_clock::now().time_since_epoch()).count();
}

// ================================================================ testes

static void testFont() {
    section("Fonte propria");
    StrokeFont& f = StrokeFont::instance();
    for (char c = 32; c < 127; ++c) {
        if (c == '^' || c == '`' || c == '{' || c == '}' || c == '~') continue;  // nao usados na interface
        if (!f.hasGlyph(uint32_t(c))) std::printf("  sem glifo: '%c'\n", c);
        CHECK(f.hasGlyph(uint32_t(c)));
    }
    const char* pt = "áàâãéêíóôõúçÁÀÂÃÉÊÍÓÔÕÚÇ×·–—°";
    const char* p = pt;
    while (uint32_t cp = utf8Next(p)) CHECK(f.hasGlyph(cp));
    const GlyphBitmap& g = f.glyph('A', 32, FontWeight::Regular);
    int ink = 0;
    for (uint8_t a : g.alpha) ink += a > 128;
    CHECK(g.w > 0 && g.h > 0 && ink > 20);
    CHECK(f.advance('W', 32, FontWeight::Regular) > f.advance('i', 32, FontWeight::Regular));
}

static void testRenderer() {
    section("Renderizador em software");
    SoftRenderer r(1280, 720);
    r.beginFrame();
    r.clear(Color(10, 20, 30));
    CHECK(r.pixels()[0] == Color(10, 20, 30).argb());
    r.fillRect(Rectf{100, 100, 50, 50}, Color(255, 0, 0));
    CHECK(r.pixels()[120 * 1280 + 120] == Color(255, 0, 0).argb());
    CHECK(r.pixels()[99 * 1280 + 120] == Color(10, 20, 30).argb());
    // Alfa global e recorte
    r.pushClip(Rectf{0, 0, 10, 10});
    r.fillRect(Rectf{0, 0, 100, 100}, Color(0, 255, 0));
    r.popClip();
    CHECK(r.pixels()[5 * 1280 + 5] == Color(0, 255, 0).argb());
    CHECK(r.pixels()[50 * 1280 + 50] == Color(10, 20, 30).argb());
    r.pushAlpha(0.5f);
    r.fillRect(Rectf{200, 200, 10, 10}, Color(255, 255, 255));
    r.popAlpha();
    uint32_t mid = r.pixels()[205 * 1280 + 205];
    int red = int((mid >> 16) & 0xFF);
    CHECK(red > 120 && red < 140);
    // Canto arredondado com antialiasing: pixel do canto externo nao pintado.
    r.fillRoundRect(Rectf{400, 400, 100, 100}, 30, Color(0, 0, 255));
    CHECK(r.pixels()[401 * 1280 + 401] == Color(10, 20, 30).argb());
    CHECK(r.pixels()[450 * 1280 + 450] == Color(0, 0, 255).argb());
    // Poligono convexo
    Vec2 tri[3] = {{600, 100}, {700, 300}, {500, 300}};
    r.fillConvex(tri, 3, Color(255, 255, 0));
    CHECK(r.pixels()[250 * 1280 + 600] == Color(255, 255, 0).argb());
    // Texturas: criacao, desenho e contabilidade.
    std::vector<uint32_t> px(64 * 64, 0xFF336699u);
    TextureId t = r.createTexture(64, 64, px.data());
    CHECK(r.textureCount() == 1 && r.textureBytes() == 64 * 64 * 4);
    r.drawTexture(t, Rectf{800, 100, 128, 128});
    CHECK(r.pixels()[160 * 1280 + 860] == 0xFF336699u);
    r.destroyTexture(t);
    CHECK(r.textureCount() == 0 && r.textureBytes() == 0);
    r.drawTexture(t, Rectf{0, 0, 10, 10});  // id invalido: nao deve travar
    // Texto
    TextStyle st;
    st.size = 32;
    CHECK(r.measureText("Neutro X", st) > 80);
    r.drawText("Olá, Neutro X — ação", 50, 600, st);
    st.rotated90 = true;
    r.drawText("xbox live", 1200, 300, st);

    // Resolucoes alternativas: 1080p (escala 1.5) e 4:3 (com barras).
    SoftRenderer hd(1920, 1080);
    CHECK(std::fabs(hd.scale() - 1.5f) < 1e-4f);
    hd.beginFrame();
    hd.fillRect(Rectf{0, 0, 1280, 720}, Color(255, 0, 0));
    CHECK(hd.pixels()[1079 * 1920 + 1919] == Color(255, 0, 0).argb());
    SoftRenderer sd(640, 480);
    sd.beginFrame();
    sd.clear(Color(255, 255, 255));
    CHECK(sd.pixels()[0] == 0xFF000000u);                    // barra superior (letterbox)
    CHECK(sd.pixels()[240 * 640 + 320] == 0xFFFFFFFFu);      // area util
}

static void testInput() {
    section("Controle: repeticao, analogico, desconexao");
    InputMapper m;
    RawPadState s;
    s.connected = true;
    // borda de pressionamento
    s.buttons = bit(Button::A);
    InputFrame f = m.update(s, 0.016f);
    CHECK(f.press(Button::A));
    f = m.update(s, 0.016f);
    CHECK(!f.press(Button::A) && f.down(Button::A));  // A nao repete
    // repeticao do direcional: segurar 1 s
    s.buttons = bit(Button::Down);
    int presses = 0;
    for (int i = 0; i < 60; ++i) presses += m.update(s, 1.0f / 60).press(Button::Down);
    // 1 (inicial) + repeticoes a partir de 0.42 s a cada 0.11 s => 1 + 6 = 7 (+-1)
    CHECK(presses >= 6 && presses <= 8);
    // quadro muito longo nao gera rajada
    presses = 0;
    for (int i = 0; i < 2; ++i) presses += m.update(s, 1.0f).press(Button::Down);
    CHECK(presses <= 2);
    s.buttons = 0;
    m.update(s, 0.016f);
    // analogico com histerese
    s.ly = 0.6f;
    CHECK(m.update(s, 0.016f).press(Button::Up));
    s.ly = 0.45f;  // entre os limiares: continua segurado, sem nova borda
    f = m.update(s, 0.016f);
    CHECK(f.down(Button::Up) && !f.press(Button::Up));
    s.ly = 0.2f;
    CHECK(!m.update(s, 0.016f).down(Button::Up));
    // desconexao: sem eventos, sem travar
    s.buttons = bit(Button::A);
    s.connected = false;
    f = m.update(s, 0.016f);
    CHECK(f.disconnectedNow && !f.press(Button::A));
    s.connected = true;
    f = m.update(s, 0.016f);
    CHECK(f.connectedNow && f.press(Button::A));
    // reset suprime botoes ainda segurados
    m.reset();
    CHECK(!m.update(s, 0.016f).press(Button::A));
    s.buttons = 0;
    m.update(s, 0.016f);
    s.buttons = bit(Button::A);
    CHECK(m.update(s, 0.016f).press(Button::A));
}

static void testSettings() {
    section("Configuracoes: integridade");
    Settings s;
    s.defaultDashboard = "metro_v2";
    s.sfxVolume = 30;
    s.animations = false;
    s.animIntensity = AnimIntensity::High;
    s.accentTheme = 3;
    s.showFps = true;
    Settings back;
    CHECK(parseSettings(serializeSettings(s), back) == LoadResult::Ok);
    CHECK(back.defaultDashboard == "metro_v2" && back.sfxVolume == 30 && !back.animations &&
          back.animIntensity == AnimIntensity::High && back.accentTheme == 3 && back.showFps);
    // Valores invalidos sao rejeitados individualmente.
    Settings bad;
    LoadResult lr = parseSettings("sfx_volume=999\ndefault_dashboard=../../etc\nanimations=talvez\naccent_theme=2\nlixo\n", bad);
    CHECK(lr == LoadResult::Corrupt);
    CHECK(bad.sfxVolume == 80 && bad.defaultDashboard.empty() && bad.animations && bad.accentTheme == 2);
    // Arquivo: ausente, escrita atomica e leitura.
    std::string dir = tempDir("settings");
    std::string path = dir + "neutrox.ini";
    Settings tmp;
    CHECK(loadSettingsFile(path, tmp) == LoadResult::Missing);
    CHECK(saveSettingsFile(path, s));
    CHECK(saveSettingsFile(path, s));  // sobrescrever funciona
    Settings fromFile;
    CHECK(loadSettingsFile(path, fromFile) == LoadResult::Ok && fromFile.sfxVolume == 30);
    FILE* fp = std::fopen((path + ".tmp").c_str(), "rb");
    CHECK(fp == nullptr);  // temporario removido
    if (fp) std::fclose(fp);
    // Arquivo binario/corrompido
    fp = std::fopen(path.c_str(), "wb");
    const char junk[] = {'\x00', '\x01', 'z', '\xff'};
    std::fwrite(junk, 1, sizeof(junk), fp);
    std::fclose(fp);
    CHECK(loadSettingsFile(path, fromFile) == LoadResult::Corrupt && fromFile.sfxVolume == 80);
}

static void testAudio() {
    section("Audio: temas recriados por versao e mixer");
    for (int t = 0; t < kSoundThemeCount; ++t) {
        for (int i = 0; i < kSfxCount; ++i) {
            std::vector<int16_t> pcm = synthesize(SoundTheme(t), Sfx(i));
            int peak = 0;
            for (int16_t v : pcm) peak = std::max(peak, std::abs(int(v)));
            if (pcm.empty() || peak <= 1000) std::printf("  tema %d efeito %d: vazio/baixo (pico %d)\n", t, i, peak);
            CHECK(!pcm.empty() && peak > 1000 && peak <= 32767);
        }
    }
    // Os temas sao realmente diferentes entre si.
    CHECK(synthesize(SoundTheme::Blades6717, Sfx::Navigate) != synthesize(SoundTheme::MetroV2_17559, Sfx::Navigate));
    CHECK(synthesize(SoundTheme::Nxe9199, Sfx::Confirm) != synthesize(SoundTheme::Kinect13604, Sfx::Confirm));
    Mixer m;
    m.play(Sfx::Confirm);
    m.play(Sfx::Navigate);
    CHECK(m.activeVoices() == 2);
    std::vector<int16_t> buf(2 * 44100);
    m.mix(buf.data(), 44100);  // 1 s: todas as vozes terminam
    CHECK(m.activeVoices() == 0);
    for (int i = 0; i < 50; ++i) m.play(Sfx::Navigate);
    CHECK(m.activeVoices() <= 12);  // limite de vozes
    m.setMasterVolume(0);
    m.mix(buf.data(), 100);
    CHECK(buf[0] == 0 && buf[99] == 0);
}

static void writeBytes(const std::string& path, const std::vector<uint8_t>& b) {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (f) { std::fwrite(b.data(), 1, b.size(), f); std::fclose(f); }
}

static void testOriginalSounds() {
    section("Sons originais: decodificador WAV e pasta local");
    // WAV PCM 16 bits mono 44,1 kHz (gerado aqui, sem material proprietario)
    std::string dir = tempDir("orig");
    std::vector<int16_t> tone(4410);
    for (size_t i = 0; i < tone.size(); ++i) tone[i] = int16_t(8000 * std::sin(i * 0.1));
    CHECK(writeWav(dir + "t.wav", tone));
    std::vector<uint8_t> bytes;
    {
        FILE* f = std::fopen((dir + "t.wav").c_str(), "rb");
        uint8_t b[4096];
        size_t n;
        while (f && (n = std::fread(b, 1, sizeof(b), f)) > 0) bytes.insert(bytes.end(), b, b + n);
        if (f) std::fclose(f);
    }
    std::vector<int16_t> dec;
    std::string why;
    CHECK(decodeWav(bytes, dec, why) && dec.size() == tone.size());
    CHECK(std::abs(dec[100] - tone[100]) <= 2);
    // XMA e rejeitado com motivo explicito.
    std::vector<uint8_t> xma = bytes;
    xma[20] = 0x66; xma[21] = 0x01;  // formato 0x0166 (XMA2)
    CHECK(!decodeWav(xma, dec, why) && why.find("XMA") != std::string::npos);
    std::vector<uint8_t> junk = {'n', 'a', 'o', ' ', 'w', 'a', 'v'};
    CHECK(!decodeWav(junk, dec, why) && !why.empty());

    // Pasta local: Blades com 2 arquivos (um via mapeamento), 1 XMA, resto ausente.
    std::string base = tempDir("orig/sons_originais");
    std::string bl = base + "blades_6717/";
    std::string mk = "mkdir \"" + bl + "\" >nul 2>nul";
    for (char& c : mk) if (c == '/') c = '\\';
    std::system(mk.c_str());
    writeBytes(bl + "navegar.wav", bytes);
    writeBytes(bl + "som_ok.wav", bytes);
    writeBytes(bl + "voltar.wav", xma);
    writeBytes(bl + "mapeamento.ini", std::vector<uint8_t>{'c','o','n','f','i','r','m','a','r','=','s','o','m','_','o','k','.','w','a','v','\n'});
    Mixer m;
    m.loadOriginals(base);
    OrigReport rep = m.originalsReport();
    CHECK(rep.loaded(SoundTheme::Blades6717) == 2);
    CHECK(rep.e[int(SoundTheme::Blades6717)][int(Sfx::Navigate)].status == OrigStatus::Loaded);
    CHECK(rep.e[int(SoundTheme::Blades6717)][int(Sfx::Confirm)].status == OrigStatus::Loaded);
    CHECK(rep.e[int(SoundTheme::Blades6717)][int(Sfx::Back)].status == OrigStatus::Unsupported);
    CHECK(rep.e[int(SoundTheme::Blades6717)][int(Sfx::Error)].status == OrigStatus::Missing);
    CHECK(rep.loaded(SoundTheme::Nxe9199) == 0);
    // Fonte "Originais": som ausente fica mudo (sem substituicao automatica).
    m.setSource(SoundSource::Originals);
    m.setTheme(SoundTheme::Blades6717);
    m.play(Sfx::Error);
    CHECK(m.activeVoices() == 0);
    m.play(Sfx::Navigate);
    CHECK(m.activeVoices() == 1);
    m.setTheme(SoundTheme::Nxe9199);
    m.play(Sfx::Confirm);
    CHECK(m.activeVoices() == 1);
    // Fonte "Recriados" so quando escolhida explicitamente.
    m.setSource(SoundSource::Recreated);
    m.play(Sfx::Confirm);
    CHECK(m.activeVoices() == 2);
    // Tema do Neutro X sempre toca (identidade propria do aplicativo).
    m.setSource(SoundSource::Originals);
    m.setTheme(SoundTheme::NeutroX);
    m.play(Sfx::Confirm);
    CHECK(m.activeVoices() == 3);
    // Recarregar com voz ativa nao deixa ponteiros pendentes.
    m.loadOriginals(base);
    std::vector<int16_t> buf(2 * 1000);
    m.mix(buf.data(), 1000);
    CHECK(m.activeVoices() == 0);

    section("Troca automatica do tema sonoro com o dashboard");
    Harness h(tempDir("theme"));
    h.app.init();
    h.idle(3.0f);
    h.waitIdle();
    CHECK(h.plat.mixer().theme() == SoundTheme::NeutroX);
    for (int d = 0; d < kDashboardCount; ++d) {
        h.app.openDashboard(DashboardId(d));
        h.waitIdle();
        CHECK(h.plat.mixer().theme() == soundThemeFor(DashboardId(d)));
    }
    h.tap(Button::Start);  // menu do Neutro X por cima: sons do Neutro X
    h.step(0, 2);
    CHECK(h.plat.mixer().theme() == SoundTheme::NeutroX);
    h.tap(Button::B);
    h.step(0, 2);
    CHECK(h.plat.mixer().theme() == SoundTheme::MetroV2_17559);
    // Pastas por versao foram preparadas na pasta de dados (fora do repositorio).
    FILE* g = std::fopen((h.plat.dataPath() + "sons_originais/LEIA-ME.txt").c_str(), "rb");
    CHECK(g != nullptr);
    if (g) std::fclose(g);
    h.app.openMainMenu();
    h.waitIdle();
    h.app.openSettings();
    h.waitIdle();
    for (int i = 0; i < 3; ++i) h.tap(Button::Down);
    h.tap(Button::A);  // "Sons originais: estado"
    h.waitIdle();
    h.idle(0.5f);
    CHECK(std::string(h.app.topScreenName()) == "SoundStatus");
    h.shot("15_sons_originais_estado");
    h.app.shutdown();
}

static void testAppFlow() {
    section("Aplicativo: inicializacao, menu e cinco interfaces");
    Harness h(tempDir("app_flow"));
    CHECK(h.app.init());
    h.step(0, 2);
    CHECK(std::string(h.app.topScreenName()) == "Boot");
    h.shot("01_abertura");
    h.idle(3.0f);
    h.waitIdle();
    CHECK(std::string(h.app.topScreenName()) == "MainMenu");
    h.idle(1.0f);
    h.shot("02_menu_principal");

    // Seleciona cada dashboard pelo menu e verifica carga + navegacao.
    const char* shots[] = {"03_blades", "04_nxe", "05_kinect", "06_metro_v1", "07_metro_v2"};
    for (int d = 0; d < kDashboardCount; ++d) {
        h.app.openMainMenu();
        h.waitIdle();
        CHECK(std::string(h.app.topScreenName()) == "MainMenu");
        for (int k = 0; k < d; ++k) h.tap(Button::Down);
        h.idle(0.4f);
        if (d == 2) h.shot("02b_menu_selecao_kinect");
        h.tap(Button::A);
        h.waitIdle();
        CHECK(h.app.hasActiveDashboard() && int(h.app.activeDashboard()) == d);
        CHECK(std::string(h.app.topScreenName()) == dashboardInfo(DashboardId(d)).name);
        // navegacao entre secoes
        h.tap(Button::RB);
        h.tap(Button::RB);
        h.tap(Button::LB);
        h.idle(0.6f);
        h.shot(shots[d]);
    }

    section("Menu global (START) e troca sem reiniciar");
    h.tap(Button::Start);
    h.idle(0.4f);
    CHECK(std::string(h.app.topScreenName()) == "GlobalMenu");
    h.shot("08_menu_global");
    h.tap(Button::Down);  // "Trocar de dashboard"
    h.tap(Button::A);
    h.idle(0.3f);
    h.shot("09_menu_trocar");
    // vai para Blades (indice 0) a partir de Metro V2 (indice 4)
    for (int i = 0; i < 4; ++i) h.tap(Button::Up);
    h.tap(Button::A);
    h.waitIdle();
    CHECK(h.app.activeDashboard() == DashboardId::Blades);
    CHECK(h.app.screenCount() == 1);

    section("Configuracoes pela interface");
    h.tap(Button::Start);
    h.idle(0.3f);
    h.tap(Button::Down);
    h.tap(Button::Down);  // Configuracoes
    h.tap(Button::A);
    h.waitIdle();
    CHECK(std::string(h.app.topScreenName()) == "Settings");
    h.tap(Button::Right);  // dashboard padrao: Blades
    h.tap(Button::Down);
    h.tap(Button::Left);   // volume 70
    for (int i = 0; i < 5; ++i) h.tap(Button::Down);  // sons, estado, animacoes, intensidade, tema
    h.tap(Button::Right);  // tema de cores -> 1
    h.idle(0.5f);
    h.shot("10_configuracoes");
    h.tap(Button::B);      // salva e volta
    h.waitIdle();
    CHECK(h.app.activeDashboard() == DashboardId::Blades);
    Settings saved;
    CHECK(loadSettingsFile(h.plat.dataPath() + "neutrox.ini", saved) == LoadResult::Ok);
    CHECK(saved.defaultDashboard == "blades" && saved.sfxVolume == 70 && saved.accentTheme == 1);

    section("Sobre e saida segura");
    h.app.openMainMenu();
    h.waitIdle();
    h.app.openAbout();
    h.waitIdle();
    h.idle(0.6f);
    CHECK(std::string(h.app.topScreenName()) == "About");
    h.shot("11_sobre");
    h.tap(Button::B);
    h.waitIdle();
    h.tap(Button::B);  // sair -> dialogo
    h.idle(0.3f);
    CHECK(std::string(h.app.topScreenName()) == "Dialog");
    h.shot("12_dialogo_sair");
    h.tap(Button::A);  // padrao "Nao"
    h.waitIdle();
    CHECK(!h.app.quitRequested());
    h.tap(Button::B);
    h.idle(0.3f);
    h.tap(Button::Left);  // "Sim"
    h.tap(Button::A);
    h.waitIdle();
    CHECK(h.app.quitRequested());
    h.app.shutdown();
    CHECK(h.plat.renderer().textureCount() == 0);  // tudo liberado
}

static bool has(const std::string& s, const char* part) { return s.find(part) != std::string::npos; }

static void testBlades() {
    section("Fase 4: Blades - laminas, lista, subpaineis, opcoes");
    Harness h(tempDir("blades"));
    h.app.init();
    h.idle(3.0f);
    h.waitIdle();
    h.app.openDashboard(DashboardId::Blades);
    h.waitIdle();
    h.idle(1.0f);
    std::string s = h.app.topDebugState();
    CHECK(has(s, "blade=0 sel=0 detail=0"));
    h.shot("20_blades_xbox_live");
    // Lista: limites e movimento
    h.tap(Button::Down);
    h.tap(Button::Down);
    CHECK(has(h.app.topDebugState(), "sel=2"));
    h.tap(Button::Up);
    h.tap(Button::Up);
    h.tap(Button::Up);  // ja no topo
    CHECK(has(h.app.topDebugState(), "sel=0"));
    // Laminas: RB/Direita avancam, LB/Esquerda voltam, sem passar dos limites
    h.tap(Button::RB);
    CHECK(has(h.app.topDebugState(), "blade=1"));
    h.step(0, 9);  // meio da animacao de "virar" a lamina
    h.tap(Button::Right);
    h.step(0, 6);
    h.shot("21_blades_transicao");
    h.idle(0.8f);
    CHECK(has(h.app.topDebugState(), "blade=2"));
    h.shot("22_blades_midia");
    h.tap(Button::Right);
    h.tap(Button::Right);  // limite
    CHECK(has(h.app.topDebugState(), "blade=3"));
    h.idle(0.8f);
    h.shot("23_blades_sistema");
    h.tap(Button::Left);
    h.tap(Button::LB);
    CHECK(has(h.app.topDebugState(), "blade=1"));
    h.idle(0.8f);
    h.shot("24_blades_jogos");
    // Subpainel: Biblioteca de Jogos
    h.tap(Button::A);
    h.idle(0.5f);
    CHECK(has(h.app.topDebugState(), "detail=1 detailSel=0"));
    for (int i = 0; i < 7; ++i) h.tap(Button::Down);  // rola ate o ultimo jogo
    h.idle(0.5f);
    CHECK(has(h.app.topDebugState(), "detailSel=7"));
    h.shot("25_blades_biblioteca");
    h.tap(Button::Down);  // limite
    CHECK(has(h.app.topDebugState(), "detailSel=7"));
    h.tap(Button::A);  // item informativo (toast), continua no subpainel
    CHECK(has(h.app.topDebugState(), "detail=1"));
    h.tap(Button::B);
    h.idle(0.4f);
    CHECK(has(h.app.topDebugState(), "detail=0"));
    // B fora do subpainel volta para a primeira lamina
    h.tap(Button::B);
    h.idle(0.8f);
    CHECK(has(h.app.topDebugState(), "blade=0"));
    // Opcoes (Y): fundo grafite e relogio
    h.tap(Button::Y);
    h.idle(0.4f);
    CHECK(has(h.app.topDebugState(), "options=1"));
    int texBefore = h.plat.renderer().textureCount();
    h.tap(Button::A);  // fundo -> grafite (recria a textura de fundo)
    CHECK(has(h.app.topDebugState(), "graphite=1"));
    CHECK(h.plat.renderer().textureCount() == texBefore);
    h.tap(Button::Down);
    h.tap(Button::A);  // relogio -> ocultar
    CHECK(has(h.app.topDebugState(), "clock=0"));
    h.idle(0.3f);
    h.shot("26_blades_opcoes");
    h.tap(Button::B);
    h.idle(0.5f);
    CHECK(has(h.app.topDebugState(), "options=0"));
    h.shot("27_blades_grafite");
    // Sistema -> Configuracoes do Neutro X abre a tela do shell e volta
    for (int i = 0; i < 3; ++i) h.tap(Button::RB);
    for (int i = 0; i < 4; ++i) h.tap(Button::Down);
    h.tap(Button::A);
    h.waitIdle();
    CHECK(std::string(h.app.topScreenName()) == "Settings");
    h.tap(Button::B);
    h.waitIdle();
    CHECK(std::string(h.app.topScreenName()) == "Blades");
    // Subpaineis de todas as laminas abrem e fecham sem erro.
    for (int b = 0; b < 4; ++b) {
        h.tap(b == 0 ? Button::B : Button::RB);
        h.idle(0.5f);
        for (int i = 0; i < 6; ++i) {
            h.tap(Button::A);
            h.step(0, 10);
            if (std::string(h.app.topScreenName()) != "Blades") {  // itens que abrem telas do shell
                h.tap(Button::B);
                h.waitIdle();
            } else if (has(h.app.topDebugState(), "detail=1")) {
                h.tap(Button::B);
                h.step(0, 10);
            }
            h.tap(Button::Down);
        }
        for (int i = 0; i < 6; ++i) h.tap(Button::Up);
    }
    CHECK(std::string(h.app.topScreenName()) == "Blades");
    CHECK(logErrorCount() == 0);
    h.app.shutdown();
    CHECK(h.plat.renderer().textureCount() == 0);
}

static void testNxe() {
    section("Fase 5: NXE - canais, cartoes em profundidade, avatar, paineis");
    Harness h(tempDir("nxe"));
    h.app.init();
    h.idle(3.0f);
    h.waitIdle();
    h.app.openDashboard(DashboardId::Nxe);
    h.waitIdle();
    h.idle(1.0f);
    CHECK(has(h.app.topDebugState(), "channel=0 card=0 panel=0"));
    h.shot("30_nxe_meu_xbox");
    // Cartoes: Direita/Esquerda com limites
    h.tap(Button::Right);
    h.tap(Button::Right);
    CHECK(has(h.app.topDebugState(), "card=2"));
    h.step(0, 8);
    h.shot("31_nxe_esteira");
    h.tap(Button::Left);
    h.tap(Button::Left);
    h.tap(Button::Left);  // limite
    CHECK(has(h.app.topDebugState(), "card=0"));
    // Avatar acena (X)
    h.idle(0.6f);
    h.tap(Button::X);
    CHECK(has(h.app.topDebugState(), "wave=1"));
    h.idle(0.6f);
    h.shot("32_nxe_avatar_acenando");
    h.idle(2.5f);
    CHECK(has(h.app.topDebugState(), "wave=0"));
    // Canais: Baixo/RB avancam, Cima/LB voltam, com limites
    h.tap(Button::Down);
    CHECK(has(h.app.topDebugState(), "channel=1"));
    h.step(0, 8);
    h.shot("33_nxe_troca_canal");
    h.idle(0.8f);
    for (int i = 0; i < 3; ++i) h.tap(Button::Right);
    h.idle(0.8f);
    CHECK(has(h.app.topDebugState(), "channel=1 card=3"));
    h.shot("34_nxe_biblioteca");
    // Painel de jogo
    h.tap(Button::A);
    h.idle(0.6f);
    CHECK(has(h.app.topDebugState(), "panel=1 panelSel=0"));
    h.tap(Button::A);  // "Jogar": demonstracao, nao executa
    CHECK(has(h.app.topDebugState(), "panel=1"));
    h.tap(Button::Down);
    h.idle(0.4f);
    CHECK(has(h.app.topDebugState(), "panelSel=1"));
    h.shot("35_nxe_painel_jogo");
    h.tap(Button::B);
    h.idle(0.5f);
    CHECK(has(h.app.topDebugState(), "panel=0"));
    h.tap(Button::RB);
    h.tap(Button::RB);
    h.tap(Button::RB);  // limite
    CHECK(has(h.app.topDebugState(), "channel=3"));
    h.tap(Button::LB);
    CHECK(has(h.app.topDebugState(), "channel=2"));
    h.idle(0.8f);
    h.shot("36_nxe_midia");
    h.tap(Button::Down);
    h.idle(0.6f);
    // Configuracoes -> "Neutro X" abre a tela do shell
    for (int i = 0; i < 3; ++i) h.tap(Button::Right);
    h.tap(Button::A);
    h.waitIdle();
    CHECK(std::string(h.app.topScreenName()) == "Settings");
    h.tap(Button::B);
    h.waitIdle();
    CHECK(std::string(h.app.topScreenName()) == "NXE");
    // B: volta ao primeiro cartao, depois ao primeiro canal
    h.tap(Button::B);
    CHECK(has(h.app.topDebugState(), "channel=3 card=0"));
    h.tap(Button::B);
    h.idle(0.8f);
    CHECK(has(h.app.topDebugState(), "channel=0"));
    // Editor de avatar: camiseta
    for (int i = 0; i < 4; ++i) h.tap(Button::Right);
    h.tap(Button::A);
    h.idle(0.4f);
    h.tap(Button::Down);
    h.tap(Button::Down);
    h.tap(Button::A);
    h.idle(0.4f);
    CHECK(has(h.app.topDebugState(), "shirt=2"));
    h.shot("37_nxe_editor_avatar");
    h.tap(Button::B);
    for (int i = 0; i < 4; ++i) h.tap(Button::Left);
    h.idle(0.8f);
    // Opcoes: fundo personalizado e avatar
    int tex = h.plat.renderer().textureCount();
    h.tap(Button::Y);
    h.idle(0.3f);
    CHECK(has(h.app.topDebugState(), "options=1"));
    h.tap(Button::A);
    h.tap(Button::A);  // Verde -> Ceu -> Por do sol
    CHECK(has(h.app.topDebugState(), "theme=2"));
    CHECK(h.plat.renderer().textureCount() == tex);
    h.idle(0.3f);
    h.shot("38_nxe_opcoes");
    h.tap(Button::A);  // Noite
    h.tap(Button::B);
    h.idle(0.6f);
    h.shot("39_nxe_noite");
    h.tap(Button::Y);
    h.tap(Button::Down);
    h.tap(Button::A);  // ocultar avatar
    CHECK(has(h.app.topDebugState(), "avatar=0"));
    h.tap(Button::B);
    h.tap(Button::X);  // sem avatar: nao acena
    CHECK(has(h.app.topDebugState(), "wave=0"));
    // Todos os cartoes de todos os canais abrem e fecham sem erro
    const int counts[4] = {6, 9, 4, 5};
    for (int c = 0; c < 4; ++c) {
        for (int k = 0; k < counts[c]; ++k) {
            h.tap(Button::A);
            h.step(0, 10);
            if (std::string(h.app.topScreenName()) != "NXE") { h.tap(Button::B); h.waitIdle(); }
            else if (has(h.app.topDebugState(), "panel=1")) { h.tap(Button::B); h.step(0, 10); }
            h.tap(Button::Right);
        }
        h.tap(Button::Down);
    }
    CHECK(std::string(h.app.topScreenName()) == "NXE");
    CHECK(logErrorCount() == 0);
    h.app.shutdown();
    CHECK(h.plat.renderer().textureCount() == 0);
}

static void testKinect() {
    section("Fase 6: Kinect - grade, paginas, destaque, paginas de botoes");
    Harness h(tempDir("kinect"));
    h.app.init();
    h.idle(3.0f);
    h.waitIdle();
    h.app.openDashboard(DashboardId::Kinect);
    h.waitIdle();
    h.idle(1.0f);
    CHECK(has(h.app.topDebugState(), "section=0 sel=0 detail=0"));
    h.shot("40_kinect_inicio");
    // Grade 4x2: limites e passagem de pagina pela borda
    for (int i = 0; i < 3; ++i) h.tap(Button::Right);
    CHECK(has(h.app.topDebugState(), "section=0 sel=3"));
    h.tap(Button::Down);
    CHECK(has(h.app.topDebugState(), "sel=7"));
    h.tap(Button::Down);  // limite
    CHECK(has(h.app.topDebugState(), "sel=7"));
    h.tap(Button::Up);
    h.tap(Button::Right);  // borda direita -> proxima secao
    CHECK(has(h.app.topDebugState(), "section=1 sel=0"));
    h.step(0, 10);
    h.shot("41_kinect_deslize_pagina");
    h.tap(Button::Left);  // borda esquerda -> secao anterior, ultima coluna
    CHECK(has(h.app.topDebugState(), "section=0 sel=3"));
    h.tap(Button::LB);  // primeira secao: limite
    CHECK(has(h.app.topDebugState(), "section=0"));
    h.idle(0.8f);
    // Secoes por LB/RB
    h.tap(Button::RB);
    h.idle(0.8f);
    CHECK(has(h.app.topDebugState(), "section=1"));
    h.tap(Button::Right);
    h.idle(0.6f);
    h.shot("42_kinect_jogos");
    // Favorito
    h.tap(Button::X);
    CHECK(has(h.app.topDebugState(), "favs=1"));
    h.tap(Button::X);
    CHECK(has(h.app.topDebugState(), "favs=0"));
    h.tap(Button::X);
    // Pagina de botoes grandes
    h.tap(Button::A);
    h.idle(0.6f);
    CHECK(has(h.app.topDebugState(), "detail=1 detailSel=0"));
    h.tap(Button::A);  // "Jogar": demonstracao
    CHECK(has(h.app.topDebugState(), "detail=1"));
    h.tap(Button::Down);
    h.idle(0.4f);
    h.shot("43_kinect_pagina_jogo");
    for (int i = 0; i < 4; ++i) h.tap(Button::Down);
    CHECK(has(h.app.topDebugState(), "detailSel=3"));  // ultimo botao: Voltar
    h.tap(Button::A);
    h.idle(0.5f);
    CHECK(has(h.app.topDebugState(), "detail=0"));
    for (int i = 0; i < 4; ++i) h.tap(Button::RB);
    h.tap(Button::RB);  // limite
    CHECK(has(h.app.topDebugState(), "section=5"));
    h.idle(0.8f);
    h.shot("44_kinect_configuracoes");
    // Sensor Kinect: informa que nao e usado
    for (int i = 0; i < 3; ++i) h.tap(Button::Right);
    h.tap(Button::A);
    h.idle(0.6f);
    CHECK(has(h.app.topDebugState(), "detail=1"));
    h.shot("45_kinect_sensor");
    h.tap(Button::B);
    h.idle(0.4f);
    // Baixo sob linha mais curta: vai ao ultimo cartao (Sobre, 5); "Neutro X" (4) abre as configuracoes
    h.tap(Button::Down);
    CHECK(has(h.app.topDebugState(), "section=5 sel=5"));
    h.tap(Button::Left);
    h.tap(Button::A);
    h.waitIdle();
    CHECK(std::string(h.app.topScreenName()) == "Settings");
    h.tap(Button::B);
    h.waitIdle();
    CHECK(std::string(h.app.topScreenName()) == "Kinect");
    // B: primeiro cartao da secao; B de novo: Inicio. Atalho "Musica" do Inicio leva a secao Musica
    h.tap(Button::B);
    CHECK(has(h.app.topDebugState(), "section=5 sel=0"));
    h.tap(Button::B);
    h.idle(0.8f);
    CHECK(has(h.app.topDebugState(), "section=0 sel=0"));
    h.tap(Button::Right);
    h.tap(Button::A);
    h.idle(0.8f);
    CHECK(has(h.app.topDebugState(), "section=3"));
    h.tap(Button::B);
    h.idle(0.8f);
    // Opcoes
    int tex = h.plat.renderer().textureCount();
    h.tap(Button::Y);
    h.idle(0.3f);
    CHECK(has(h.app.topDebugState(), "options=1"));
    h.tap(Button::A);
    CHECK(has(h.app.topDebugState(), "theme=1"));
    CHECK(h.plat.renderer().textureCount() == tex);
    h.idle(0.3f);
    h.shot("46_kinect_opcoes");
    h.tap(Button::A);  // grafite
    h.tap(Button::Down);
    h.tap(Button::A);  // ocultar avatar
    CHECK(has(h.app.topDebugState(), "theme=2 avatar=0"));
    h.tap(Button::B);
    h.idle(0.6f);
    h.shot("47_kinect_grafite");
    // Todos os cartoes de todas as secoes abrem e fecham sem erro
    const int counts[6] = {8, 8, 6, 5, 5, 6};
    int opened = 0;
    for (int s = 0; s < 6; ++s) {
        for (int i = 0; i < counts[s]; ++i) {
            // posiciona: Inicio (B, B), secao s (RB), cartao i (Direita/Baixo)
            h.tap(Button::B);
            h.tap(Button::B);
            h.step(0, 20);
            for (int k = 0; k < s; ++k) h.tap(Button::RB);
            for (int k = 0; k < i / 4; ++k) h.tap(Button::Down);
            for (int k = 0; k < i % 4; ++k) h.tap(Button::Right);
            std::string want = "section=" + std::to_string(s) + " sel=" + std::to_string(i) + " ";
            CHECK(has(h.app.topDebugState(), want.c_str()));
            h.tap(Button::A);
            h.step(0, 10);
            ++opened;
            if (std::string(h.app.topScreenName()) != "Kinect") { h.tap(Button::B); h.waitIdle(); }
            else if (has(h.app.topDebugState(), "detail=1")) { h.tap(Button::B); h.step(0, 10); }
        }
    }
    CHECK(opened == 38);
    CHECK(std::string(h.app.topScreenName()) == "Kinect");
    CHECK(logErrorCount() == 0);
    h.app.shutdown();
    CHECK(h.plat.renderer().textureCount() == 0);
}

static void testMetroV1() {
    section("Fase 7: Metro V1 - categorias, blocos, navegacao espacial, paginas");
    Harness h(tempDir("metro_v1"));
    h.app.init();
    h.idle(3.0f);
    h.waitIdle();
    h.app.openDashboard(DashboardId::MetroV1);
    h.waitIdle();
    h.idle(1.0f);
    CHECK(has(h.app.topDebugState(), "category=0 sel=0 detail=0"));
    h.shot("50_metro1_inicio");
    // Navegacao espacial no grupo "inicio": 0 (2x2) -> 1 -> 2 -> 4 -> borda -> social
    h.tap(Button::Right);
    CHECK(has(h.app.topDebugState(), "sel=1"));
    h.tap(Button::Right);
    CHECK(has(h.app.topDebugState(), "sel=2"));
    h.tap(Button::Right);
    CHECK(has(h.app.topDebugState(), "sel=4"));
    h.tap(Button::Right);
    CHECK(has(h.app.topDebugState(), "category=1 sel=0"));
    h.step(0, 10);
    h.shot("51_metro1_troca_categoria");
    h.idle(0.8f);
    h.tap(Button::Left);  // volta pela borda: bloco mais a direita de "inicio"
    CHECK(has(h.app.topDebugState(), "category=0 sel=4"));
    h.tap(Button::Down);
    CHECK(has(h.app.topDebugState(), "sel=5"));
    h.tap(Button::Down);  // sem bloco abaixo
    CHECK(has(h.app.topDebugState(), "sel=5"));
    h.tap(Button::Left);  // 5 (4,1) -> 3 (2,1 largo)
    CHECK(has(h.app.topDebugState(), "sel=3"));
    // Categorias por LB/RB com limites
    h.tap(Button::LB);  // limite
    CHECK(has(h.app.topDebugState(), "category=0"));
    for (int i = 0; i < 8; ++i) h.tap(Button::RB);
    CHECK(has(h.app.topDebugState(), "category=7"));
    h.idle(0.8f);
    h.shot("52_metro1_configuracoes");
    // Sistema: pagina demonstrativa
    h.tap(Button::A);
    h.idle(0.5f);
    CHECK(has(h.app.topDebugState(), "detail=1 detailSel=0"));
    h.tap(Button::Down);
    CHECK(has(h.app.topDebugState(), "detailSel=1"));
    h.shot("53_metro1_pagina_sistema");
    h.tap(Button::B);
    h.idle(0.4f);
    CHECK(has(h.app.topDebugState(), "detail=0"));
    // Blocos que abrem telas do proprio app
    h.tap(Button::Right);
    h.tap(Button::Right);  // Neutro X (2,0 largo)
    CHECK(has(h.app.topDebugState(), "sel=4"));
    h.tap(Button::A);
    h.waitIdle();
    CHECK(std::string(h.app.topScreenName()) == "Settings");
    h.tap(Button::B);
    h.waitIdle();
    h.tap(Button::Down);  // Sobre
    h.tap(Button::A);
    h.waitIdle();
    CHECK(std::string(h.app.topScreenName()) == "About");
    h.tap(Button::B);
    h.waitIdle();
    h.tap(Button::Left);  // Sons originais (1,1)
    CHECK(has(h.app.topDebugState(), "sel=3"));
    h.tap(Button::A);
    h.waitIdle();
    CHECK(std::string(h.app.topScreenName()) == "SoundStatus");
    h.tap(Button::B);
    h.waitIdle();
    CHECK(std::string(h.app.topScreenName()) == "Metro V1");
    // B: primeiro bloco, depois "inicio"
    h.tap(Button::B);
    CHECK(has(h.app.topDebugState(), "category=7 sel=0"));
    h.tap(Button::B);
    h.idle(0.8f);
    CHECK(has(h.app.topDebugState(), "category=0"));
    // Fixar (X)
    h.tap(Button::X);
    CHECK(has(h.app.topDebugState(), "pins=1"));
    h.idle(0.3f);
    h.shot("54_metro1_fixado");
    h.tap(Button::X);
    CHECK(has(h.app.topDebugState(), "pins=0"));
    // Jogos: pagina do jogo em destaque
    for (int i = 0; i < 4; ++i) h.tap(Button::RB);
    h.tap(Button::Right);
    CHECK(has(h.app.topDebugState(), "category=4 sel=2"));
    h.idle(0.6f);
    h.shot("55_metro1_jogos");
    h.tap(Button::A);
    h.idle(0.5f);
    h.tap(Button::A);  // jogar: demonstracao
    CHECK(has(h.app.topDebugState(), "detail=1"));
    h.shot("56_metro1_pagina_jogo");
    h.tap(Button::B);
    h.idle(0.4f);
    // Opcoes: fundo e cor dos blocos
    int tex = h.plat.renderer().textureCount();
    h.tap(Button::Y);
    h.idle(0.3f);
    h.tap(Button::A);
    h.tap(Button::Down);
    h.tap(Button::A);
    h.tap(Button::A);
    CHECK(has(h.app.topDebugState(), "options=1 bg=1 color=2"));
    CHECK(h.plat.renderer().textureCount() == tex);
    h.idle(0.3f);
    h.shot("57_metro1_opcoes");
    h.tap(Button::B);
    h.idle(0.6f);
    h.shot("58_metro1_cinza_laranja");
    // Todos os blocos de todas as categorias executam sua acao e voltam
    const int counts[8] = {6, 4, 5, 5, 8, 6, 6, 6};
    int opened = 0;
    for (int c = 0; c < 8; ++c) {
        for (int i = 0; i < counts[c]; ++i) {
            CHECK(h.app.topDebugSelect(c, i));
            h.step(0, 2);
            h.tap(Button::A);
            h.step(0, 10);
            ++opened;
            if (std::string(h.app.topScreenName()) != "Metro V1") { h.tap(Button::B); h.waitIdle(); }
            else if (has(h.app.topDebugState(), "detail=1")) { h.tap(Button::B); h.step(0, 10); }
            CHECK(std::string(h.app.topScreenName()) == "Metro V1");
        }
    }
    CHECK(opened == 46);
    CHECK(!h.app.topDebugSelect(8, 0) && !h.app.topDebugSelect(0, 99));
    CHECK(logErrorCount() == 0);
    h.app.shutdown();
    CHECK(h.plat.renderer().textureCount() == 0);
}

static void testMetroV2() {
    section("Fase 8: Metro V2 - blocos escuros, biblioteca, paineis locais");
    Harness h(tempDir("metro_v2"));
    h.app.init();
    h.idle(3.0f);
    h.waitIdle();
    h.app.openDashboard(DashboardId::MetroV2);
    h.waitIdle();
    h.idle(1.0f);
    CHECK(has(h.app.topDebugState(), "category=0 sel=0 library=0"));
    h.shot("60_metro2_inicio");
    // Navegacao espacial e bordas
    h.tap(Button::Right);
    CHECK(has(h.app.topDebugState(), "sel=1 "));
    h.tap(Button::Right);
    CHECK(has(h.app.topDebugState(), "sel=5 "));
    h.tap(Button::Right);
    CHECK(has(h.app.topDebugState(), "category=1 sel=0"));
    h.step(0, 8);
    h.shot("61_metro2_troca_categoria");
    h.idle(0.6f);
    h.tap(Button::Left);
    CHECK(has(h.app.topDebugState(), "category=0 sel=5 "));
    h.tap(Button::Down);
    CHECK(has(h.app.topDebugState(), "sel=6 "));
    h.tap(Button::A);  // Mensagens
    h.idle(0.5f);
    CHECK(has(h.app.topDebugState(), "detail=1"));
    h.tap(Button::B);
    h.idle(0.4f);
    // Painel de informacoes locais
    h.tap(Button::Right);
    CHECK(has(h.app.topDebugState(), "sel=7 "));
    h.tap(Button::A);
    h.idle(0.5f);
    CHECK(has(h.app.topDebugState(), "info=1"));
    h.shot("62_metro2_informacoes");
    h.tap(Button::B);
    h.idle(0.4f);
    CHECK(has(h.app.topDebugState(), "info=0"));
    // Biblioteca de jogos
    h.tap(Button::B);  // primeiro bloco
    h.tap(Button::Right);  // Meus jogos
    h.tap(Button::A);
    h.idle(0.6f);
    CHECK(has(h.app.topDebugState(), "library=1 filter=0 libSel=0 libCount=8"));
    for (int i = 0; i < 4; ++i) h.tap(Button::Right);  // limite da linha
    CHECK(has(h.app.topDebugState(), "libSel=3 "));
    h.tap(Button::Down);
    h.tap(Button::Down);  // limite
    CHECK(has(h.app.topDebugState(), "libSel=7 "));
    h.idle(0.5f);
    h.shot("63_metro2_biblioteca");
    h.tap(Button::A);
    h.idle(0.5f);
    CHECK(has(h.app.topDebugState(), "library=1") && has(h.app.topDebugState(), "detail=1"));
    h.tap(Button::A);  // jogar: demonstracao
    CHECK(has(h.app.topDebugState(), "detail=1"));
    h.shot("64_metro2_info_jogo");
    h.tap(Button::B);
    h.idle(0.4f);
    CHECK(has(h.app.topDebugState(), "library=1") && has(h.app.topDebugState(), "detail=0"));
    // Filtros por categoria
    h.tap(Button::RB);
    CHECK(has(h.app.topDebugState(), "filter=1 libSel=0 libCount=2"));
    for (int i = 0; i < 5; ++i) h.tap(Button::RB);
    CHECK(has(h.app.topDebugState(), "filter=6 libSel=0 libCount=3"));
    h.idle(0.5f);
    h.shot("65_metro2_filtro_arcade");
    h.tap(Button::RB);
    CHECK(has(h.app.topDebugState(), "filter=7 libSel=0 libCount=8"));
    h.tap(Button::RB);  // limite
    CHECK(has(h.app.topDebugState(), "filter=7"));
    h.tap(Button::LB);
    CHECK(has(h.app.topDebugState(), "filter=6"));
    h.tap(Button::B);
    h.idle(0.5f);
    CHECK(has(h.app.topDebugState(), "library=0"));
    // Categoria configuracoes: painel local, Neutro X, Sobre, Sons originais
    for (int i = 0; i < 4; ++i) h.tap(Button::RB);
    h.tap(Button::RB);  // limite
    CHECK(has(h.app.topDebugState(), "category=4"));
    h.idle(0.6f);
    h.shot("66_metro2_configuracoes");
    h.tap(Button::A);  // Sistema -> painel de informacoes
    h.idle(0.4f);
    CHECK(has(h.app.topDebugState(), "info=1"));
    h.tap(Button::B);
    h.idle(0.4f);
    h.tap(Button::Right);
    h.tap(Button::Right);  // Neutro X
    h.tap(Button::A);
    h.waitIdle();
    CHECK(std::string(h.app.topScreenName()) == "Settings");
    h.tap(Button::B);
    h.waitIdle();
    h.tap(Button::Down);  // Sobre
    h.tap(Button::A);
    h.waitIdle();
    CHECK(std::string(h.app.topScreenName()) == "About");
    h.tap(Button::B);
    h.waitIdle();
    CHECK(h.app.topDebugSelect(4, 4));  // Sons originais
    h.step(0, 2);
    h.tap(Button::A);
    h.waitIdle();
    CHECK(std::string(h.app.topScreenName()) == "SoundStatus");
    h.tap(Button::B);
    h.waitIdle();
    CHECK(std::string(h.app.topScreenName()) == "Metro V2");
    // Fixar e opcoes
    h.tap(Button::X);
    CHECK(has(h.app.topDebugState(), "pins=1"));
    int tex = h.plat.renderer().textureCount();
    h.tap(Button::Y);
    h.idle(0.3f);
    h.tap(Button::A);  // fundo preto
    h.tap(Button::Down);
    h.tap(Button::A);  // destaque azul
    CHECK(has(h.app.topDebugState(), "options=1 bg=1 accent=1"));
    CHECK(h.plat.renderer().textureCount() == tex);
    h.idle(0.3f);
    h.shot("67_metro2_opcoes");
    h.tap(Button::B);
    h.tap(Button::B);
    h.tap(Button::B);
    h.idle(0.8f);
    h.shot("68_metro2_preto_azul");
    // Todos os blocos executam sua acao e voltam
    const int counts[5] = {10, 12, 7, 8, 8};
    int opened = 0;
    for (int c = 0; c < 5; ++c) {
        for (int i = 0; i < counts[c]; ++i) {
            CHECK(h.app.topDebugSelect(c, i));
            h.step(0, 2);
            h.tap(Button::A);
            h.step(0, 12);
            ++opened;
            std::string s = h.app.topDebugState();
            if (std::string(h.app.topScreenName()) != "Metro V2") { h.tap(Button::B); h.waitIdle(); }
            else if (has(s, "detail=1") || has(s, "info=1") || has(s, "library=1")) { h.tap(Button::B); h.step(0, 12); }
            CHECK(std::string(h.app.topScreenName()) == "Metro V2");
            CHECK(has(h.app.topDebugState(), "library=0") && has(h.app.topDebugState(), "detail=0") &&
                  has(h.app.topDebugState(), "info=0"));
        }
    }
    CHECK(opened == 45);
    CHECK(logErrorCount() == 0);
    h.app.shutdown();
    CHECK(h.plat.renderer().textureCount() == 0);
}

static void testSwitchStressAndMemory() {
    section("Troca repetida de dashboard (100x) e memoria");
    Harness h(tempDir("stress"));
    h.app.init();
    h.idle(3.0f);
    h.waitIdle();
    // Aquecimento: uma volta completa.
    for (int d = 0; d < kDashboardCount; ++d) { h.app.openDashboard(DashboardId(d)); h.waitIdle(); }
    h.app.openMainMenu();
    h.waitIdle();
    StrokeFont::instance().clearCache();
    h.step(0, 2);
    StrokeFont::instance().clearCache();
    long long baseBytes = g_liveBytes.load();
    int baseTex = h.plat.renderer().textureCount();
    size_t baseTexBytes = h.plat.renderer().textureBytes();
    size_t peakTexBytes = 0;
    for (int i = 0; i < 100; ++i) {
        h.app.openDashboard(DashboardId(i % kDashboardCount));
        h.waitIdle();
        CHECK(int(h.app.activeDashboard()) == i % kDashboardCount);
        CHECK(h.app.screenCount() == 1);
        peakTexBytes = std::max(peakTexBytes, h.plat.renderer().textureBytes());
    }
    h.app.openMainMenu();
    h.waitIdle();
    StrokeFont::instance().clearCache();
    h.step(0, 2);
    StrokeFont::instance().clearCache();
    long long endBytes = g_liveBytes.load();
    std::printf("  heap vivo: inicio %.1f KB, fim %.1f KB (dif %+lld bytes)\n", baseBytes / 1024.0, endBytes / 1024.0,
                endBytes - baseBytes);
    std::printf("  texturas: base %d (%.1f MB), pico durante trocas %.1f MB\n", baseTex, baseTexBytes / 1048576.0,
                peakTexBytes / 1048576.0);
    CHECK(h.plat.renderer().textureCount() == baseTex);
    CHECK(h.plat.renderer().textureBytes() == baseTexBytes);
    // So uma interface carregada por vez: pico = base + 1 fundo de 1280x720 (+ folga para o menu).
    CHECK(peakTexBytes <= baseTexBytes + 2 * 1280 * 720 * 4);
    CHECK(std::llabs(endBytes - baseBytes) < 64 * 1024);  // sem vazamento significativo
    h.app.shutdown();
    CHECK(h.plat.renderer().textureCount() == 0);
}

static void testErrorRecovery() {
    section("Recuperacao de erro ao carregar interface");
    Harness h(tempDir("error"));
    h.app.init();
    h.idle(3.0f);
    h.waitIdle();
    struct FailingDash : Dashboard {
        FailingDash() : Dashboard(DashboardId::Kinect) {}
        bool load(Context&, std::string& err) override { err = "recurso de teste ausente"; return false; }
        void unload(Context&) override {}
        void update(Context&, const InputFrame&, float) override {}
        void render(Context&, IRenderer&) override {}
    };
    struct ThrowingDash : Dashboard {
        ThrowingDash() : Dashboard(DashboardId::Nxe) {}
        bool load(Context&, std::string&) override { throw std::runtime_error("excecao simulada"); }
        void unload(Context&) override {}
        void update(Context&, const InputFrame&, float) override {}
        void render(Context&, IRenderer&) override {}
    };
    h.app.setDashboardFactoryForTests([](DashboardId id) -> std::unique_ptr<Dashboard> {
        if (id == DashboardId::Kinect) return std::unique_ptr<Dashboard>(new FailingDash());
        if (id == DashboardId::Nxe) return std::unique_ptr<Dashboard>(new ThrowingDash());
        return dashboardInfo(id).create();
    });
    h.app.openDashboard(DashboardId::Kinect);
    h.waitIdle();
    CHECK(std::string(h.app.topScreenName()) == "Error");
    CHECK(!h.app.hasActiveDashboard());
    h.shot("13_tela_erro");
    h.tap(Button::B);
    h.waitIdle();
    CHECK(std::string(h.app.topScreenName()) == "MainMenu");
    h.app.openDashboard(DashboardId::Nxe);
    h.waitIdle();
    CHECK(std::string(h.app.topScreenName()) == "Error");
    h.tap(Button::B);
    h.waitIdle();
    // Depois de erros, uma interface valida continua abrindo normalmente.
    h.app.openDashboard(DashboardId::MetroV1);
    h.waitIdle();
    CHECK(h.app.activeDashboard() == DashboardId::MetroV1);

    section("Controle desconectado durante o uso");
    RawPadState s;
    s.connected = false;
    for (int i = 0; i < 120; ++i) { h.plat.advance(1.0 / 60); h.app.frame(s, 1.0f / 60); }
    CHECK(h.app.activeDashboard() == DashboardId::MetroV1);
    h.shot("14_controle_desconectado");
    h.step(0, 30);  // reconecta
    CHECK(h.app.activeDashboard() == DashboardId::MetroV1);
    h.app.shutdown();
}

static void testCorruptSettingsAtStartup() {
    section("Inicializacao com configuracao corrompida");
    std::string dir = tempDir("corrupt");
    FILE* f = std::fopen((dir + "neutrox.ini").c_str(), "wb");
    std::fputs("default_dashboard=xyz\nsfx_volume=-5\n", f);
    std::fclose(f);
    Harness h(dir);
    CHECK(h.app.init());
    CHECK(h.app.settings().defaultDashboard.empty() && h.app.settings().sfxVolume == 80);
    Settings reread;
    CHECK(loadSettingsFile(dir + "neutrox.ini", reread) == LoadResult::Ok);  // arquivo reparado
    h.idle(3.0f);
    h.waitIdle();
    CHECK(std::string(h.app.topScreenName()) == "MainMenu");
    h.app.shutdown();

    section("Dashboard padrao abre direto ao iniciar");
    Settings s;
    s.defaultDashboard = "metro_v1";
    saveSettingsFile(dir + "neutrox.ini", s);
    Harness h2(dir);
    h2.app.init();
    h2.idle(3.0f);
    h2.waitIdle();
    CHECK(h2.app.activeDashboard() == DashboardId::MetroV1);
    h2.app.shutdown();
}

static void testPerformance() {
    section("Desempenho (tempo de CPU por quadro, renderizador em software)");
    struct Res { int w, h; };
    const Res resList[] = {{1280, 720}, {1920, 1080}};
    for (const Res& res : resList) {
        Harness h(tempDir("perf"), res.w, res.h);
        h.app.init();
        h.idle(3.0f);
        h.waitIdle();
        auto measure = [&](const char* label) {
            h.step(0, 10);
            double t0 = nowMs();
            const int n = 120;
            h.step(0, n);
            double ms = (nowMs() - t0) / n;
            std::printf("  %4dx%-4d %-22s %6.2f ms/quadro (~%.0f FPS de CPU)\n", res.w, res.h, label, ms, 1000.0 / ms);
            return ms;
        };
        double menu = measure("menu principal");
        double worst = menu;
        for (int d = 0; d < kDashboardCount; ++d) {
            h.app.openDashboard(DashboardId(d));
            h.waitIdle();
            worst = std::max(worst, measure(dashboardInfo(DashboardId(d)).name));
        }
        if (res.w == 1280) CHECK(worst < 16.7);  // 60 FPS no PC em 720p
        h.app.shutdown();
    }
    // Custo de um quadro AO VIVO (sem cache), por componente, em 720p.
    SoftRenderer r(1280, 720);
    auto bench = [&](const char* label, const std::function<void()>& fn) {
        fn();
        double t0 = nowMs();
        for (int i = 0; i < 30; ++i) { r.beginFrame(); fn(); }
        double ms = (nowMs() - t0) / 30;
        std::printf("  ao vivo 720p %-28s %6.2f ms\n", label, ms);
        return ms;
    };
    double bl = bench("Blades (cena completa)", [&]() { dashboardInfo(DashboardId::Blades).drawPreview(r, 0); });
    bench("radial tela cheia", [&]() { r.fillRadial(640, 380, 700, Color(228, 231, 234), Color(228, 231, 234, 0), Rectf{0, 0, 1280, 720}); });
    bench("contorno circulo r=520", [&]() { r.strokeCircle(640, 380, 520, 3, Color(255, 255, 255, 40)); });
    bench("poligono 1/4 tela alfa", [&]() {
        Vec2 q[4] = {{400, 30}, {700, 30}, {480, 690}, {180, 690}};
        r.fillConvex(q, 4, Color(255, 255, 255, 16));
    });
    bench("icone 110 px", [&]() { drawIcon(r, Icon::Person, 640, 360, 110, Color(255, 255, 255, 200)); });
    bench("arte abstrata 434x324", [&]() { drawAbstractArt(r, Rectf{700, 120, 434, 324}, 1, Color(200, 100, 50), Color(20, 10, 5)); });
    bench("gradiente 1100x660", [&]() { r.fillGradientV(Rectf{100, 30, 1100, 660}, Color(90, 180, 40), Color(30, 90, 10)); });
    bench("ret. arredondado 1100x660", [&]() { r.fillRoundRect(Rectf{100, 30, 1100, 660}, 22, Color(90, 180, 40)); });
    bench("texto 40 linhas", [&]() {
        TextStyle st;
        st.size = 30;
        for (int i = 0; i < 40; ++i) r.drawText("Biblioteca de Jogos", 100, 10.0f + i * 17, st);
    });
    (void)bl;  // referencia informativa (inclui o fundo desenhado direto, sem cache)

    // Pior caso realista: o dashboard Blades AO VIVO durante a animacao de troca
    // de lamina. Usa a mediana por quadro (resistente a ruido da maquina).
    auto liveBench = [](DashboardId id, const char* label, Button fwd, Button back) {
        Harness h(tempDir("perf_live"), 1280, 720);
        h.app.init();
        h.idle(3.0f);
        h.waitIdle();
        h.app.openDashboard(id);
        h.waitIdle();
        h.idle(1.2f);
        std::vector<double> ft;
        for (int round = 0; round < 6; ++round) {
            h.step(bit(round % 2 ? back : fwd), 1);
            for (int f = 0; f < 24; ++f) {
                double t0 = nowMs();
                h.step(0, 1);
                ft.push_back(nowMs() - t0);
            }
        }
        std::sort(ft.begin(), ft.end());
        double med = ft[ft.size() / 2], p90 = ft[ft.size() * 9 / 10];
        std::printf("  %s ao vivo (720p): mediana %.2f ms, p90 %.2f ms\n", label, med, p90);
        CHECK(med < 16.7);
        h.app.shutdown();
    };
    liveBench(DashboardId::Blades, "Blades troca de lamina", Button::RB, Button::LB);
    liveBench(DashboardId::Nxe, "NXE troca de canal", Button::Down, Button::Up);
    liveBench(DashboardId::Nxe, "NXE esteira de cartoes", Button::Right, Button::Left);
    liveBench(DashboardId::Kinect, "Kinect deslize de pagina", Button::RB, Button::LB);
    liveBench(DashboardId::Kinect, "Kinect destaque na grade", Button::Down, Button::Up);
    liveBench(DashboardId::MetroV1, "Metro V1 troca de categoria", Button::RB, Button::LB);
    liveBench(DashboardId::MetroV1, "Metro V1 borda entre blocos", Button::Right, Button::Left);
    liveBench(DashboardId::MetroV2, "Metro V2 troca de categoria", Button::RB, Button::LB);
    liveBench(DashboardId::MetroV2, "Metro V2 navegacao entre blocos", Button::Right, Button::Left);
}

int main(int argc, char** argv) {
    if (argc > 1) {
        g_out = argv[1];
        if (g_out.back() != '/' && g_out.back() != '\\') g_out += '/';
    }
    tempDir("");
    logSetFile(g_out + "tests.log");
    std::printf("Neutro X %s - testes automatizados\n", kNeutroXVersion);
    testFont();
    testRenderer();
    testInput();
    testSettings();
    testAudio();
    testOriginalSounds();
    testAppFlow();
    testBlades();
    testNxe();
    testKinect();
    testMetroV1();
    testMetroV2();
    testSwitchStressAndMemory();
    testErrorRecovery();
    testCorruptSettingsAtStartup();
    testPerformance();
    std::printf("\nResultado: %d verificacoes aprovadas, %d falharam\n", g_pass, g_fail);
    logSetFile("");
    return g_fail == 0 ? 0 : 1;
}
