# Registro das fases

| Fase | Conteúdo | Situação |
|---|---|---|
| 1 | Análise do pacote, XEX, imagens, ferramentas, estrutura | ✅ Concluída (11/11/2026) |
| 2 | Núcleo: loop, telas, erros, abstrações de vídeo/controle/áudio, configurações | ✅ Concluída (11/11/2026) |
| 3 | Menu principal: logo, seleção com miniaturas, navegação, configurações | ✅ Concluída (11/11/2026) |
| — | Extra: temas sonoros por versão + carregador de sons originais (pasta local) | ✅ Concluído (11/11/2026) |
| 4 | Blades: lâminas, abas, animações, navegação, sons | ✅ Concluída (11/11/2026) |
| 5 | NXE: painéis, canais, animações, avatar genérico | ✅ Concluída (11/11/2026) |
| 6 | Kinect: cartões, layout, transições | ✅ Concluída (11/11/2026) |
| 7 | Metro V1: blocos, categorias, animações | ✅ Concluída (11/11/2026) |
| 8 | Metro V2: tema escuro, blocos, biblioteca demonstrativa | ✅ Concluída (11/11/2026) |
| 9 | Integração, memória, estabilidade, desempenho | ✅ Concluída (11/11/2026) |
| 10 | Compilação para Xbox 360 (XEX), instalação e teste pelo Aurora | 🔶 XEX gerado; correções do primeiro teste no console (0.9.0-beta); **aguardando novo teste** |

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
## Fase 6 — resultado (Kinect)
- Grade 4×2 de cartões grandes sobre ondas roxas e verdes, com título translúcido grande
  da seção.
- Seis seções como páginas lado a lado (Início, Jogos, Aplicativos, Música, Vídeos,
  Configurações): LB/RB ou passar da borda com Esquerda/Direita desliza com interpolação;
  indicador de seções embaixo.
- O cartão selecionado "salta" (escala + moldura branca) e o destaque desliza entre os
  cartões; Baixo sob uma linha mais curta vai ao último cartão dela.
- A abre páginas simplificadas de **botões grandes** (jogo, perfil, amigos, mídia,
  aplicativos, sistema somente-leitura, família); atalhos do Início levam às seções.
- **X marca favorito** (estrela no cartão). B: primeiro cartão → Início.
- Y: fundos Roxo, Azul e Grafite; mostrar/ocultar avatar (na margem esquerda).
- **Sem sensor**: um aviso fixo e o cartão "Sensor Kinect" explicam que tudo funciona
  pelo controle. Sons do tema Kinect 13604.
## Fase 7 — resultado (Metro V1)
- Fundo claro; oito categorias em tipografia leve no topo (início, social, tv, vídeo, jogos,
  música, aplicativos, configurações). O cabeçalho rola para manter a categoria atual à
  esquerda.
- Um grupo de blocos por categoria, em duas linhas, com tamanhos 1×1, 2×1 e 2×2; os grupos
  vizinhos "espiam" nas bordas. Troca de categoria com deslize horizontal e animação de
  entrada dos blocos em cascata.
- **Navegação espacial**: as setas vão ao bloco mais próximo naquela direção; na borda,
  mudam de categoria (entrando pelo bloco da borda mais próximo da linha atual).
- **Borda branca destacada** no bloco selecionado, deslizando e mudando de tamanho.
- **Cartão informativo** sob o grupo (título + descrição do bloco).
- **Cada bloco tem uma ação** (46 ao todo): páginas demonstrativas no estilo da geração ou
  telas do app (Configurações, Sobre, Sons originais).
- X fixa blocos; Y: fundo claro/cinza e cor dos blocos (verde, azul, laranja, roxo).
- Sons do tema Metro V1 15574.
## Fase 8 — resultado (Metro V2)
- Fundo escuro com destaques verdes; cinco categorias (início, jogos, filmes e tv,
  aplicativos, configurações) com sublinhado verde que desliza; grade compacta de blocos
  em três linhas, navegação espacial rápida e troca de categoria pelas bordas ou LB/RB.
