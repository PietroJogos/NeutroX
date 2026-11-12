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
#include <filesystem>
#include <functional>
#include <new>
#include <string>
#include <vector>

#include "audio/audio.h"
#include "core/app.h"
#include "core/log.h"
#include "core/png_writer.h"
#include "core/fsutil.h"
#include "graphics/font.h"
#include "system/launcher.h"
#include "library/title_meta.h"
#include "library/covers.h"
#include "library/library.h"
#include "graphics/image_decode.h"
#include "net/http.h"
#include "dashboards/common/demo_content.h"
#include <thread>
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

// Cabecalho XEX2 minimo e valido (somente para TESTES): flags de titulo + Execution ID.
static std::vector<uint8_t> makeTestXex(uint32_t titleId, uint32_t mediaId, uint32_t flags = 1, bool withExecId = true,
                                        uint8_t disc = 1, uint8_t discs = 1) {
    std::vector<uint8_t> d(0x80, 0);
    auto be = [&d](size_t o, uint32_t v) { d[o] = uint8_t(v >> 24); d[o + 1] = uint8_t(v >> 16); d[o + 2] = uint8_t(v >> 8); d[o + 3] = uint8_t(v); };
    d[0] = 'X'; d[1] = 'E'; d[2] = 'X'; d[3] = '2';
    be(4, flags);
    be(8, 0x80);    // inicio dos dados PE (= fim do cabecalho)
    be(0x10, 0x60); // informacoes de seguranca (nao usadas pela biblioteca)
    be(0x14, withExecId ? 1 : 0);
    if (withExecId) {
        be(0x18, 0x00040006);
        be(0x1C, 0x30);
        be(0x30, mediaId);
        be(0x34, 0x00010000);  // versao
        be(0x3C, titleId);
        d[0x42] = disc;
        d[0x43] = discs;
    }
    return d;
}

// Biblioteca de TESTE: 8 titulos sinteticos numa pasta do proprio teste (nunca no produto).
static const char* const kTestGames[] = {"Corrida Neutra", "Planeta Azul", "Torre de Blocos", "Ecos do Abismo",
                                         "Pixel Fazenda", "Orbita Zero", "Lamina de Vento", "Cidade Aberta"};
static void seedTestLibrary(const std::string& dataDir) {
    const std::string games = dataDir + "jogos_teste/";
    std::string ini = "versao=1\n[opcoes]\nbusca_ao_iniciar=0\ncapas_automaticas=0\n[pasta]\ncaminho=" + normalizePath(games) +
                      "\nprofundidade=3\nativa=1\n";
    for (int i = 0; i < 8; ++i) {
        const std::string dir = games + kTestGames[i] + "/";
        makeDirs(dir);
        const uint32_t tid = 0x4E580100u + uint32_t(i);
        std::vector<uint8_t> x = makeTestXex(tid, 0x1000u + uint32_t(i));
        writeFileAtomic(dir + "default.xex", x.data(), x.size());
        ini += "[jogo]\nnome=" + std::string(kTestGames[i]) + "\ntitle_id=" + hex8(tid) + "\nmedia_id=" + hex8(0x1000u + uint32_t(i)) +
               "\ntipo=" + (i == 2 || i == 4 || i == 5 ? "2" : "0") + "\ncaminho=" + normalizePath(dir + "default.xex") +
               "\npasta=" + normalizePath(games) + "\nadicionado=" + std::to_string(1700000000 + i) + "\ndisponivel=1\n";
    }
    writeFileAtomic(dataDir + "biblioteca.ini", ini);
}

struct Harness {
    HeadlessPlatform plat;
    App app;
    explicit Harness(const std::string& dataDir, int w = 1280, int h = 720, bool seedLibrary = true)
        : plat(w, h, dataDir), app(plat) {
        if (seedLibrary) seedTestLibrary(dataDir);
    }
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

// Pasta de teste vazia (apaga a de uma execucao anterior; so dentro de test_output/).
static std::string freshDir(const char* name) {
    std::string d = g_out + name + "/";
    std::error_code ec;
    std::filesystem::remove_all(std::filesystem::u8path(d), ec);
    std::filesystem::create_directories(std::filesystem::u8path(d), ec);
    return d;
}

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
    const char* pt = "áàâãéêíóôõúçÁÀÂÃÉÊÍÓÔÕÚÇ×·–—°…→←↑↓";
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
    writeBytes(bl + "extra_original.wav", bytes);  // original sem funcao identificada
    const char aus[] = "# gerado pelo extrator\nerro=causa registrada pelo extrator\n";
    writeBytes(bl + "ausentes.ini", std::vector<uint8_t>(aus, aus + sizeof(aus) - 1));
    std::remove((bl + "manifesto.ini").c_str());  // de uma execucao anterior na mesma pasta
    Mixer m;
    m.loadOriginals(base);
    OrigReport rep = m.originalsReport();
    CHECK(rep.filesInFolder[int(SoundTheme::Blades6717)] == 4);  // navegar, som_ok, voltar(.wav XMA), extra
    CHECK(rep.filesInFolder[int(SoundTheme::Nxe9199)] == 0);
    CHECK(rep.e[int(SoundTheme::Blades6717)][int(Sfx::Error)].reason == "causa registrada pelo extrator");
    CHECK(rep.e[int(SoundTheme::Blades6717)][int(Sfx::Startup)].reason == "arquivo não encontrado");
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
    CHECK(m.residentOriginals() == int(SoundTheme::Blades6717));
    CHECK(m.originalBytes() > 0);
    // Trocar de dashboard descarrega o conjunto anterior (a voz do Blades sai) e carrega o novo.
    m.setTheme(SoundTheme::Nxe9199);
    CHECK(m.residentOriginals() == int(SoundTheme::Nxe9199));
    CHECK(m.activeVoices() == 0);
    CHECK(m.originalBytes() == 0);  // NXE sem arquivos: nada na memoria
    m.play(Sfx::Confirm);
    CHECK(m.activeVoices() == 0);
    // Outra versao nao toca o conjunto que nao esta carregado.
    m.playThemed(SoundTheme::Blades6717, Sfx::Navigate);
    CHECK(m.activeVoices() == 0);
    // Fonte "Recriados" so quando escolhida explicitamente.
    m.setSource(SoundSource::Recreated);
    m.play(Sfx::Confirm);
    CHECK(m.activeVoices() == 1);
    // Tema do Neutro X sempre toca (identidade propria do aplicativo) e mantem o conjunto residente.
    m.setSource(SoundSource::Originals);
    m.setTheme(SoundTheme::NeutroX);
    m.play(Sfx::Confirm);
    CHECK(m.activeVoices() == 2);
    CHECK(m.residentOriginals() == int(SoundTheme::Nxe9199));
    m.setTheme(SoundTheme::Blades6717);
    CHECK(m.residentOriginals() == int(SoundTheme::Blades6717));
    m.play(Sfx::Navigate);
    CHECK(m.activeVoices() == 3);
    // Recarregar com voz ativa nao deixa ponteiros pendentes.
    m.loadOriginals(base);
    std::vector<int16_t> buf(2 * 1000);
    m.mix(buf.data(), 1000);
    CHECK(m.activeVoices() == 0);
    CHECK(m.residentOriginals() == int(SoundTheme::Blades6717));  // recarga mantem o tema ativo

    section("Manifesto da extracao (manifesto.ini)");
    {
        CHECK(rep.manifestFiles[int(SoundTheme::Blades6717)] == -1);  // sem manifesto: desconhecido
        CHECK(rep.situation[int(SoundTheme::Blades6717)].empty());
        const char man[] = "# Gerado por tools/sound_extract.py\ndashboard=Blades\nxex_estado=aberto\naudios_originais=4\n"
                           "efeitos_esperados=8\nnao_confirmados=1\nerros_conversao=2\nrecursos_externos=3\n"
                           "situacao=parcial: 2 de 8 efeitos confirmados\n"
                           "arquivo.extra_original.wav=nao_confirmado|-|pacote/extra|XMA2 1ch|1|44100|0.5|00\n"
                           "nao_confirmados=x9\n";  // valor invalido posterior nao vira numero
        writeBytes(bl + "manifesto.ini", std::vector<uint8_t>(man, man + sizeof(man) - 1));
        m.loadOriginals(base);
        OrigReport r2 = m.originalsReport();
        const int b = int(SoundTheme::Blades6717);
        CHECK(r2.manifestFiles[b] == 4);
        CHECK(r2.unconfirmed[b] == -1);
        CHECK(r2.convErrors[b] == 2);
        CHECK(r2.externalRefs[b] == 3);
        CHECK(r2.situation[b] == "parcial: 2 de 8 efeitos confirmados");
        CHECK(r2.xexState[b] == "aberto");
        CHECK(r2.unconfirmed[int(SoundTheme::Nxe9199)] == -1);
        // O manifesto nao altera a contagem real: so arquivos validos contam como carregados.
        CHECK(r2.loaded(SoundTheme::Blades6717) == 2);
    }

