// Neutro X - interface de plataforma.
//
// Separa tudo o que e especifico de sistema (janela/video, controle, audio,
// relogio, caminhos de arquivo, perfil, inicializacao de titulos) do restante
// do aplicativo.
// Implementacoes:
//   - platform/win32/    : pre-visualizacao no Windows (GDI + XInput + waveOut)
//   - platform/headless/ : testes automatizados (sem janela, entrada roteirizada)
//   - platform/xbox360/  : console (xsdk)
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "audio/audio.h"
#include "graphics/renderer.h"
#include "input/input.h"

namespace nx {

// Raiz de armazenamento que o usuario pode navegar (ex.: "hdd:/", "usb0:/").
struct StorageRoot {
    std::string path;      // caminho aceito pelas funcoes de arquivo, termina com '/'
    std::string label;     // nome exibido, ex.: "Disco rígido (Hdd1:)"
    std::string consoleName;  // nome no estilo do console/DashLaunch: "Hdd1", "Usb0"... (vazio no PC)
};

// Estado do dispositivo de audio (diagnostico).
struct AudioDeviceStatus {
    bool present = false;   // a plataforma tem saida de audio
    bool open = false;      // dispositivo aberto e alimentado
    std::string detail;     // ex.: "xsdk_audio aberto, 48 kHz, 6 canais" ou o erro
    uint64_t framesPlayed = 0;  // quadros entregues ao sistema
    uint64_t framesSilent = 0;  // quadros de silencio por atraso do alimentador
};

// Perfil do jogador conectado (somente leitura).
struct UserProfile {
    bool supported = false;  // a plataforma consegue consultar perfis
    bool signedIn = false;
    int userIndex = -1;
    std::string gamertag;
    uint64_t xuid = 0;
    std::string detail;  // origem ou motivo (ex.: "XamUserGetName", "nenhum perfil conectado")
};

class IPlatform {
public:
    virtual ~IPlatform() = default;
    virtual const char* name() const = 0;

    // Processa eventos do sistema. Retorna false se o sistema pediu para encerrar.
    virtual bool pumpEvents() = 0;
    virtual RawPadState pollInput() = 0;

    virtual IRenderer& renderer() = 0;
    virtual void present() = 0;  // mostra o quadro desenhado
    virtual IAudio& audio() = 0;

    virtual double timeSeconds() = 0;  // relogio monotonico
    virtual void waitForNextFrame(double frameStart, double targetFrameSeconds) = 0;

    // Pasta onde o Neutro X pode gravar seus PROPRIOS arquivos (termina com separador).
    virtual std::string dataPath() = 0;

    // Hora local para o relogio da interface.
    virtual void localTime(int& hour, int& minute, int& day, int& month, int& year) = 0;

    // ---- Armazenamento e titulos
    // Unidades que podem ser navegadas (somente as presentes).
    virtual std::vector<StorageRoot> storageRoots() { return {}; }
    // true se esta plataforma consegue encerrar o Neutro X e iniciar outro .xex.
    virtual bool canLaunchTitles() const { return false; }
    // Caminho do dashboard oficial do console (vazio = desconhecido/indisponivel).
    virtual std::string systemDashboardPath() { return ""; }

    // ---- Diagnosticos
    virtual AudioDeviceStatus audioStatus() { return {}; }
    virtual UserProfile userProfile() { return {}; }
};

}  // namespace nx
