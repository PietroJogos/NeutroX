# Registro das fases

| Fase | Conteúdo | Situação |
|---|---|---|
| 1 | Análise do pacote, XEX, imagens, ferramentas, estrutura | ✅ Concluída (11/11/2026) |
| 2 | Núcleo: loop, telas, erros, abstrações de vídeo/controle/áudio, configurações | ✅ Concluída (11/11/2026) |
| 3 | Menu principal: logo, seleção com miniaturas, navegação, configurações | ✅ Concluída (11/11/2026) |
| — | Extra: temas sonoros por versão + carregador de sons originais (pasta local) | ✅ Concluído (11/11/2026) |
| 4 | Blades: lâminas, abas, animações, navegação, sons | ✅ Concluída (11/11/2026) |
| 5 | NXE: painéis, canais, animações, avatar genérico | ✅ Concluída (11/11/2026) |
| 6 | Kinect: cartões, layout, transições | ⏳ Próxima |
| 7 | Metro V1: blocos, categorias, animações | Pendente |
| 8 | Metro V2: tema escuro, blocos, biblioteca demonstrativa | Pendente |
| 9 | Integração, memória, estabilidade, desempenho | Pendente (base já testada) |
| 10 | Compilação para Xbox 360 (XEX), instalação e teste pelo Aurora | Pendente — depende da cadeia xsdk |

Política de releases: toda versão é publicada como **pré-release (beta)** até a Fase 10
terminar; só então uma versão é marcada como *Latest*.

## Fase 1 — resultado
- Relatório: [RELATORIO_FASE1.md](RELATORIO_FASE1.md).
- Versões dos cinco XEX confirmadas pelo Execution ID.
- Ferramenta `tools/xex_inspect.py` (somente leitura).
- Estrutura do projeto e build CMake funcionando.

## Fase 2 — resultado
- `App` com pilha de telas adiada, transições com tela de carregamento, tratamento de
  falhas/exceções na carga, toasts, contador de FPS.
- `IRenderer` + `SoftRenderer` + fonte vetorial própria + ícones.
- `InputMapper` com repetição, analógico com histerese e desconexão.
- `Mixer` com sons sintetizados; configurações validadas com gravação atômica.
- Plataformas: Windows (GDI + XInput + waveOut) e headless (testes).

## Fase 3 — resultado
- Abertura animada, menu inicial com as cinco interfaces (miniaturas desenhadas por código),
  Configurações, Sobre, Sair (com confirmação), menu START dentro dos dashboards com troca
  direta de interface, tela de erro.
- Dashboard padrão (abre direto ao iniciar) e ação contextual X para defini-lo.

## Fase 4 — resultado (Blades)
- Quatro lâminas (Xbox Live, Jogos, Mídia, Sistema) com rótulos girados; as anteriores
  dobram-se à esquerda e as seguintes à direita, com profundidade (encolhem e escurecem).
- Esquerda/Direita ou LB/RB "viram" as lâminas com animação; Cima/Baixo movem a barra de
  seleção animada; painel de destaque com troca suave.
- A abre subpainéis por item (perfil, amigos, conquistas com barras, biblioteca de jogos de
  demonstração, mídia, configurações somente-leitura, memória, rede); B volta; B na lista
  volta para a primeira lâmina.
- Y abre as opções da interface (fundo Clássico/Grafite, mostrar/ocultar relógio).
- Relógio, data e jogador no topo da lâmina aberta. Sons do tema Blades 6717.
- "Configurações do Neutro X" e "Sobre" abrem as telas do próprio app.
- Desempenho: cache de cena — quadros parados viram uma cópia de memória; o gradiente
  radial ficou 2× mais rápido (tabela de cores).
## Fase 5 — resultado (NXE)
- Canais empilhados no canto superior esquerdo (Meu Xbox, Biblioteca de Jogos, Mídia,
  Configurações): Cima/Baixo ou LB/RB trocam de canal com deslize vertical.
- Cartões grandes em profundidade (5 posições que encolhem para o fundo): Esquerda/Direita
  giram a "esteira" com animação; contador "x de N" e indicação do próximo canal.
- Avatar genérico próprio diante do cartão de perfil, com respiração; **X faz o avatar acenar**.
- Painéis (A): perfil, amigos, conquistas com barras, mensagens, atividade, biblioteca com
  todos os jogos de demonstração (cartões com arte; "Jogar" só informa que é demonstração),
  mídia, sistema somente-leitura, família; "Neutro X" e "Sobre" abrem as telas do app.
- **Editor de avatar**: seis cores de camiseta, com prévia ao vivo.
- **Fundo personalizado** (Y): Verde, Céu, Pôr do sol e Noite; mostrar/ocultar avatar.
- Sons do tema NXE 9199. Quadros parados em cache; o avatar anima ao vivo por cima.