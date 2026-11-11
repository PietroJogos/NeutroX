// Neutro X - abstracao de controle.
//
// A plataforma fornece um RawPadState por quadro (controle Xbox 360 via
// XInput no Windows / API do console no Xbox 360, mais teclado como reserva
// no PC). O InputMapper converte isso em eventos com repeticao controlada.
#pragma once
#include <cstdint>

namespace nx {

enum class Button : int {
    Up = 0, Down, Left, Right,
    A, B, X, Y,
    LB, RB, LT, RT,
    Start, Back,
    Count
};
constexpr int kButtonCount = int(Button::Count);

inline uint32_t bit(Button b) { return 1u << int(b); }

struct RawPadState {
    bool connected = false;  // algum controle fisico conectado
    uint32_t buttons = 0;    // bits de Button (direcional digital + botoes)
    float lx = 0, ly = 0;    // analogico esquerdo, -1..1 (ly positivo = cima)
    float lt = 0, rt = 0;    // gatilhos 0..1
    bool keyboardActive = false;  // entrada veio do teclado (somente PC)
};

struct InputFrame {
    bool pressed[kButtonCount] = {};  // borda de descida OU repeticao
    bool held[kButtonCount] = {};
    bool connected = false;
    bool disconnectedNow = false;  // controle acabou de ser desconectado
    bool connectedNow = false;     // controle acabou de ser conectado

    bool press(Button b) const { return pressed[int(b)]; }
    bool down(Button b) const { return held[int(b)]; }
};

// Converte estado bruto em eventos de navegacao com atraso inicial e
// repeticao, evitando movimentos rapidos demais com botao segurado.
class InputMapper {
public:
    struct Config {
        float repeatDelay = 0.42f;     // segundos ate comecar a repetir
        float repeatInterval = 0.11f;  // intervalo entre repeticoes
        float stickPress = 0.55f;      // limiar do analogico (com histerese)
        float stickRelease = 0.35f;
    };

    InputMapper();
    explicit InputMapper(const Config& cfg);
    InputFrame update(const RawPadState& raw, float dt);
    void reset();  // limpa estados (ex.: ao trocar de tela)

private:
    Config cfg_;
    bool heldPrev_[kButtonCount] = {};
    float holdTime_[kButtonCount] = {};
    float nextRepeat_[kButtonCount] = {};
    bool stickDir_[4] = {};
    bool wasConnected_ = false;
    bool suppressUntilRelease_ = false;
};

}  // namespace nx