- Bloco selecionado "salta" (escala, sombra, verde mais claro).
- **Biblioteca de jogos demonstrativa**: filtros por categoria (todos, ação, aventura,
  corrida, quebra-cabeça, simulação, arcade, a–z) com LB/RB, grade de miniaturas locais,
  contador, linha de informações e **tela informativa** de cada jogo (jogar → só informa
  que é demonstração; conquistas; complementos; detalhes).
- **Painéis de informações locais** com valores reais: hora/data, versão, resolução,
  dashboard atual e padrão, volume, sons originais carregados, memória de texturas, pasta
  de dados, conexão (offline).
- Configurações: atalhos para Neutro X, Sobre, Sons originais e painéis de sistema.
- Y: fundo escuro/preto e cor de destaque (verde, azul, laranja); X fixa blocos.
- Sons do tema Metro V2 17559.
- Também: glifo "×" adicionado à fonte e ferramenta `tools/glyph_check.py`, que verifica se
  todo texto da interface tem glifo; o dashboard provisório (não mais usado) foi removido.

**Com a Fase 8, as cinco interfaces estão completas.**
## Fase 9 — resultado (integração e otimização)
- **Memória**: o cache de cena passou a capturar o próprio quadro (`captureTexture`) em
  vez de manter uma tela fora da tela e uma cópia duplicada do fundo. Cada interface agora
  custa **+3,5 MB em 720p / +7,9 MB em 1080p** acima do menu (antes ≈ +10,5 / +23,7 MB).
- **Integração**: as opções de cada interface (fundos, avatar, camiseta, cores, relógio)
  são gravadas em `neutrox.ini` (`opt.<interface>.<opção>`) e restauradas ao reabrir;
  "Restaurar padrões" também as limpa.
- **Resistência**: teste com 6.000 ações aleatórias (~21.600 quadros) em todas as telas,
  com desconexões do controle — sem erros, pilha máxima de 3 telas e memória de volta ao
  valor inicial.
- **Erros**: pasta de dados sem permissão de escrita — o app inicia, todas as interfaces
  abrem e as falhas de gravação são registradas sem travar.
- **Desempenho**: medições ao vivo de todas as animações em 720p e 1080p; relatório de
  memória por interface.
## Extra (0.7.1) — extração automática dos sons originais
- `tools/xex_unpack.cpp` (`neutrox_xex_unpack`): extrai a imagem de XEX **não
  criptografados** (LZX), verifica MZ/PE e grava os pacotes de recursos; recusa os
  criptografados.
- `tools/sound_extract.py`: processamento em lote — versão pelo Execution ID, áudios
  embutidos nos pacotes XUIZ, conversão XMA → WAV (FFmpeg), funções só por nome original,
  instalação em `%LOCALAPPDATA%\NeutroX\sons_originais`, `ausentes.ini` com causas.
- Resultado com os arquivos fornecidos: **Metro V2 17559 → 16 áudios, 6 de 8 efeitos**;
  Blades, NXE, Kinect e Metro V1 continuam fechados (criptografados).
- Tela "Sons originais: estado": total de arquivos encontrados, arquivos por versão,
  arquivo real de cada efeito e a causa registrada de cada ausência.
## Extra (0.7.2) — análise dos cinco XEX e bancos XACT
- `tools/sound_extract.py` reescrito: relatório técnico por versão (cabeçalho XEX2, bibliotecas
  de áudio, pacotes), leitura de bancos **XACT3** (`.xwb` + `.xsb`: cue → onda), cenas/skins
  `.xur` como evidência de função, referências externas (`sharedres://`), validação de cada WAV
  (canais, taxa, duração contra o banco, silêncio), status confirmado / alternativa /
  sem evento / não confirmado, `manifesto.ini` por versão e `manifesto.json`.
