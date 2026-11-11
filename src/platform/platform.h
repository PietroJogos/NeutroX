// Neutro X - interface de plataforma.
//
// Separa tudo o que e especifico de sistema (janela/video, controle, audio,
// relogio, caminhos de arquivo) do restante do aplicativo.
// Implementacoes:
//   - platform/win32/    : pre-visualizacao no Windows (GDI + XInput + waveOut)
//   - platform/headless/ : testes automatizados (sem janela, entrada roteirizada)
//   - platform/xbox360/  : porta para o console (ver README da pasta)
#pragma once
#include <string>
#include "audio/audio.h"
#include "graphics/renderer.h"
#include "input/input.h"

namespace nx {

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
};

}  // namespace nx
