// Neutro X - elementos de interface reutilizaveis (icones proprios, dicas de
// botoes, logotipo, indicador de carregamento). Tudo desenhado por codigo.
#pragma once
#include <initializer_list>
#include "graphics/renderer.h"
#include "input/input.h"

namespace nx {

struct DashboardInfo;

enum class Icon {
    Controller, Music, Video, Gear, Person, Friends, Bag, Search, List, Download,
    Star, Home, Globe, Power, Info, Apps, Picture, Disc, Plus, ArrowLeft, Trophy,
    Mail, Tv, Storage, Clock, Sensor, Palette, Speaker, Network, Family, Count
};

// bg = cor usada para recortes internos (normalmente a cor do fundo do bloco).
void drawIcon(IRenderer& r, Icon icon, float cx, float cy, float size, Color color, Color bg = Color(0, 0, 0, 0));
void strokeArc(IRenderer& r, float cx, float cy, float radius, float a0deg, float a1deg, float thickness, Color c);

// Circulo colorido com a letra do botao (A/B/X/Y) ou pilula (LB/RB/START...).
// Retorna a largura ocupada.
float drawButtonGlyph(IRenderer& r, Button b, float x, float cy, float size);

struct Hint {
    Button button;
    const char* label;
};
// Desenha uma linha de dicas. align Right: x e a borda direita.
float drawHints(IRenderer& r, std::initializer_list<Hint> hints, float x, float cy, float size, Color textColor,
                Align align = Align::Left);

// Logotipo "NEUTRO X". progress 0..1 anima a entrada do X.
void drawNeutroLogo(IRenderer& r, float x, float y, float size, Color accent, float progress = 1.0f);

void drawSpinner(IRenderer& r, float cx, float cy, float radius, float time, Color c);

// Texto com quebra automatica de linha. Retorna altura usada.
float drawWrapped(IRenderer& r, const char* utf8, float x, float y, float width, const TextStyle& st,
                  float lineHeight, int maxLines = 99);

// Fundo da identidade visual do Neutro X: parte estatica (pode ir para
// textura) e faixas de luz animadas (desenhadas a cada quadro).
void drawNeutroBackground(IRenderer& r, Color accent);
void drawNeutroBands(IRenderer& r, float time, Color accent);

// Posicao y do topo do texto para centralizar as maiusculas em cy.
inline float textTopForCenter(float cy, float size) { return cy - size * 0.4375f; }

// Avatar generico original (sem relacao com o servico oficial de avatares).
struct AvatarLook {
    Color skin = Color::hex(0xE8B894);
    Color hair = Color::hex(0x3A2A1E);
    Color shirt = Color::hex(0x2F7FD8);
    Color pants = Color::hex(0x2B3442);
    Color shoes = Color::hex(0xEDEDED);
};
// wave 0..1: levanta o braco direito e acena.
void drawAvatar(IRenderer& r, float cx, float footY, float height, const AvatarLook& look, float time = 0, float wave = 0);

// Arte abstrata deterministica para blocos/cartoes de demonstracao.
void drawAbstractArt(IRenderer& r, const Rectf& rc, int seed, Color c1, Color c2);

// Tela de carregamento exibida durante a troca de dashboard.
void drawLoadingScreen(IRenderer& r, const DashboardInfo& info, float time);

}  // namespace nx