    section("Troca automatica do tema sonoro com o dashboard");
    Harness h(freshDir("theme"));  // sem sobras de versoes antigas (ex.: sons_originais/)
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
    FILE* g = std::fopen((h.plat.dataPath() + "local_audio/LEIA-ME.txt").c_str(), "rb");
    CHECK(g != nullptr);
    if (g) std::fclose(g);
    h.app.openMainMenu();
    h.waitIdle();
    h.app.openSettings();
    h.waitIdle();
    for (int i = 0; i < 4; ++i) h.tap(Button::Down);  // (linha "Jogos" entrou na posicao 1)
    h.tap(Button::A);  // "Sons originais: estado"
    h.waitIdle();
    h.idle(0.5f);
    CHECK(std::string(h.app.topScreenName()) == "SoundStatus");
    h.shot("15_sons_originais_estado");
    h.app.shutdown();

    section("Pasta antiga sons_originais/ continua aceita");
    {
        std::string d = tempDir("legado");
        tempDir("legado/sons_originais");
        Harness hl(d);
        CHECK(hl.app.originalSoundsDir() == d + "sons_originais/");
        std::string d2 = tempDir("novo");
        Harness hn(d2);
        CHECK(hn.app.originalSoundsDir() == d2 + "local_audio/");
    }
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
    h.tap(Button::Down);   // Jogos: gerenciar pastas
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
    h.tap(Button::B);  // sair -> dialogo de saida (Aurora / Dashboard / Cancelar)
    h.idle(0.3f);
    CHECK(std::string(h.app.topScreenName()) == "ChoiceDialog");
    h.shot("12_dialogo_sair");
    h.tap(Button::Up);  // ultima opcao: Cancelar
    h.tap(Button::A);
    h.waitIdle();
    CHECK(!h.app.quitRequested());
    CHECK(std::string(h.app.topScreenName()) == "MainMenu");
    h.tap(Button::B);
    h.idle(0.3f);
    h.tap(Button::B);  // B tambem cancela
    h.waitIdle();
    CHECK(!h.app.quitRequested());
    h.tap(Button::B);
    h.idle(0.3f);
    h.tap(Button::A);  // "Voltar ao Aurora": na previa (sem lancador) apenas fecha
    h.waitIdle();
    CHECK(h.app.quitRequested());
    CHECK(h.app.launchOnExit().empty());
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
    // "Meus Jogos" abre a biblioteca real (com o estilo do Blades)
    h.tap(Button::A);
    h.waitIdle();
    h.idle(0.5f);
    CHECK(std::string(h.app.topScreenName()) == "Library");
    CHECK(h.app.libraryStyle() == DashboardId::Blades);
    h.shot("25_blades_meus_jogos");
    h.tap(Button::B);
    h.waitIdle();
    CHECK(std::string(h.app.topScreenName()) == "Blades");
    // Subpainel: Jogos Arcade (titulos XBLA da biblioteca de teste: 3)
    h.tap(Button::Down);
    h.tap(Button::A);
    h.idle(0.5f);
    CHECK(has(h.app.topDebugState(), "detail=1 detailSel=0"));
    for (int i = 0; i < 5; ++i) h.tap(Button::Down);
    h.idle(0.5f);
    CHECK(has(h.app.topDebugState(), "detailSel=2"));  // limite: 3 jogos arcade
    h.tap(Button::A);  // jogo da biblioteca: abre os detalhes reais
    h.waitIdle();
    CHECK(std::string(h.app.topScreenName()) == "GameDetail");
    h.tap(Button::B);
    h.waitIdle();
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
    h.tap(Button::A);  // "Jogar": abre os detalhes reais do jogo (com o botao Jogar)
    h.waitIdle();
    CHECK(std::string(h.app.topScreenName()) == "GameDetail");
    h.tap(Button::B);
    h.waitIdle();
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
    h.tap(Button::A);  // "Jogar": abre os detalhes reais do jogo
    h.waitIdle();
    CHECK(std::string(h.app.topScreenName()) == "GameDetail");
    h.tap(Button::B);
    h.waitIdle();
    CHECK(has(h.app.topDebugState(), "detail=1"));
    h.tap(Button::Down);
    h.idle(0.4f);
    h.shot("43_kinect_pagina_jogo");
    for (int i = 0; i < 4; ++i) h.tap(Button::Down);
    CHECK(has(h.app.topDebugState(), "detailSel=2"));  // ultimo botao: Voltar (Jogar, Detalhes, Voltar)
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
    h.shot("56_metro1_pagina_jogo");
    h.tap(Button::A);  // jogar: abre os detalhes reais do jogo
    h.waitIdle();
    CHECK(std::string(h.app.topScreenName()) == "GameDetail");
    h.tap(Button::B);
    h.waitIdle();
    CHECK(has(h.app.topDebugState(), "detail=1"));
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
    h.shot("64_metro2_info_jogo");
    h.tap(Button::A);  // jogar: abre os detalhes reais do jogo
    h.waitIdle();
    CHECK(std::string(h.app.topScreenName()) == "GameDetail");
    h.tap(Button::B);
    h.waitIdle();
    CHECK(has(h.app.topDebugState(), "detail=1"));
    h.tap(Button::B);
    h.idle(0.4f);
    CHECK(has(h.app.topDebugState(), "library=1") && has(h.app.topDebugState(), "detail=0"));
    // Filtros pelo tipo real do titulo (biblioteca de teste: 5 jogos extraidos + 3 XBLA)
    h.tap(Button::RB);
    CHECK(has(h.app.topDebugState(), "filter=1 libSel=0 libCount=5"));
    for (int i = 0; i < 2; ++i) h.tap(Button::RB);
    CHECK(has(h.app.topDebugState(), "filter=3 libSel=0 libCount=3"));
    h.idle(0.5f);
    h.shot("65_metro2_filtro_arcade");
    for (int i = 0; i < 2; ++i) h.tap(Button::RB);
    CHECK(has(h.app.topDebugState(), "filter=5 libSel=0 libCount=8"));
    h.tap(Button::RB);  // limite
    CHECK(has(h.app.topDebugState(), "filter=5"));
    h.tap(Button::LB);
    CHECK(has(h.app.topDebugState(), "filter=4"));
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

// ---------------------------------------------------------------- Fase 9

static void testOptionPersistence() {
    section("Fase 9: opcoes de cada interface persistem entre execucoes");
    Settings parsed;
    CHECK(parseSettings("opt.nxe.theme=2\nopt.kinect.avatar=0\n", parsed) == LoadResult::Ok);
    CHECK(parsed.opt("nxe.theme", 0, 0, 3) == 2 && parsed.opt("kinect.avatar", 1, 0, 1) == 0);
    CHECK(parsed.opt("nxe.theme", 0, 0, 1) == 1);  // limitado ao intervalo pedido
    Settings bad;
    CHECK(parseSettings("opt.nxe.theme=3\nopt.BAD KEY=1\nopt.x=999999\nopt.=1\n", bad) == LoadResult::Corrupt);
    CHECK(bad.opt("nxe.theme", 0, 0, 3) == 3 && bad.dashOptions.size() == 1);
    Settings rt;
    rt.setOpt("metro2.accent", 2);
    Settings back;
    CHECK(parseSettings(serializeSettings(rt), back) == LoadResult::Ok && back.opt("metro2.accent", 0, 0, 2) == 2);

    std::string dir = tempDir("persist");
    {
        Harness h(dir);
        h.app.init();
        h.idle(3.0f);
        h.waitIdle();
        h.app.openDashboard(DashboardId::Nxe);
        h.waitIdle();
        h.tap(Button::Y);
        h.tap(Button::A);
        h.tap(Button::A);  // tema 2
        h.tap(Button::Down);
        h.tap(Button::A);  // oculta avatar
        h.tap(Button::B);
        h.app.openDashboard(DashboardId::Blades);
        h.waitIdle();
        h.tap(Button::Y);
        h.tap(Button::A);  // grafite
        h.tap(Button::B);
        h.app.openDashboard(DashboardId::MetroV2);
        h.waitIdle();
        h.tap(Button::Y);
        h.tap(Button::Down);
        h.tap(Button::A);
        h.tap(Button::A);  // destaque laranja
        h.tap(Button::B);
        h.app.shutdown();
    }
    {
        Harness h(dir);  // "reinicia" o aplicativo
        h.app.init();
        h.idle(3.0f);
        h.waitIdle();
        h.app.openDashboard(DashboardId::Nxe);
        h.waitIdle();
        CHECK(has(h.app.topDebugState(), "theme=2 avatar=0"));
        h.app.openDashboard(DashboardId::Blades);
        h.waitIdle();
        CHECK(has(h.app.topDebugState(), "graphite=1"));
        h.app.openDashboard(DashboardId::MetroV2);
        h.waitIdle();
        CHECK(has(h.app.topDebugState(), "accent=2"));
        h.shot("70_persistencia_metro2_laranja");
        // Restaurar padroes limpa tambem as opcoes das interfaces
        h.app.settings() = Settings::defaults();
        h.app.saveSettings();
        h.app.openDashboard(DashboardId::Nxe);
        h.waitIdle();
        CHECK(has(h.app.topDebugState(), "theme=0 avatar=1"));
        h.app.shutdown();
    }
}

static void testExitTargets() {
    section("Saida controlada: Aurora, Xbox Dashboard e Cancelar (sem reinicializacao)");
    // Caminhos no estilo do DashLaunch -> unidades do Neutro X
    std::string base = freshDir("exit_targets");
    std::string hdd = base + "hdd/", usb = base + "usb0/";
    makeDirs(hdd);
    makeDirs(usb);
    std::vector<StorageRoot> roots = {{hdd, "Disco", "Hdd1"}, {usb, "USB", "Usb0"}};
    CHECK(consolePathToLocal("Hdd:\\Aurora\\Aurora.xex", roots) == normalizePath(hdd + "Aurora/Aurora.xex"));
    CHECK(consolePathToLocal("Hdd1:\\Apps\\A.xex", roots) == normalizePath(hdd + "Apps/A.xex"));
    CHECK(consolePathToLocal("Usb:\\X\\default.xex", roots) == normalizePath(usb + "X/default.xex"));
    CHECK(consolePathToLocal("Mu0:\\x.xex", roots).empty());
    CHECK(localPathToConsole(hdd + "Games/Halo/default.xex", roots) == "Hdd1:\\Games\\Halo\\default.xex");
    CHECK(iniValue("[Paths]\r\nDefault = Hdd:\\Aurora\\Aurora.xex ; comentario\r\n", "default") == "Hdd:\\Aurora\\Aurora.xex");

    // Sem Aurora em lugar nenhum: motivo explicado, nada encerrado
    LaunchTarget none = findAurora(roots, "");
    CHECK(!none.found && !none.reason.empty());
    // launch.ini (somente leitura) apontando para o Aurora numa pasta fora do comum
    makeDirs(hdd + "Dash/Aur");
    writeBytes(hdd + "Dash/Aur/Aurora.xex", {'X', 'E', 'X', '2'});
    writeBytes(hdd + "launch.ini", std::vector<uint8_t>({'[', 'P', 'a', 't', 'h', 's', ']', '\n'}));
    {
        std::string ini = "[Paths]\nDefault = Hdd:\\Dash\\Aur\\Aurora.xex\n";
        writeBytes(hdd + "launch.ini", std::vector<uint8_t>(ini.begin(), ini.end()));
    }
    std::vector<uint8_t> before;
    readFileBytes(hdd + "launch.ini", before, 4096);
    LaunchTarget viaIni = findAurora(roots, "");
    CHECK(viaIni.found && viaIni.path == normalizePath(hdd + "Dash/Aur/Aurora.xex"));
    CHECK(viaIni.source.find("launch.ini") != std::string::npos);
    std::vector<uint8_t> after;
    readFileBytes(hdd + "launch.ini", after, 4096);
    CHECK(before == after);  // launch.ini nunca e alterado
    // Local padrao no USB e caminho configurado tem prioridade
    makeDirs(usb + "Aurora");
    writeBytes(usb + "Aurora/Aurora.xex", {'X', 'E', 'X', '2'});
    CHECK(findAurora(roots, usb + "Aurora/Aurora.xex").source == "caminho configurado");
    LaunchTarget miss = findAurora(roots, usb + "nao/existe.xex");  // configurado sumiu: cai no launch.ini
    CHECK(miss.found && miss.source.find("launch.ini") != std::string::npos);

    // Fluxo no "console" (plataforma que inicia titulos)
    Harness h(tempDir("exit_flow"));
    h.plat.setRoots(roots);
    h.plat.setLaunchable(true, base + "flash/dash.xex");  // dash.xex ausente
    h.app.init();
    h.idle(3.0f);
    h.waitIdle();
    LaunchTarget dash = h.app.exitTargetInfo(ExitTarget::Dashboard);
    CHECK(!dash.found && dash.reason.find("dash.xex") != std::string::npos);
    std::string why;
    CHECK(!h.app.requestExitTo(ExitTarget::Dashboard, &why) && !why.empty());
    CHECK(!h.app.quitRequested());  // destino indisponivel: continua rodando
    // Dialogo: a opcao do Dashboard fica desabilitada com o motivo; Cancelar volta
    h.tap(Button::B);
    h.idle(0.3f);
    CHECK(std::string(h.app.topScreenName()) == "ChoiceDialog");
    h.tap(Button::Down);  // Xbox Dashboard (desabilitado)
    h.tap(Button::A);
    h.waitIdle();
    CHECK(!h.app.quitRequested());
    CHECK(std::string(h.app.topScreenName()) == "ChoiceDialog");
    h.tap(Button::Up);  // Voltar ao Aurora
    h.tap(Button::A);
    h.waitIdle();
    CHECK(h.app.quitRequested());
    CHECK(h.app.launchOnExit() == normalizePath(hdd + "Dash/Aur/Aurora.xex"));
    h.app.shutdown();
    CHECK(h.plat.renderer().textureCount() == 0);

    // Dashboard oficial presente
    makeDirs(base + "flash");
    writeBytes(base + "flash/dash.xex", {'X', 'E', 'X', '2'});
    Harness h2(tempDir("exit_flow2"));
    h2.plat.setRoots(roots);
    h2.plat.setLaunchable(true, base + "flash/dash.xex");
    h2.app.init();
    h2.idle(3.0f);
    h2.waitIdle();
    CHECK(h2.app.requestExitTo(ExitTarget::Dashboard));
    CHECK(h2.app.launchOnExit() == base + "flash/dash.xex");
    h2.app.shutdown();
}


// ------------------------------------------------------------ biblioteca de jogos

static std::vector<uint8_t> readFixture(const char* name) {
    std::vector<uint8_t> b;
    readFileBytes(std::string(NX_TEST_FIXTURES) + name, b, 1u << 20);
    return b;
}

// Cabecalho STFS sintetico (somente TESTES).
static std::vector<uint8_t> makeTestStfs(const char* magic, uint32_t contentType, uint32_t titleId, const std::string& name,
                                         const std::vector<uint8_t>& thumb) {
    std::vector<uint8_t> d(0x971A, 0);
    std::memcpy(d.data(), magic, 4);
    auto be = [&d](size_t o, uint32_t v) { d[o] = uint8_t(v >> 24); d[o + 1] = uint8_t(v >> 16); d[o + 2] = uint8_t(v >> 8); d[o + 3] = uint8_t(v); };
    be(0x344, contentType);
    be(0x354, 0xABCD0001u);
    be(0x360, titleId);
    for (size_t i = 0; i < name.size() && i < 0x7F; ++i) d[0x411 + i * 2 + 1] = uint8_t(name[i]);  // UTF-16BE (ASCII)
    be(0x1712, uint32_t(thumb.size()));
    std::copy(thumb.begin(), thumb.end(), d.begin() + 0x171A);
    return d;
}

static void testTitleMetadata() {
    section("Biblioteca: cabecalhos reais (XEX2 e STFS), sem aceitar qualquer .xex");
    XexMeta m;
    std::vector<uint8_t> ok = makeTestXex(0x4D5307E6, 0x12345678, 1, true, 1, 2);
    CHECK(parseXexHeader(ok.data(), ok.size(), m) && m.titleId == 0x4D5307E6 && m.mediaId == 0x12345678);
    CHECK(m.discNumber == 1 && m.discCount == 2);
    std::vector<uint8_t> dll = makeTestXex(0x4D5307E6, 1, 0x08 | 0x01);
    CHECK(!parseXexHeader(dll.data(), dll.size(), m) && m.error.find("DLL") != std::string::npos);
    std::vector<uint8_t> patch = makeTestXex(0x4D5307E6, 1, 0x01 | 0x10);
    CHECK(!parseXexHeader(patch.data(), patch.size(), m) && m.error.find("patch") != std::string::npos);
    std::vector<uint8_t> noId = makeTestXex(0x4D5307E6, 1, 1, false);
    CHECK(!parseXexHeader(noId.data(), noId.size(), m) && m.error.find("Execution ID") != std::string::npos);
    std::vector<uint8_t> notTitle = makeTestXex(0x4D5307E6, 1, 0x80);
    CHECK(!parseXexHeader(notTitle.data(), notTitle.size(), m));
    std::vector<uint8_t> junk(64, 'A');
    CHECK(!parseXexHeader(junk.data(), junk.size(), m));
    std::vector<uint8_t> trunc(ok.begin(), ok.begin() + 0x20);
    CHECK(!parseXexHeader(trunc.data(), trunc.size(), m));  // nunca le fora do buffer

    std::vector<uint8_t> thumb = readFixture("miniatura_teste.png");
    CHECK(thumb.size() > 50);
    StfsMeta s;
    std::vector<uint8_t> god = makeTestStfs("LIVE", kContentGamesOnDemand, 0x415607E6, "Jogo GOD de Teste", thumb);
    CHECK(parseStfsHeader(god.data(), god.size(), s) && s.titleId == 0x415607E6 && s.contentType == kContentGamesOnDemand);
    CHECK(s.displayName == "Jogo GOD de Teste" && s.thumbnail == thumb);
    std::vector<uint8_t> shortHdr(god.begin(), god.begin() + 0x400);
    CHECK(!parseStfsHeader(shortHdr.data(), shortHdr.size(), s));
    CHECK(hex8(0x4D5307E6) == "4D5307E6");
    uint32_t v = 0;
    CHECK(parseHex8("4d5307e6", v) && v == 0x4D5307E6 && !parseHex8("XYZ", v));

    section("Imagens: PNG (deflate) e JPEG baseline proprios");
    Image img;
    std::string why;
    CHECK(decodePng(thumb.data(), thumb.size(), img, &why) && img.w == 64 && img.h == 64);
    CHECK(img.px.size() == 64 * 64 && (img.px[size_t(10) * 64 + 20] & 0x00FF0000u) >> 16 == 80);  // R = x*4
    CHECK((img.px[63] >> 24) == 100);  // alfa da coluna 63
    std::vector<uint8_t> jpg = readFixture("capa_teste.jpg");
    CHECK(decodeJpeg(jpg.data(), jpg.size(), img, &why) && img.w == 300 && img.h == 200);
    auto near = [](uint32_t p, int r, int g, int b) {
        return std::abs(int((p >> 16) & 255) - r) < 30 && std::abs(int((p >> 8) & 255) - g) < 30 && std::abs(int(p & 255) - b) < 30;
    };
    CHECK(near(img.px[size_t(100) * 300 + 50], 30, 60, 200));   // verso (azul)
    CHECK(near(img.px[size_t(100) * 300 + 250], 220, 30, 30));  // frente (vermelho)
    Image front = frontCover(img);
    CHECK(front.h == 200 && front.w == 141 && near(front.px[size_t(100) * front.w + 70], 220, 30, 30));
    Image fit = coverFit(front, CoverTextures::kW, CoverTextures::kH);
    CHECK(fit.w == CoverTextures::kW && fit.h == CoverTextures::kH);
    CHECK(validateCoverBytes(jpg, &why));
    std::string html = "<html><body>erro</body></html>";
    CHECK(!validateCoverBytes(std::vector<uint8_t>(html.begin(), html.end()), &why));
    std::vector<uint8_t> broken = jpg;
    broken.resize(broken.size() / 3);
    CHECK(!decodeJpeg(broken.data(), broken.size(), img, &why) || img.w == 300);  // truncado: falha sem travar
    std::string json = "{\"Covers\":[{\"CoverID\":\"4245\",\"Rating\":\"5\",\"Official\":\"0\"},{\"CoverID\":\"225\",\"Rating\":\"5\",\"Official\":\"1\"},"
                       "{\"CoverID\":\"99\",\"Rating\":null,\"Official\":\"0\"}],\"CoversCount\":3}";
    int count = 0;
    CHECK(pickCoverId(json, &count) == "225" && count == 3);  // oficial primeiro
    CHECK(pickCoverId("{\"Covers\":[]}", &count).empty() && count == 0);
}

static void testLibraryScan() {
    section("Biblioteca: busca nas pastas, profundidade, ignorados, duplicatas, GOD/XBLA");
    const std::string base = freshDir("lib_scan");
    const std::string hdd = base + "hdd/", usb = base + "usb0/";
    auto put = [](const std::string& path, const std::vector<uint8_t>& b) {
        makeDirs(parentPath(path));
        writeFileAtomic(path, b.data(), b.size());
    };
    put(hdd + "Games/Jogo A/default.xex", makeTestXex(0x41410001, 0x11));
    put(hdd + "Games/Jogo A/media/extra.xex", makeTestXex(0x41410002, 0x12));  // dentro de um jogo: nao entra
    put(hdd + "Games/Modulo DLL/default.xex", makeTestXex(0x41410003, 0x13, 0x09));
    put(hdd + "Games/Duplo/Disc 1/default.xex", makeTestXex(0x41410004, 0x14, 1, true, 1, 2));
    put(hdd + "Games/Duplo/Disc 2/default.xex", makeTestXex(0x41410004, 0x15, 1, true, 2, 2));
    put(hdd + "Games/a/b/c/d/e/Fundo/default.xex", makeTestXex(0x41410005, 0x16));  // alem da profundidade
    put(hdd + "Games/Cache/Jogo Ignorado/default.xex", makeTestXex(0x41410006, 0x17));
    put(hdd + "Games/Aplicativo/Ferramenta.xex", makeTestXex(0x41410007, 0x18));
    put(hdd + "Games/Proprio/default.xex", makeTestXex(0x4E455558, 0x19));  // o proprio Neutro X
    std::vector<uint8_t> thumb = readFixture("miniatura_teste.png");
    put(hdd + "Content/0000000000000000/415607E6/00007000/ABCDEF0123456789", makeTestStfs("LIVE", kContentGamesOnDemand, 0x415607E6, "GOD Teste", thumb));
    makeDirs(hdd + "Content/0000000000000000/415607E6/00007000/ABCDEF0123456789.data");
    put(hdd + "Content/0000000000000000/58410A01/000D0000/1122334455667788", makeTestStfs("LIVE", kContentArcadeTitle, 0x58410A01, "XBLA Teste", {}));
    put(hdd + "Content/0000000000000000/FFFE07D1/00010000/SaveDeJogo", makeTestStfs("CON ", 0x00000001, 0xFFFE07D1, "Save", {}));
    put(usb + "Games/Jogo A (copia)/default.xex", makeTestXex(0x41410001, 0x11));  // mesma Media ID: duplicata

    Library lib;
    std::string why;
    CHECK(lib.addFolder(hdd + "Games", &why));
    CHECK(lib.addFolder(hdd + "Content", &why));
    CHECK(lib.addFolder(usb + "Games", &why));
    CHECK(!lib.addFolder(hdd + "Games", &why) && why.find("já") != std::string::npos);
    CHECK(!lib.addFolder(hdd + "NaoExiste", &why));
    lib.folders()[0].maxDepth = 4;
    lib.addIgnore("Cache");
    ScanStats st;
    std::vector<GameEntry> found = Library::scan(lib.folders(), lib.options(), st);
    lib.merge(found, st, 1000);
    auto byTitle = [&](uint32_t tid) -> const GameEntry* {
        for (const GameEntry& g : lib.games())
            if (g.titleId == tid) return &g;
        return nullptr;
    };
    std::printf("  busca: %d pastas, %d candidatos, %d titulos, %d recusados\n", st.folders, st.candidates, st.found, st.rejected);
    CHECK(byTitle(0x41410001) && byTitle(0x41410001)->name == "Jogo A" && byTitle(0x41410001)->kind == GameKind::Xex);
    CHECK(!byTitle(0x41410002));  // nao percorre a pasta de um jogo
    CHECK(!byTitle(0x41410003));  // DLL recusada
    CHECK(!byTitle(0x41410005));  // alem da profundidade
    CHECK(!byTitle(0x41410006));  // pasta ignorada
    CHECK(!byTitle(0x41410007));  // aplicativos desligados por padrao
    CHECK(!byTitle(0x4E455558));  // o proprio Neutro X
    CHECK(!byTitle(0xFFFE07D1));  // pacote que nao e jogo (save)
    int discs = 0;
    for (const GameEntry& g : lib.games())
        if (g.titleId == 0x41410004) { ++discs; CHECK(g.name.find("Duplo (disco") == 0); }
    CHECK(discs == 2);
    const GameEntry* god = byTitle(0x415607E6);
    CHECK(god && god->kind == GameKind::God && god->name == "GOD Teste" && !god->thumb.empty());
    const GameEntry* xbla = byTitle(0x58410A01);
    CHECK(xbla && xbla->kind == GameKind::Xbla);
    // Duplicata: duas entradas, uma oculta nas vistas
    int copies = 0;
    for (const GameEntry& g : lib.games()) copies += g.titleId == 0x41410001;
    CHECK(copies == 2);
    int visibleA = 0;
    for (int i : lib.view(Library::Filter::All)) visibleA += lib.games()[size_t(i)].titleId == 0x41410001;
    CHECK(visibleA == 1);
    const size_t visible = lib.view(Library::Filter::All).size();
    CHECK(visible == 5);  // Jogo A, Duplo x2, GOD, XBLA

    section("Biblioteca: favoritos, remover sem apagar, indisponivel, nova busca preserva dados");
    const std::string idA = byTitle(0x41410001)->id;
    lib.toggleFavorite(idA);
    CHECK(lib.view(Library::Filter::Favorites).size() == 1);
    CHECK(lib.view(Library::Filter::Search, "god").size() == 1 && lib.view(Library::Filter::Search, "JOGO a").size() == 1);
    // Rebusca: favorito e data de adicao preservados
    found = Library::scan(lib.folders(), lib.options(), st);
    lib.merge(found, st, 2000);
    CHECK(lib.find(idA) && lib.find(idA)->favorite && lib.find(idA)->added == 1000);
    // Remover: sai da biblioteca, arquivo continua, nao volta numa nova busca
    const std::string xblaPath = xbla->path, xblaId = xbla->id;
    CHECK(lib.remove(xblaId));
    CHECK(fileExists(xblaPath));
    found = Library::scan(lib.folders(), lib.options(), st);
    lib.merge(found, st, 3000);
    CHECK(!byTitle(0x58410A01));
    // Jogo apagado por fora: fica marcado como indisponivel (nao some sozinho)
    std::remove((hdd + "Games/Duplo/Disc 2/default.xex").c_str());
    lib.refreshAvailability();
    CHECK(lib.view(Library::Filter::Unavailable).size() == 1);
    // Adicionar manualmente valida o arquivo
    std::string id;
    CHECK(lib.addManual(hdd + "Games/Aplicativo/Ferramenta.xex", &why, &id) && lib.find(id)->kind == GameKind::App);
    CHECK(!lib.addManual(hdd + "Games/Modulo DLL/default.xex", &why) && why.find("DLL") != std::string::npos);
    CHECK(!lib.addManual(hdd + "Games/Aplicativo/Ferramenta.xex", &why));  // ja esta

    section("Biblioteca: gravacao segura e sem escritas repetidas");
    const std::string ini = base + "biblioteca.ini";
    CHECK(lib.dirty() && lib.save(ini) && !lib.dirty());
    std::vector<uint8_t> first;
    readFileBytes(ini, first, 1u << 20);
    std::remove(ini.c_str());
    CHECK(lib.save(ini) && !fileExists(ini));  // nada mudou: nao grava de novo
    lib.markDirty();
    CHECK(lib.save(ini));
    Library again;
    CHECK(again.load(ini));
    CHECK(again.games().size() == lib.games().size() && again.folders().size() == 3 && again.options().ignore.size() == lib.options().ignore.size());
    CHECK(again.find(idA) && again.find(idA)->favorite && again.find(id) && again.find(id)->manual);
    CHECK(!fileExists(ini + ".tmp"));
    // Remover a pasta monitorada tira as entradas dela (os arquivos ficam)
    again.removeFolder(2);
    int fromUsb = 0;
    for (const GameEntry& g : again.games()) fromUsb += g.path.find("usb0") != std::string::npos;
    CHECK(fromUsb == 0 && fileExists(usb + "Games/Jogo A (copia)/default.xex"));
    // Arquivo corrompido: carrega o que for valido, sem travar
    writeFileAtomic(base + "ruim.ini", std::string("[jogo]\ntitle_id=ZZZ\ncaminho=\n[pasta]\n\n\x01\x02 lixo"));
    Library bad;
    bad.load(base + "ruim.ini");
    CHECK(bad.games().empty() && bad.folders().empty());
}

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
using socklen_t = int;
using TestSock = SOCKET;
static void closeTestSock(TestSock s) { closesocket(s); }
#else
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using TestSock = int;
static void closeTestSock(TestSock s) { close(s); }
#endif

// Servidor HTTP local minimo (somente para testes): responde CoverInfo.php e Cover.php.
struct TestHttpServer {
    TestSock s{};
    int port = 0;
    std::thread th;
    std::atomic<bool> stop{false};
    std::atomic<int> requests{0};
    std::vector<uint8_t> cover;
    bool start() {
        HttpResponse dummy;
        httpGet("http://127.0.0.1:1/", dummy);  // inicia o Winsock pelo cliente
        s = socket(AF_INET, SOCK_STREAM, 0);
        sockaddr_in a{};
        a.sin_family = AF_INET;
        a.sin_addr.s_addr = htonl(0x7F000001);
        a.sin_port = 0;
        if (bind(s, reinterpret_cast<sockaddr*>(&a), sizeof a) != 0 || listen(s, 4) != 0) return false;
        socklen_t len = sizeof a;
        getsockname(s, reinterpret_cast<sockaddr*>(&a), &len);
        port = ntohs(a.sin_port);
        th = std::thread([this]() {
            while (!stop) {
                TestSock c = accept(s, nullptr, nullptr);
                if (stop) { closeTestSock(c); break; }
                char buf[2048];
                int n = int(recv(c, buf, sizeof buf - 1, 0));
                std::string req(buf, buf + (n > 0 ? n : 0));
                ++requests;
                std::string body, type = "text/html", status = "200 OK";
                if (req.find("CoverInfo.php?titleid=4D5307E6") != std::string::npos)
                    body = "{\"Covers\":[{\"CoverID\":\"7\",\"Rating\":\"4\",\"Official\":\"1\"}],\"CoversCount\":1}";
                else if (req.find("CoverInfo.php") != std::string::npos)
                    body = "{\"Covers\":[],\"CoversCount\":0}";
                else if (req.find("Cover.php?size=large&cid=7") != std::string::npos) {
                    body.assign(cover.begin(), cover.end());
                    type = "image/png";  // como o servidor real: tipo errado, conteudo JPEG
                } else { status = "404 Not Found"; body = "nao encontrado"; }
                std::string head = "HTTP/1.1 " + status + "\r\nContent-Type: " + type + "\r\nContent-Length: " +
                                   std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n";
                send(c, head.data(), int(head.size()), 0);
                send(c, body.data(), int(body.size()), 0);
                closeTestSock(c);
            }
        });
        return true;
    }
    ~TestHttpServer() {
        stop = true;
        if (port) {
            HttpResponse r;
            httpGet("http://127.0.0.1:" + std::to_string(port) + "/fim", r);  // desbloqueia o accept
        }
        if (th.joinable()) th.join();
        closeTestSock(s);
    }
};

static void testCovers() {
    section("Capas: download do XboxUnity (servidor local de teste), validacao, cache e modo offline");
    TestHttpServer srv;
    srv.cover = readFixture("capa_teste.jpg");
    CHECK(srv.start());
    const std::string base = freshDir("covers");
    CoverService svc;
    svc.setDirs(base + "capas/", base + "capas_importar/");
    svc.setBaseUrl("http://127.0.0.1:" + std::to_string(srv.port));
    auto waitResults = [&svc](int want) {
        std::vector<CoverResult> all;
        for (int i = 0; i < 400 && int(all.size()) < want; ++i) {
            for (auto& r : svc.takeResults()) all.push_back(r);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return all;
    };
    svc.request("jogoA", 0x4D5307E6, false, true);
    svc.request("jogoB", 0x11111111, false, false);  // sem capa no servidor
    std::vector<CoverResult> rs = waitResults(2);
    CHECK(rs.size() == 2);
    for (const CoverResult& r : rs) {
        if (r.gameId == "jogoA") CHECK(r.ok && r.file == "4D5307E6.jpg" && fileExists(base + "capas/4D5307E6.jpg"));
        if (r.gameId == "jogoB") CHECK(!r.ok && r.message.find("nenhuma capa") != std::string::npos);
    }
    std::vector<uint8_t> saved;
    readFileBytes(base + "capas/4D5307E6.jpg", saved, 1u << 20);
    CHECK(saved == srv.cover);
    CHECK(!fileExists(base + "capas/4D5307E6.jpg.part") && !fileExists(base + "capas/4D5307E6.jpg.tmp"));
    // Em cache: nao baixa de novo
    const int before = srv.requests.load();
    svc.request("jogoA", 0x4D5307E6, false, false);
    rs = waitResults(1);
    CHECK(rs.size() == 1 && rs[0].ok && srv.requests.load() == before);
    // Sem internet (porta fechada): falha com motivo, capa salva continua
    CoverService offline;
    offline.setDirs(base + "capas/", base + "capas_importar/");
    offline.setBaseUrl("http://127.0.0.1:1");
    offline.request("jogoA", 0x4D5307E6, true, true);
    std::vector<CoverResult> off;
    for (int i = 0; i < 600 && off.empty(); ++i) {
        off = offline.takeResults();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    CHECK(off.size() == 1 && !off[0].ok && !off[0].message.empty());
    CHECK(fileExists(base + "capas/4D5307E6.jpg"));
    std::printf("  offline: %s\n", off.empty() ? "?" : off[0].message.c_str());
    HttpResponse https;
    CHECK(!httpGet("https://xboxunity.net/", https) && https.error.find("HTTPS") != std::string::npos);
    // Importacao manual
    makeDirs(base + "capas_importar");
    writeFileAtomic(base + "capas_importar/415607E6.jpg", srv.cover.data(), srv.cover.size());
    writeFileAtomic(base + "capas_importar/nome_errado.jpg", srv.cover.data(), srv.cover.size());
    writeFileAtomic(base + "capas_importar/58410A01.png", std::string("isto nao e uma imagem de verdade ............................................"));
    std::vector<std::string> notes;
    auto imported = svc.importFolder(&notes);
    CHECK(imported.size() == 1 && imported[0].first == 0x415607E6 && notes.size() == 2);
}

static void testLibraryApp() {
    section("Meus Jogos: tela, filtros, pesquisa, favoritos, detalhes, pastas e Jogar");
    const std::string base = freshDir("lib_app");
    makeDirs(base + "hdd/Games/Halo Teste");
    std::vector<uint8_t> x = makeTestXex(0x4D5307E6, 0x99);
    writeFileAtomic(base + "hdd/Games/Halo Teste/default.xex", x.data(), x.size());
    makeDirs(base + "hdd/Games/Outro Jogo");
    x = makeTestXex(0x41410042, 0x98);
    writeFileAtomic(base + "hdd/Games/Outro Jogo/default.xex", x.data(), x.size());
    Harness h(base + "dados/", 1280, 720, false);
    h.plat.setRoots({{base + "hdd/", "Disco de teste", "Hdd1"}});
    h.app.init();
    h.app.library().options().autoCovers = false;  // testes nao acessam a internet
    h.idle(3.0f);
    h.waitIdle();
    CHECK(h.app.library().games().empty());
    // Menu principal -> Meus Jogos (vazio) -> A abre "Jogos e pastas"
    for (int i = 0; i < 5; ++i) h.tap(Button::Down);
    h.tap(Button::A);
    h.waitIdle();
    CHECK(std::string(h.app.topScreenName()) == "Library");
    h.shot("80_meus_jogos_vazio");
    h.tap(Button::A);
    h.waitIdle();
    CHECK(std::string(h.app.topScreenName()) == "LibrarySettings");
    // "+ Adicionar pasta…" -> navegador: unidade -> Games -> X (usar esta pasta)
    h.tap(Button::A);
    h.waitIdle();
    CHECK(std::string(h.app.topScreenName()) == "FolderBrowser");
    h.tap(Button::A);  // entra na unidade
    h.waitIdle();
    h.shot("81_navegador_pastas");
    h.tap(Button::Down);  // [ Usar esta pasta ] -> Games
    h.tap(Button::A);     // entra em Games
    h.tap(Button::X);     // usar esta pasta
    h.waitIdle();
    CHECK(std::string(h.app.topScreenName()) == "LibrarySettings");
    CHECK(h.app.library().folders().size() == 1);
    for (int i = 0; i < 300 && (h.app.libraryScanning() || h.app.library().games().size() < 2); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        h.step(0, 1);
    }
    CHECK(h.app.library().games().size() == 2);
    h.shot("82_jogos_e_pastas");
    h.tap(Button::B);
    h.waitIdle();
    CHECK(std::string(h.app.topScreenName()) == "Library");
    h.idle(0.6f);
    h.shot("83_meus_jogos_carrossel");
    // Favoritar (X), filtro Favoritos (RB), voltar (LB)
    h.tap(Button::X);
    CHECK(h.app.library().view(Library::Filter::Favorites).size() == 1);
    h.tap(Button::RB);
    h.tap(Button::LB);
    // Pesquisa: Y -> opcoes -> Pesquisar -> teclado "OUTRO" -> START
    h.tap(Button::Y);
    h.waitIdle();
    CHECK(std::string(h.app.topScreenName()) == "ChoiceDialog");
    h.tap(Button::Down);
    h.tap(Button::A);
    h.waitIdle();
    CHECK(std::string(h.app.topScreenName()) == "Keyboard");
    auto typeKey = [&](int row, int col) {
        for (int i = 0; i < row; ++i) h.tap(Button::Down);
        for (int i = 0; i < col; ++i) h.tap(Button::Right);
        h.tap(Button::A);
        for (int i = 0; i < row; ++i) h.tap(Button::Up);
        for (int i = 0; i < col; ++i) h.tap(Button::Left);
    };
    typeKey(1, 4);  // O
    typeKey(2, 0);  // U
    h.shot("84_teclado_pesquisa");
    h.tap(Button::Start);
    h.waitIdle();
    CHECK(std::string(h.app.topScreenName()) == "Library");
    // Grade (Y -> visualizacao)
    h.tap(Button::Y);
    h.waitIdle();
    h.tap(Button::A);
    h.waitIdle();
    CHECK(h.app.library().options().view == 1);
    h.idle(0.4f);
    h.shot("85_meus_jogos_grade");
    // Detalhes e Jogar
    h.tap(Button::A);
    h.waitIdle();
    CHECK(std::string(h.app.topScreenName()) == "GameDetail");
    h.shot("86_detalhes_do_jogo");
    h.tap(Button::A);  // Jogar -> confirmacao
    h.waitIdle();
    h.tap(Button::Left);
    h.tap(Button::A);  // Sim: na previa nao executa (motivo informado), nada encerra
    h.waitIdle();
    CHECK(!h.app.quitRequested());
    std::string why;
    const std::string gid = h.app.library().view(Library::Filter::Search, "OU").empty()
                                ? h.app.library().games()[0].id
                                : h.app.library().games()[size_t(h.app.library().view(Library::Filter::Search, "OU")[0])].id;
    CHECK(!h.app.launchGame(gid, &why) && why.find("prévia") != std::string::npos);
    // Plataforma que inicia titulos: valida e encerra para iniciar o default.xex
    h.plat.setLaunchable(true);
    CHECK(h.app.launchGame(gid, &why));
    CHECK(h.app.quitRequested() && endsWithNoCase(h.app.launchOnExit(), "default.xex"));
    CHECK(h.app.library().find(gid)->lastPlayed > 0);
    h.app.shutdown();
    CHECK(fileExists(base + "dados/biblioteca.ini"));
    CHECK(h.plat.renderer().textureCount() == 0);

    section("Meus Jogos: aparencia segue a interface, mesmos jogos");
    Harness h2(base + "dados/", 1280, 720, false);
    h2.app.init();
    h2.idle(3.0f);
    h2.waitIdle();
    CHECK(h2.app.library().games().size() == 2);  // persistido
    for (int d = 0; d < kDashboardCount; ++d) {
        h2.app.openDashboard(DashboardId(d));
        h2.waitIdle();
        h2.app.openLibrary();
        h2.waitIdle();
        h2.idle(0.5f);
        CHECK(std::string(h2.app.topScreenName()) == "Library" && h2.app.libraryStyle() == DashboardId(d));
        h2.shot(std::string("87_meus_jogos_") + dashboardInfo(DashboardId(d)).key);
        h2.tap(Button::B);
        h2.waitIdle();
    }
    // Jogo apagado por fora: nao inicia, fica indisponivel
    std::remove((base + "hdd/Games/Outro Jogo/default.xex").c_str());
    h2.plat.setLaunchable(true);
    std::string why2;
    for (const GameEntry& g : h2.app.library().games())
        if (g.titleId == 0x41410042) CHECK(!h2.app.launchGame(g.id, &why2) && why2.find("não existe") != std::string::npos);
    CHECK(!h2.app.quitRequested());
    h2.app.shutdown();

    section("Perfil conectado (somente leitura) e troca de perfil");
    Harness h3(freshDir("perfil"), 1280, 720, false);
    UserProfile p;
    p.supported = true;
    p.signedIn = true;
    p.userIndex = 0;
    p.gamertag = "Jogador Teste";
    h3.plat.setProfile(p);
    h3.app.init();
    CHECK(std::string(demoProfile().gamertag) == "Jogador Teste" && demoProfile().real);
    CHECK(profilePointsText(demoProfile()) == "Gamerscore não lido");
    const unsigned v0 = h3.app.profileVersion();
    p.gamertag = "Outro Perfil";
    h3.plat.setProfile(p);
    h3.idle(2.5f);
    CHECK(h3.app.profileVersion() != v0 && std::string(demoProfile().gamertag) == "Outro Perfil");
    p.signedIn = false;
    h3.plat.setProfile(p);
    h3.idle(2.5f);
    CHECK(!demoProfile().real && std::string(demoProfile().gamertag) == "Nenhum perfil conectado");
    h3.app.openDashboard(DashboardId::Nxe);
    h3.waitIdle();
    h3.idle(0.5f);
    h3.shot("88_nxe_sem_perfil");
    h3.app.shutdown();
}

static void testSoak() {
    section("Fase 9: teste de resistencia (entradas aleatorias em todas as telas)");
    Harness h(tempDir("soak"));
    h.app.init();
    h.idle(3.0f);
    h.waitIdle();
    StrokeFont::instance().clearCache();
    h.step(0, 2);
    StrokeFont::instance().clearCache();
    long long baseHeap = g_liveBytes.load();
    int baseTex = h.plat.renderer().textureCount();
    int errorsBefore = logErrorCount();
    uint32_t seed = 0x9E3779B9u;
    auto rnd = [&seed]() { seed = seed * 1664525u + 1013904223u; return seed >> 8; };
    const Button pool[] = {Button::Up, Button::Down, Button::Left, Button::Right, Button::A, Button::B, Button::X,
                           Button::Y, Button::LB, Button::RB, Button::Start, Button::Up, Button::Down, Button::Right};
    const int poolN = int(sizeof(pool) / sizeof(pool[0]));
    int maxScreens = 0, maxTex = 0, disconnects = 0, quits = 0, dashOpens = 0;
    size_t maxTexBytes = 0;
    const int kSteps = 6000;  // ~6000 acoes, ~21000 quadros (~6 min simulados)
    for (int i = 0; i < kSteps; ++i) {
        std::string top = h.app.topScreenName();
        Button b = pool[rnd() % poolN];
        if (top == "Dialog" || top == "ChoiceDialog") b = Button::B;  // nunca confirma sair/restaurar
        if (i % 600 == 300) {
            // abre um dashboard aleatorio pelo fluxo normal
            h.app.openDashboard(DashboardId(rnd() % kDashboardCount));
            h.waitIdle();
            ++dashOpens;
        } else if (i % 997 == 500) {
            // controle desconectado por meio segundo no meio do uso
            RawPadState s;
            s.connected = false;
            s.buttons = bit(b);
            for (int k = 0; k < 30; ++k) { h.plat.advance(1.0 / 60); h.app.frame(s, 1.0f / 60); }
            ++disconnects;
        } else {
            h.step(bit(b), 1);
            h.step(0, 1 + int(rnd() % 4));
        }
        if (h.app.quitRequested()) { ++quits; break; }
        maxScreens = std::max(maxScreens, h.app.screenCount());
        maxTex = std::max(maxTex, h.plat.renderer().textureCount());
        maxTexBytes = std::max(maxTexBytes, h.plat.renderer().textureBytes());
    }
    std::printf("  %d acoes, %llu quadros, %d aberturas diretas de dashboard, %d desconexoes\n", kSteps,
                static_cast<unsigned long long>(h.app.stats().frames), dashOpens, disconnects);
    std::printf("  pilha maxima: %d telas; texturas maximas: %d (%.1f MB)\n", maxScreens, maxTex, maxTexBytes / 1048576.0);
    CHECK(quits == 0);
    CHECK(maxScreens <= 4);
    CHECK(maxTex <= 16);
    CHECK(logErrorCount() == errorsBefore);
    // Volta ao menu e confere memoria
    h.app.openMainMenu();
    h.waitIdle();
    StrokeFont::instance().clearCache();
    h.step(0, 2);
    StrokeFont::instance().clearCache();
    long long endHeap = g_liveBytes.load();
    std::printf("  heap no menu: antes %.1f KB, depois %.1f KB (dif %+lld bytes)\n", baseHeap / 1024.0, endHeap / 1024.0,
                endHeap - baseHeap);
    CHECK(h.plat.renderer().textureCount() == baseTex);
    CHECK(std::llabs(endHeap - baseHeap) < 64 * 1024);  // opcoes salvas e textos de aviso sao pequenos
    h.app.shutdown();
    CHECK(h.plat.renderer().textureCount() == 0);
}

static void testUnwritableData() {
    section("Fase 9: pasta de dados sem permissao de escrita");
    // Um caminho "dentro de um arquivo" nunca pode ser criado nem gravado.
    std::string dir = tempDir("unwritable");
    FILE* f = std::fopen((dir + "arquivo").c_str(), "wb");
    if (f) { std::fputs("x", f); std::fclose(f); }
    Harness h(dir + "arquivo/");
    int errBefore = logErrorCount();
    CHECK(h.app.init());  // inicia mesmo assim, com padroes
    h.idle(3.0f);
    h.waitIdle();
    CHECK(std::string(h.app.topScreenName()) == "MainMenu");
    CHECK(!h.app.saveSettings());  // falha informada, sem travar
    CHECK(logErrorCount() > errBefore);
    for (int d = 0; d < kDashboardCount; ++d) {
        h.app.openDashboard(DashboardId(d));
        h.waitIdle();
        CHECK(h.app.hasActiveDashboard() && int(h.app.activeDashboard()) == d);
        h.tap(Button::Y);  // mudar uma opcao tenta salvar: falha tratada
        h.tap(Button::A);
        h.tap(Button::B);
        h.step(0, 20);
    }
    CHECK(std::string(h.app.topScreenName()) != "");
    h.app.shutdown();
    CHECK(h.plat.renderer().textureCount() == 0);
}

static void testMemoryReport() {
    section("Fase 9: memoria por interface (heap + texturas, 720p e 1080p)");
    const int res[2][2] = {{1280, 720}, {1920, 1080}};
    for (const auto& rs : res) {
        Harness h(tempDir("memory"), rs[0], rs[1]);
        h.app.init();
        h.idle(3.0f);
        h.waitIdle();
        StrokeFont::instance().clearCache();
        h.step(0, 2);
        StrokeFont::instance().clearCache();
        long long menuHeap = g_liveBytes.load();
        size_t menuTex = h.plat.renderer().textureBytes();
        std::printf("  %dx%d menu: heap %.1f MB, texturas %.1f MB\n", rs[0], rs[1], menuHeap / 1048576.0, menuTex / 1048576.0);
        for (int d = 0; d < kDashboardCount; ++d) {
            h.app.openDashboard(DashboardId(d));
            h.waitIdle();
            h.idle(1.0f);
            StrokeFont::instance().clearCache();
            h.step(0, 2);
            StrokeFont::instance().clearCache();
            long long heap = g_liveBytes.load();
            size_t tex = h.plat.renderer().textureBytes();
            std::printf("  %dx%d %-9s heap %.1f MB (%+.1f), texturas %.1f MB (%+.1f)\n", rs[0], rs[1],
                        dashboardInfo(DashboardId(d)).name, heap / 1048576.0, (heap - menuHeap) / 1048576.0, tex / 1048576.0,
                        (double(tex) - double(menuTex)) / 1048576.0);
            // So o fundo e o cache da interface (2 quadros) alem das previas
            CHECK(double(tex) - double(menuTex) <= 2.0 * rs[0] * rs[1] * 4 + 1024);
        }
        h.app.shutdown();
    }
}

// Opcional: valida sons originais REAIS instalados pelo extrator. So roda se a
// variavel NEUTROX_DADOS_REAIS apontar para a pasta de dados que contem
// local_audio/ (material proprietario nunca entra no repositorio).
static void testRealOriginals() {
    const char* real = std::getenv("NEUTROX_DADOS_REAIS");
    if (!real || !*real) return;
    section("Sons originais reais (pasta local do usuario)");
    std::string dir = real;
    if (dir.back() != '/' && dir.back() != '\\') dir += '/';
    Harness h(dir, 1280, 720, false);  // pasta REAL do usuario: nao semeia biblioteca de teste
    h.app.init();
    OrigReport rep = h.plat.mixer().originalsReport();
    for (int t = 1; t < kSoundThemeCount; ++t)
        std::printf("  %-16s %d de %d efeitos com funcao, %d arquivos na pasta, %d nao confirmados, %d erros (%s)\n",
                    soundThemeInfo(SoundTheme(t)).folder, rep.loaded(SoundTheme(t)), kSfxCount, rep.filesInFolder[t],
                    rep.unconfirmed[t], rep.convErrors[t], rep.situation[t].c_str());
    int total = 0;
    for (int t = 1; t < kSoundThemeCount; ++t) total += rep.filesInFolder[t];
    CHECK(total > 0);
    h.idle(3.0f);
    h.waitIdle();
    // O dashboard com originais toca os arquivos reais (vozes criadas no tema certo)
    for (int d = 0; d < kDashboardCount; ++d) {
        h.app.openDashboard(DashboardId(d));
        h.waitIdle();
        CHECK(h.plat.mixer().theme() == soundThemeFor(DashboardId(d)));
        h.plat.mixer().stopAll();
        h.plat.mixer().play(Sfx::Confirm);
        int voices = h.plat.mixer().activeVoices();
        bool hasOrig = rep.e[int(soundThemeFor(DashboardId(d)))][int(Sfx::Confirm)].status == OrigStatus::Loaded;
        CHECK(voices == (hasOrig ? 1 : 0));  // sem original: mudo, sem substituicao
        CHECK(h.plat.mixer().residentOriginals() == int(soundThemeFor(DashboardId(d))));
    }
    h.app.openMainMenu();
    h.waitIdle();
    h.app.openSoundStatus();
    h.waitIdle();
    h.idle(0.5f);
    for (int i = 0; i < 4; ++i) h.tap(Button::Down);  // Metro V2
    h.idle(0.3f);
    h.shot("80_sons_originais_reais");
    h.app.shutdown();
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
    Harness h(dir, 1280, 720, false);  // pasta REAL do usuario: nao semeia biblioteca de teste
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

static void testParallelRenderer() {
    section("Renderizador paralelo: faixas identicas ao modo imediato, pixel a pixel");
    // 1) Cenas completas desenhadas direto (previas das cinco interfaces) em 720p e 1080p.
    const int sizes[][2] = {{1280, 720}, {1920, 1080}, {640, 480}};
    for (const auto& sz : sizes) {
        SoftRenderer a(sz[0], sz[1]), b(sz[0], sz[1]);
        b.setThreads(4);
        bool same = true;
        for (int d = 0; d < kDashboardCount; ++d) {
            for (float t : {0.0f, 1.7f}) {
                a.beginFrame();
                dashboardInfo(DashboardId(d)).drawPreview(a, t);
                a.endFrame();
                b.beginFrame();
                dashboardInfo(DashboardId(d)).drawPreview(b, t);
                b.endFrame();
                if (std::memcmp(a.pixels(), b.pixels(), size_t(sz[0]) * sz[1] * 4) != 0) same = false;
            }
        }
        CHECK(same);
        CHECK(b.lastCommandCount() > 0);
    }
    // 2) Aplicativo inteiro: mesma sequencia de entradas, uma instancia imediata e outra com 6 threads.
    // Mesma pasta de dados: o retrato da biblioteca e global e os caminhos aparecem na tela.
    Harness h1(tempDir("par_ab")), h6(tempDir("par_ab"));
    h6.plat.soft().setThreads(6);
    h1.app.init();
    h6.app.init();
    auto both = [&](uint32_t buttons, int frames) {
        h1.step(buttons, frames);
        h6.step(buttons, frames);
    };
    both(0, 200);
    int compared = 0, equal = 0;
    auto compare = [&]() {
        ++compared;
        if (std::memcmp(h1.plat.soft().pixels(), h6.plat.soft().pixels(), size_t(1280) * 720 * 4) == 0) ++equal;
    };
    compare();
    for (int d = 0; d < kDashboardCount; ++d) {
        h1.app.openDashboard(DashboardId(d));
        h6.app.openDashboard(DashboardId(d));
        both(0, 60);
        compare();
        const Button seq[] = {Button::Right, Button::Down, Button::RB, Button::Right, Button::LB, Button::Left, Button::Up};
        for (Button bt : seq) {
            both(bit(bt), 1);
            both(0, 3);
            compare();  // no meio da animacao (desenho ao vivo)
            both(0, 30);
            compare();  // parado (cache de cena)
        }
        both(bit(Button::Start), 1);  // menu global sobreposto
        both(0, 20);
        compare();
        both(bit(Button::B), 1);
        both(0, 20);
    }
    std::printf("  %d quadros comparados, %d identicos\n", compared, equal);
    CHECK(compared > 50 && equal == compared);
    h1.app.shutdown();
    h6.app.shutdown();
    CHECK(h6.plat.renderer().textureCount() == 0);

    // 3) Tempo: quadro ao vivo do pior caso medido (troca de canal do NXE), 1 x 4 threads.
    auto live = [](int threads) {
        Harness h(tempDir("par_perf"));
        h.plat.soft().setThreads(threads);
        h.app.init();
        h.idle(3.0f);
        h.waitIdle();
        h.app.openDashboard(DashboardId::Nxe);
        h.waitIdle();
        h.idle(1.0f);
        std::vector<double> ft;
        for (int round = 0; round < 6; ++round) {
            h.step(bit(round % 2 ? Button::Up : Button::Down), 1);
            for (int f = 0; f < 24; ++f) {
                double t0 = nowMs();
                h.step(0, 1);
                ft.push_back(nowMs() - t0);
            }
        }
        std::sort(ft.begin(), ft.end());
        h.app.shutdown();
        return ft[ft.size() / 2];
    };
    const double t1 = live(1), t4 = live(4);
    std::printf("  NXE troca de canal ao vivo: 1 thread %.2f ms, 4 threads %.2f ms (%.1fx)\n", t1, t4, t1 / t4);
    CHECK(t4 < t1);
}

// Percorre cada eixo de navegacao ate o fim e volta (inclusive dando a volta do ultimo
// para o primeiro) medindo o PIOR quadro de logica e de desenho, para achar travadas.
static void testNavigationHitches() {
    section("Navegacao: primeiro -> ultimo -> primeiro sem picos (cinco interfaces)");
    struct Axis { Button fwd, back; const char* name; };
    const Axis axes[] = {{Button::Right, Button::Left, "esq/dir"}, {Button::Down, Button::Up, "cima/baixo"},
                         {Button::RB, Button::LB, "LB/RB"}};
    double worstAll = 0;
    for (int d = 0; d < kDashboardCount; ++d) {
        Harness h(tempDir("nav_hitch"));
        h.plat.setRealClock(true);
        h.app.init();
        h.idle(3.0f);
        h.waitIdle();
        h.app.openDashboard(DashboardId(d));
        h.waitIdle();
        h.idle(1.0f);
        for (const Axis& ax : axes) {
            double worstUpd = 0, worstDraw = 0;
            std::vector<double> fwdT, backT;
            auto run = [&](Button b, int n, std::vector<double>& times) {
                for (int i = 0; i < n; ++i) {
                    h.step(bit(b), 1);
                    for (int f = 0; f < 8; ++f) {
                        const FrameStats& st = h.app.stats();
                        worstUpd = std::max(worstUpd, double(st.updateMs));
                        worstDraw = std::max(worstDraw, double(st.renderMs));
                        times.push_back(double(st.updateMs + st.renderMs));
                        h.step(0, 1);
                    }
                }
            };
            run(ax.fwd, 14, fwdT);   // ate o fim (e alem: da a volta onde a interface permite)
            run(ax.back, 28, backT);  // de volta ao comeco e alem
            auto p95 = [](std::vector<double> v) { std::sort(v.begin(), v.end()); return v[v.size() * 95 / 100]; };
            const double pf = p95(fwdT), pb = p95(backT);
            std::printf("  %-9s %-10s pior logica %5.2f ms, pior desenho %5.2f ms; p95 ida %5.2f, volta %5.2f\n",
                        dashboardInfo(DashboardId(d)).name, ax.name, worstUpd, worstDraw, pf, pb);
            worstAll = std::max(worstAll, worstUpd);
            // Voltar ao comeco nao pode custar muito mais que ir ao fim (travada relatada no console).
            CHECK(pb < pf * 2.0 + 2.0);
        }
        h.app.shutdown();
    }
    CHECK(worstAll < 8.0);  // logica de navegacao nunca deve pesar (o desenho e paralelo/cacheado)
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
    auto liveBench = [](DashboardId id, const char* label, Button fwd, Button back, int w = 1280, int hgt = 720) {
        Harness h(tempDir("perf_live"), w, hgt);
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
        std::printf("  %s ao vivo (%dp): mediana %.2f ms, p90 %.2f ms\n", label, hgt, med, p90);
        if (hgt <= 720) CHECK(med < 16.7);  // 1080p: somente informativo
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
    liveBench(DashboardId::Blades, "Blades troca de lamina", Button::RB, Button::LB, 1920, 1080);
    liveBench(DashboardId::Nxe, "NXE troca de canal", Button::Down, Button::Up, 1920, 1080);
    liveBench(DashboardId::Kinect, "Kinect deslize de pagina", Button::RB, Button::LB, 1920, 1080);
    liveBench(DashboardId::MetroV1, "Metro V1 troca de categoria", Button::RB, Button::LB, 1920, 1080);
    liveBench(DashboardId::MetroV2, "Metro V2 troca de categoria", Button::RB, Button::LB, 1920, 1080);
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
    testRealOriginals();
    testAppFlow();
    testBlades();
    testNxe();
    testKinect();
    testMetroV1();
    testMetroV2();
    testSwitchStressAndMemory();
    testErrorRecovery();
    testCorruptSettingsAtStartup();
    testOptionPersistence();
    testExitTargets();
    testTitleMetadata();
    testLibraryScan();
    testCovers();
    testLibraryApp();
    testParallelRenderer();
    testNavigationHitches();
    testSoak();
    testMemoryReport();
    testPerformance();
    testUnwritableData();
    std::printf("\nResultado: %d verificacoes aprovadas, %d falharam\n", g_pass, g_fail);
    logSetFile("");
    return g_fail == 0 ? 0 : 1;
}