- **Correção:** `dash.xwb`/`dash.xsb` estão **dentro** do XEX do Metro V2 (pacote `dashcomm`),
  não fora. Resultado: **Metro V2 17559 → 52 áudios, 7 de 8 efeitos**, 17 não confirmados,
  0 erros de conversão (falta "entrada"). As outras quatro versões seguem criptografadas.
- Neutro X: pasta `local_audio/` (aceita a antiga `sons_originais/`); só o conjunto do
  dashboard ativo fica na memória e a troca de dashboard descarrega o anterior; a tela de
  estado mostra não confirmados, erros de conversão, esperados × carregados × ausentes,
  recursos externos e estado do XEX.
- Relatório completo: [RELATORIO_AUDIO_XEX.md](RELATORIO_AUDIO_XEX.md).

## Fase 10 — andamento (0.8.0-beta)
- Cadeia do xsdk construída no WSL (GCC 15.3.0 `powerpc-xsdk-elf`, binutils, newlib,
  glslang-xenon), com autorização do usuário; biblioteca e exemplos do xsdk compilados.
- `src/platform/xbox360/`: vídeo (quadro da CPU → textura → quad na GPU → resolve → apresentação),
  controles (`XamInputGetState`), áudio (`xsdk_audio`, 48 kHz), arquivos em `game:/`, log ao lado
  do `.xex`.
- `NeutroX.xex` compilado sem avisos; cabeçalho conferido com `tools/xex_inspect.py` (XEX2,
  título `0x4E455558`, versão 0.8.0.0, sem criptografia, importa só `xam.xex` e `xboxkrnl.exe`).
- **Não testado em hardware.** O Xenia não apresenta a imagem de títulos do xsdk; a
  conclusão da fase depende do teste no console descrito em
  [INSTALACAO_XBOX.md](INSTALACAO_XBOX.md). Só depois disso uma versão vira *Latest*.

## Fase 10 — correções do primeiro uso no console (0.9.0-beta)

Problemas relatados no console real e o que foi feito (detalhes nos documentos indicados):

| Relato | Feito | Documento |
|---|---|---|
| O Xbox reiniciava ao sair | Encerramento ordenado (áudio parado antes de fechar o driver, GPU e tela liberadas, log de cada etapa) e saída por troca de aplicativo: **Voltar ao Aurora / Voltar ao Xbox Dashboard / Cancelar**. Nenhuma função de reinício é usada | [SAIDA_E_INICIALIZACAO.md](SAIDA_E_INICIALIZACAO.md) |
| Travadas e quedas ao usar dashboards e ao voltar ao começo | Medição por quadro (lógica/desenho/tela) na tela e no log; retângulos e misturas 3–9× mais rápidos; rasterização paralela em 6 threads, idêntica pixel a pixel | [DESEMPENHO.md](DESEMPENHO.md) |
| Nenhum som no console | Causa: originais ausentes no console com a fonte "Originais" (mudo por projeto). Instalador por FTP, diagnóstico de saída/mixer/pedidos, teste de saída, terceira fonte explícita, WAV float big-endian | [SONS.md](SONS.md) |
| Biblioteca real (Meus Jogos) | Busca por cabeçalhos reais (default.xex, GOD, XBLA), pastas, profundidade, ignorados, duplicatas, favoritos, pesquisa, detalhes, Jogar, capas do XboxUnity com cache e importação, uma biblioteca com aparência por interface | [BIBLIOTECA.md](BIBLIOTECA.md) |
| Avatar do perfil | Nome do perfil real (XAM, somente leitura) e troca de perfil; avatar 3D e imagem do jogador indisponíveis no xsdk (explicado) | [LIMITACOES.md](LIMITACOES.md) |

Nenhum item acima foi testado no Xbox 360: a Fase 10 continua aberta até o novo teste.
