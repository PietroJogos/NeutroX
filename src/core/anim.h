// Neutro X - utilitarios de animacao (interpolacao e suavizacao).
#pragma once
#include "core/types.h"

namespace nx {

namespace ease {
inline float outCubic(float t) { t = clampf(t, 0, 1); float u = 1 - t; return 1 - u * u * u; }
inline float inCubic(float t) { t = clampf(t, 0, 1); return t * t * t; }
inline float inOutCubic(float t) {
    t = clampf(t, 0, 1);
    return t < 0.5f ? 4 * t * t * t : 1 - std::pow(-2 * t + 2, 3.0f) / 2;
}
inline float outBack(float t) {
    t = clampf(t, 0, 1);
    const float c1 = 1.70158f, c3 = c1 + 1;
    float u = t - 1;
    return 1 + c3 * u * u * u + c1 * u * u;
}
inline float outQuad(float t) { t = clampf(t, 0, 1); return 1 - (1 - t) * (1 - t); }
}  // namespace ease

// Parametros globais de animacao (vindos das configuracoes).
struct AnimConfig {
    bool enabled = true;
    float speed = 1.0f;  // multiplicador: baixa=0.7, media=1.0, alta=1.4
};
AnimConfig& animConfig();

// Valor que persegue um alvo com decaimento exponencial (independente de FPS).
struct Smooth {
    float value = 0, target = 0;
    float rate = 14.0f;  // maior = mais rapido

    Smooth() = default;
    explicit Smooth(float v, float r = 14.0f) : value(v), target(v), rate(r) {}
    void set(float t) { target = t; }
    void snap(float v) { value = target = v; }
    void update(float dt) {
        const AnimConfig& c = animConfig();
        if (!c.enabled) { value = target; return; }
        float k = 1.0f - std::exp(-rate * c.speed * dt);
        value += (target - value) * k;
        if (std::fabs(target - value) < 0.0005f) value = target;
    }
    bool settled() const { return value == target; }
};

// Temporizador 0..1 com duracao fixa (para entradas/saidas).
struct Tween {
    float t = 1.0f, duration = 0.3f;
    void start(float dur) { duration = dur > 0 ? dur : 0.001f; t = 0; }
    void update(float dt) {
        const AnimConfig& c = animConfig();
        if (!c.enabled) { t = 1; return; }
        t += dt * c.speed / duration;
        if (t > 1) t = 1;
    }
    bool done() const { return t >= 1.0f; }
    float progress() const { return t; }
};

}  // namespace nx
