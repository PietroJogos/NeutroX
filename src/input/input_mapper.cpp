// Neutro X - mapeamento de entrada com repeticao. Ver input.h.
#include "input/input.h"

namespace nx {

namespace {
bool repeats(int b) {
    switch (Button(b)) {
        case Button::Up: case Button::Down: case Button::Left: case Button::Right:
        case Button::LB: case Button::RB:
            return true;
        default:
            return false;
    }
}
}  // namespace

InputMapper::InputMapper() : cfg_() {}
InputMapper::InputMapper(const Config& cfg) : cfg_(cfg) {}

void InputMapper::reset() {
    for (int i = 0; i < kButtonCount; ++i) {
        heldPrev_[i] = false;
        holdTime_[i] = 0;
        nextRepeat_[i] = 0;
    }
    for (bool& s : stickDir_) s = false;
    // Botoes que continuarem pressionados apos o reset nao geram eventos
    // ate serem soltos (evita "atravessar" telas com um unico toque).
    suppressUntilRelease_ = true;
}

InputFrame InputMapper::update(const RawPadState& raw, float dt) {
    InputFrame f;
    const bool active = raw.connected || raw.keyboardActive;
    f.connected = raw.connected;
    f.disconnectedNow = wasConnected_ && !raw.connected;
    f.connectedNow = !wasConnected_ && raw.connected;
    wasConnected_ = raw.connected;

    uint32_t buttons = active ? raw.buttons : 0u;

    // Analogico -> direcoes, com histerese.
    if (active) {
        const float vals[4] = {raw.ly, -raw.ly, -raw.lx, raw.lx};  // Up, Down, Left, Right
        for (int d = 0; d < 4; ++d) {
            if (stickDir_[d]) stickDir_[d] = vals[d] > cfg_.stickRelease;
            else stickDir_[d] = vals[d] > cfg_.stickPress;
            if (stickDir_[d]) buttons |= 1u << d;
        }
        if (raw.lt > 0.5f) buttons |= bit(Button::LT);
        if (raw.rt > 0.5f) buttons |= bit(Button::RT);
    } else {
        for (bool& s : stickDir_) s = false;
    }

    if (suppressUntilRelease_) {
        if (buttons == 0) suppressUntilRelease_ = false;
        for (int i = 0; i < kButtonCount; ++i) heldPrev_[i] = (buttons >> i) & 1u;
        return f;
    }

    for (int i = 0; i < kButtonCount; ++i) {
        bool h = (buttons >> i) & 1u;
        f.held[i] = h;
        if (h && !heldPrev_[i]) {
            f.pressed[i] = true;
            holdTime_[i] = 0;
            nextRepeat_[i] = cfg_.repeatDelay;
        } else if (h && repeats(i)) {
            holdTime_[i] += dt;
            if (holdTime_[i] >= nextRepeat_[i]) {
                f.pressed[i] = true;
                nextRepeat_[i] += cfg_.repeatInterval;
                // Se o quadro demorou muito, nao acumula varias repeticoes.
                if (nextRepeat_[i] < holdTime_[i]) nextRepeat_[i] = holdTime_[i] + cfg_.repeatInterval;
            }
        } else if (!h) {
            holdTime_[i] = 0;
        }
        heldPrev_[i] = h;
    }
    return f;
}

}  // namespace nx
