# Arquitetura do Neutro X

## Camadas

```text
┌────────────────────────────────────────────────────────────┐
│ shell/       Boot · Menu inicial · Configurações · Sobre ·  │
│              Menu START · Diálogo · Erro · Estado dos sons  │
│ dashboards/  Blades · NXE · Kinect · Metro V1 · Metro V2    │
├────────────────────────────────────────────────────────────┤
│ core/App     loop · pilha de telas · transições · troca de  │
│              dashboard (descarrega → carrega) · toasts      │
├───────────────┬───────────────┬──────────────┬─────────────┤
│ graphics/     │ input/        │ audio/       │ settings/   │
│ IRenderer     │ InputMapper   │ Mixer, temas │ neutrox.ini │
│ SoftRenderer  │ (repetição,   │ por versão,  │ (atômico,   │
│ StrokeFont    │  analógico,   │ sons orig.   │  validado)  │
│ ícones/UI     │  desconexão)  │ (WAV local)  │             │
├───────────────┴───────────────┴──────────────┴─────────────┤
│ platform/IPlatform: vídeo · entrada · áudio · relógio · I/O │
│   win32 (prévia PC)   headless (testes)   xbox360 (Fase 10) │
└────────────────────────────────────────────────────────────┘
```

Tudo acima de `platform/` forma a biblioteca `neutrox_core`: C++17 puro, sem APIs de
sistema, pensada para compilar também para PowerPC.

## Coordenadas e resolução

A interface é escrita em coordenadas lógicas **1280×720**. O renderizador escala de forma
uniforme para a resolução real e centraliza com barras quando a proporção não é 16:9
(testado em 1280×720, 1920×1080 e 640×480).

## Renderização

- `IRenderer`: retângulos (com cantos arredondados e antialiasing), gradientes linear e
  radial, círculos, linhas, polígonos convexos, texto (inclusive girado 90°), recorte,
  opacidade e deslocamento em pilha, texturas.
- `SoftRenderer`: rasterizador em CPU para buffer 0xAARRGGBB. Fundos estáticos são
  desenhados uma vez numa textura na resolução nativa e copiados 1:1 a cada quadro.
- **Fonte própria** (`StrokeFont`): glifos definidos como traços vetoriais (geométrica,
  com acentos do português), rasterizados por distância com antialiasing e cache por
  tamanho/peso (limite ~8 MB). Três pesos: leve, regular, negrito.
- Ícones, avatar genérico e artes de demonstração são desenhados com primitivas.

## Telas e transições

- `Screen`: `onEnter/onExit/update/render`, `isOverlay()` (desenha a tela de baixo) e
  `wantsGlobalMenu()` (START abre o menu do Neutro X).
- Operações na pilha são **adiadas** para o fim do quadro (uma tela nunca é destruída
  enquanto executa).
- Troca de dashboard: escurecer → tela de carregamento (≥1 quadro) → **descarregar a
  interface anterior e tudo acima dela** → criar → `load()` → entrar com animação.
  Se `load()` falhar ou lançar exceção, a interface é descarregada e a tela de erro aparece;
  o app continua funcionando.
- Só um dashboard fica carregado por vez (verificado nos testes: pico de texturas igual
  ao da base + um fundo).

## Dashboards

Cada interface é um `Dashboard` (`src/dashboards/<nome>/<nome>_dashboard.cpp`) com
`load()/unload()` explícitos. Padrões comuns:

- **Estado + funções de desenho puras**: o mesmo código desenha o dashboard ao vivo e a
  miniatura do menu (`drawXxxPreview`), então a prévia sempre corresponde à interface.
- **Cache de cena** (`graphics/scene_cache.h`): quando a cena para, o quadro desenhado é
  capturado para uma única textura (`IRenderer::captureTexture`); enquanto nada anima, o
  quadro é só uma cópia de memória. Elementos que animam sempre (avatar) ficam numa camada
  ao vivo por cima. Numa GPU, a captura corresponde a "resolver" o alvo para textura.
- **Opções persistentes**: cada interface lê/grava suas opções em `Settings::dashOptions`
  (`opt.<interface>.<opção>` no `neutrox.ini`).
- **Conteúdo de demonstração compartilhado** (`dashboards/common/demo_content.*`): jogos,
  amigos, mídias e perfil fictícios, usados por todas as interfaces e pela biblioteca.
- **Ganchos de teste**: `debugState()` e `debugSelect()` permitem que os testes verifiquem a
  navegação com precisão e visitem cada bloco/cartão.

| Interface | Estrutura de navegação |
|---|---|
| Blades | 4 lâminas (Esq./Dir., LB/RB) × lista vertical × subpainéis |
| NXE | 4 canais (Cima/Baixo, LB/RB) × esteira de cartões em profundidade × painéis |
| Kinect | 6 páginas (LB/RB ou bordas) × grade 4×2 × páginas de botões grandes |
| Metro V1 | 8 categorias × grupo de blocos com navegação espacial × páginas de detalhe |
| Metro V2 | 5 categorias × grade espacial × biblioteca com filtros, telas de jogo e painéis locais |

## Entrada

`RawPadState` (por quadro, da plataforma) → `InputMapper` → `InputFrame`:
- borda de pressionamento + repetição (0,42 s de atraso, depois a cada 0,11 s) só para
  direcionais e LB/RB; quadros longos não geram rajadas;
- analógico esquerdo vira direcional com histerese (0,55 / 0,35);
- desconexão: nenhum evento enquanto desconectado, aviso na tela, nada trava;
- após trocar de tela, botões ainda segurados são ignorados até serem soltos.

## Áudio

- `Mixer`: 12 vozes, interpolação linear, volume mestre, thread-safe; a plataforma puxa
  amostras (`mix()`).
- Temas: `NeutroX` (shell) + um por versão. O tema acompanha a tela do topo.
- Fonte dos sons dos dashboards: **originais** (WAV da pasta local do usuário; ausentes
  ficam mudos) ou **recriados** (escolha explícita). Detalhes em [SONS.md](SONS.md).

## Configurações

`neutrox.ini` na pasta de dados do aplicativo: chave=valor, validação por campo
(valores inválidos voltam ao padrão e o arquivo é reparado), gravação atômica
(arquivo temporário + renomear).

## Memória e recursos

- Texturas contabilizadas (`textureCount/textureBytes`); prévias das cinco interfaces
  ficam em 640×360 e 160×90 (~5,1 MB no total).
- Teste de estresse: 100 trocas de dashboard → heap e texturas voltam exatamente ao valor
  inicial.
- Custo por interface carregada: fundo + cache = 2 quadros na resolução real
  (+3,5 MB em 720p, +7,9 MB em 1080p acima do menu).
