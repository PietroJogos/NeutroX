# Relatório de testes — 0.9.0-beta

Executado em 12/11/2026 · Windows 11 · MSVC 2022 (Release, x64) · plataforma *headless*.
Comando: `build/Release/neutrox_tests.exe build/test_output`

**Resultado: 1146 verificações aprovadas, 0 falharam**; com o teste opcional da pasta real
(`NEUTROX_DADOS_REAIS`), **1162 aprovadas, 0 falharam**. As verificações das Fases 1–9 (946
na Fase 9, 977 na 0.8.0) continuam todas no conjunto. As que dependiam dos 8 jogos fictícios
agora usam uma biblioteca **de teste** com 8 títulos sintéticos (cabeçalhos XEX2 gerados na
pasta do teste), que nunca aparece no aplicativo.

## Novidades da 0.9.0

| # | Prioridade do pedido | Como foi verificado | Resultado |
|---|---|---|---|
| 1 | Sair para o Aurora | `launch.ini` simulado (lido sem alterar; bytes conferidos antes/depois), local padrão, caminho configurado com prioridade, caminhos `Hdd:`/`Hdd1:`/`Usb:`; no diálogo, "Voltar ao Aurora" encerra e deixa o `.xex` do Aurora para iniciar | ✅ (PC) |
| 2 | Sair para o Xbox Dashboard | `dash.xex` ausente: opção desabilitada com o motivo, nada encerra; presente: troca para ele | ✅ (PC) |
| — | Cancelar / B | Volta ao menu; nada encerra | ✅ |
| 3 | Primeiro → último → primeiro | Os 3 eixos das 5 interfaces, indo além do fim e dando a volta: pior lógica 0,11 ms; p95 da volta no mesmo patamar da ida | ✅ (PC) |
| 4 | Trocar de dashboard repetidamente | 100 trocas (teste existente) + soak de 6.000 ações, agora incluindo Meus Jogos, detalhes, navegador e teclado | ✅ |
| 5 | Abrir e fechar menus | START, diálogo de saída, opções, teclado, navegador | ✅ |
| 6 | Adicionar pasta | Pelo navegador: unidade → pasta → "usar esta pasta" → busca em segundo plano → 2 jogos | ✅ |
| 7 | Reconhecer jogo real | XEX2 válido aceito; DLL, patch, sem marca de título, sem Execution ID, lixo e cabeçalho truncado recusados; STFS GOD/XBLA aceitos; save/perfil ignorados; multi-disco; o próprio Neutro X ignorado | ✅ |
| 8 | Evitar duplicatas | Mesma Media ID em HDD e USB: 2 entradas, 1 visível; detalhes contam a cópia | ✅ |
| 9 | Baixar capa válida | Servidor HTTP **local** de teste com o formato do XboxUnity (JSON + JPEG servido como `image/png`): escolhe a oficial, valida, grava sem `.part`/`.tmp` sobrando; em cache não baixa de novo | ✅ |
| 10 | Sem internet | Porta fechada: falha com motivo, capa salva continua; HTTPS recusado com motivo | ✅ |
| 11 | Trocar de perfil | Perfil simulado: nome nas interfaces, troca detectada em ≤ 2 s, "Nenhum perfil conectado" | ✅ (PC) |
| 12 | Recuperação após falhas | Jogo apagado por fora: não inicia (motivo), fica "indisponível"; `biblioteca.ini` corrompido carrega vazio sem travar; filtro sem resultado (falha real encontrada no soak e corrigida) | ✅ |
| 13 | Iniciar jogo | Prévia: recusa com motivo, nada encerra; plataforma que inicia títulos: revalida o cabeçalho e encerra para iniciar o `default.xex`; GOD/XBLA recusado com o experimental desligado | ✅ (PC) — **console não testado** |
| — | Renderizador paralelo | 3 resoluções × 5 cenas e 81 quadros do aplicativo inteiro (animações, cache, menus) com 1 e 6 threads: **idênticos pixel a pixel** | ✅ |
| — | Decodificadores | PNG com deflate dinâmico e alfa; JPEG 4:2:0; recorte da frente da capa; HTML recusado; JPEG truncado não trava. Conferidos contra o Pillow nas capas reais: diferença média < 2/255 | ✅ |
| — | Biblioteca persistente | Ida e volta do `biblioteca.ini`; sem mudanças **não grava de novo**; favoritos e datas preservados entre buscas; remover não apaga o arquivo e não volta na busca seguinte; remover pasta monitorada | ✅ |
| — | Pasta real do usuário | O teste opcional não cria nem altera biblioteca na pasta real (conferido) | ✅ |

| # | Teste obrigatório | Como foi verificado | Resultado |
|---|---|---|---|
| 1 | Inicialização sem travamento | `App::init` + abertura → menu; também com configuração corrompida e com dashboard padrão | ✅ |
| 2 | Funcionamento do controle | Bordas, repetição (6–8 eventos/s segurando), analógico com histerese, sem rajada após quadro longo | ✅ |
| 3 | Seleção das cinco interfaces | Pelo menu (setas + A), cada uma carrega e vira a tela ativa | ✅ |
| 4 | Navegação entre seções | LB/RB em todas; no Blades: lâminas, lista, subpainéis, opções e limites | ✅ |
| 5 | Troca repetida | 100 trocas seguidas; pilha sempre com 1 tela | ✅ |
| 6 | Uso de memória | Heap vivo antes/depois das 100 trocas: **diferença 0 bytes**; texturas voltam a 11 (8,2 MB); pico 11,7 MB (só uma interface carregada por vez) | ✅ |
| 7 | Taxa de quadros | Tempo de CPU por quadro (tabelas abaixo) | ✅ (PC) |
| 8 | Integridade das configurações | Ida e volta, valores inválidos, arquivo binário, gravação atômica, alteração pela interface | ✅ |
| 9 | Recuperação de erros | Dashboard que falha e que lança exceção → tela de erro → menu → outra interface abre | ✅ |
| 10 | Encerramento seguro | Diálogo com "Não" por padrão; após sair, 0 texturas restantes | ✅ |
| — | Controle desconectado | 2 s desconectado dentro de um dashboard, aviso exibido, sem travar; reconexão | ✅ |
| — | Sons por versão | Tema muda com o dashboard; menu START usa sons do Neutro X; WAV/XMA/mapeamento; ausente = mudo | ✅ |
| — | Resoluções | 1280×720, 1920×1080 (escala 1,5) e 640×480 (barras) | ✅ |
| — | **Resistência (Fase 9)** | 6.000 ações aleatórias com semente fixa (~21.600 quadros) em todas as telas, 10 aberturas diretas de interface, 6 desconexões do controle: **0 erros**, pilha máxima de 3 telas, até 13 texturas; ao voltar ao menu, texturas iguais e heap +1 KB (opções salvas) | ✅ |
| — | **Opções persistentes (Fase 9)** | Tema/avatar do NXE, grafite do Blades e destaque do Metro V2 sobrevivem a reinício; chaves inválidas rejeitadas; "restaurar padrões" limpa | ✅ |
| — | **Pasta sem escrita (Fase 9)** | Inicia com padrões, as 5 interfaces abrem, mudar opções tenta salvar e a falha é registrada sem travar | ✅ |
| — | **Metro V2 (Fase 8)** | Navegação espacial e bordas, painéis de informações locais, biblioteca (limites da grade, tela do jogo, "jogar" não executa, **8 filtros com contagens 8/2/…/3/8**), atalhos de configurações, fixar, opções sem vazar textura, **os 45 blocos executam sua ação e voltam ao estado inicial**, nenhum erro registrado | ✅ |
| — | Texto da interface | `tools/glyph_check.py`: todo caractere usado nos textos tem glifo na fonte própria (encontrou e corrigiu "×") | ✅ |
| — | **Metro V1 (Fase 7)** | Navegação espacial (2×2 → 1×1, borda → categoria e volta pelo bloco mais à direita), limites, LB/RB, páginas demonstrativas, blocos que abrem Configurações/Sobre/Sons, fixar, opções sem vazar textura, **os 46 blocos das 8 categorias executam sua ação e voltam**, nenhum erro registrado | ✅ |
| — | **Kinect (Fase 6)** | Grade com limites, passagem de página pelas bordas, Baixo sob linha curta, LB/RB, favoritos, página de botões (Jogar não executa, Voltar), cartão do sensor, atalhos do Início, 3 fundos sem vazar textura, **os 38 cartões das 6 seções abrem e fecham**, nenhum erro registrado | ✅ |
| — | **NXE (Fase 5)** | Canais e cartões com limites, esteira e troca de canal animadas, avatar acena (X) e não acena oculto, painel de jogo ("Jogar" não executa), editor de avatar, 4 fundos sem vazar textura, todos os cartões de todos os canais abrem/fecham, nenhum erro registrado | ✅ |
| — | **Blades (Fase 4)** | 4 lâminas, limites, animação de troca, 6 subpainéis por lâmina abrem/fecham, rolagem até o último jogo, opções (fundo grafite recria a textura sem vazar), atalhos para Configurações/Sobre, nenhum erro registrado | ✅ |

## Desempenho (CPU, renderizador em software, PC de desenvolvimento)

**0.9.0** (1 thread; no console são 6), mediana ao vivo em 720p: Blades 3,55 ms (era 9,82),
NXE troca de canal 3,59 ms (era 11,69), Kinect 2,08 ms (era 5,98), Metro V1 1,08 ms, Metro V2
1,95 ms, menu principal 2,95 ms (era 8,31). Em 1080p, NXE troca de canal 6,28 ms (era 24,45).
Com 4 threads no PC, a troca de canal do NXE cai mais 2,8–3,8×. Detalhes em
[DESEMPENHO.md](DESEMPENHO.md). As tabelas abaixo são as da 0.8.0, para comparação.

Média por quadro logo após abrir cada tela (inclui a animação de entrada):

| Tela | 1280×720 | 1920×1080 |
|---|---:|---:|
| Menu principal | 4,6 ms | 10,3 ms |
| Blades | 3,1 ms | 6,9 ms |
| NXE | 1,8 ms | 4,0 ms |
| Kinect | 1,6 ms | 3,7 ms |
| Metro V1 | 1,0 ms | 2,2 ms |
| Metro V2 | 0,9 ms | 2,0 ms |

Pior caso realista, ao vivo durante animações (720p):

| Animação | Mediana | p90 |
|---|---:|---:|
| Blades — troca de lâmina | 6,5 ms | 7,6 ms |
| NXE — troca de canal | 9,1 ms | 11,4 ms |
| NXE — esteira de cartões | 3,8 ms | 4,5 ms |
| Kinect — deslize de página | 4,3 ms | 5,7 ms |
| Kinect — destaque na grade | 3,3 ms | 3,7 ms |
| Metro V1 — troca de categoria | 2,2 ms | 3,1 ms |
| Metro V1 — borda entre blocos | 1,9 ms | 2,2 ms |
| Metro V2 — troca de categoria | 2,6 ms | 3,8 ms |
| Metro V2 — navegação entre blocos | 2,1 ms | 2,9 ms |

Em **1080p**, ao vivo (somente informativo):

| Animação | Mediana | p90 |
|---|---:|---:|
| Blades — troca de lâmina | 15,2 ms | 19,4 ms |
| NXE — troca de canal | **22,2 ms** | 31,1 ms |
| Kinect — deslize de página | 13,9 ms | 17,3 ms |
| Metro V1 — troca de categoria | 5,9 ms | 8,8 ms |
| Metro V2 — troca de categoria | 6,0 ms | 9,2 ms |

Em 1080p, a troca de canal do NXE fica abaixo de 60 FPS no PC (≈45 FPS durante a
animação, que dura ~0,3 s); os quadros parados continuam vindo do cache.

## Memória por interface (Fase 9)

Heap total e texturas com a interface aberta (o menu é descarregado):

| Resolução | Menu | Cada interface | Diferença |
|---|---:|---:|---:|
| 1280×720 | 13,4 MB (8,2 MB texturas) | 16,9 MB (11,7 MB texturas) | **+3,5 MB** |
| 1920×1080 | 22,2 MB (12,6 MB texturas) | 30,1 MB (20,5 MB texturas) | **+7,9 MB** |

A diferença de heap é igual à de texturas: cada interface guarda só o fundo e o cache de
cena. Antes da Fase 9 o cache usava uma tela fora da tela e uma cópia do fundo (≈ +7 MB a
mais por interface em 720p).

Com a interface parada, o quadro vem do cache de cena (cópia de memória); no NXE só o
avatar é desenhado ao vivo por cima.

Os valores variam entre execuções (a máquina estava com outros programas abertos) e
**não representam o Xbox 360**.

## Prévia Windows real
`NeutroX.exe --frames 300 --mute --data <pasta> --screenshot final.png` com
`default_dashboard=metro_v2`: abriu direto no Metro V2, 300 quadros, pior quadro 11,2 ms, saída limpa.

## Extração dos sons originais (local, fora do repositório)

| Verificação | Resultado |
|---|---|
| XEX criptografados (6717, 9199, 13604, 15574) | recusados com motivo registrado (sem chaves, sem contorno) ✅ |
| Descompressão do 17559 (`neutrox_xex_unpack`) | imagem de 16.941.056 bytes, MZ+PE válidos, 36 pacotes `XUIZ`; **idêntica** nas compilações MSVC e GCC ✅ |
| Áudios embutidos encontrados (0.7.2) | banco XACT3 `dashcomm/dash.xwb` (36 ondas XMA2) + `dash.xsb` (51 cues) + 16 RIFF XMA do `controlp` ✅ |
| Conversão e validação (FFmpeg `xma1`/`xma2`) | 52 de 52; canais/taxa conferidos; duração de cada onda = declarada no banco (≤ 768 amostras de diferença); nenhum vazio ou silencioso; 0 erros ✅ |
| Cue → onda | 51 cues mapeadas; pares L/R compartilham a mesma onda (`swipe_L/R_success` → 4, `peek_L/R` → 18), coerente com o banco ✅ |
| Funções por evidência | 7 de 8 (navegar `btn_Focus`, confirmar `snd_buttonselect`, voltar `snd_buttonback`, categoria `tab_switch`, abrir `snd_panelunfold`, erro `btn_InactiveSelect`, transição `snd_transitionfrom`) ✅ |
| Não confirmados | 17 (guardados, não usados), inclusive `btn_InactiveFocus` por taxa incerta ✅ |
| Ausências registradas com causa | entrada (nenhuma cue de abertura) e, nas 4 versões criptografadas, todos os efeitos ✅ |
| Carregador do Neutro X | `local_audio/` (aceita `sons_originais/`), lê `manifesto.ini`; só o conjunto do dashboard ativo fica na memória, troca descarrega o anterior ✅ |
| Teste opcional com a pasta real (`NEUTROX_DADOS_REAIS`) | Metro V2: 7 de 8 efeitos carregados, 52 arquivos, 17 não confirmados, 0 erros; demais versões 0 ("sem acesso: XEX criptografado"); cada interface deixa residente só o próprio conjunto ✅ |
| Texto da interface | `glyph_check.py` agora também decodifica escapes `\x..`; encontrou e corrigiu "…" ✅ |

## Compilação para o Xbox 360 (0.9.0)

`NeutroX.xex` 0.9.0 compilado sem avisos de compilação (≈2,9 MB). Cabeçalho: XEX2, título
`0x4E455558`, versão 0.9.0.0, sem criptografia, importa só `xam.xex` e `xboxkrnl.exe`. As
linhas `xenia_scan` do build são avisos informativos da ferramenta do xsdk sobre o Xenia, não
sobre o console.

## Compilação para o Xbox 360 (Fase 10, 0.8.0)

| Verificação | Resultado |
|---|---|
| Cadeia `powerpc-xsdk-elf` (GCC 15.3.0) e glslang-xenon | construídas no WSL; `TOOLCHAIN_OK` ✅ |
| xsdk: `make` e `make samples` | `libxsdk.a`, `libxsc.so` e os exemplos (`hello`, `cube`, `sound`, `triangle`, `template`) compilados ✅ |
| `NeutroX.xex` | compilado sem avisos (≈2,4 MB) ✅ |
| Cabeçalho (`tools/xex_inspect.py`) | XEX2, título `0x4E455558`, versão do projeto, sem criptografia, importa `xam.xex` e `xboxkrnl.exe`, pilha de 1 MB ✅ |
| Execução no console | **não testada** (ver abaixo) |

## Não testado
- **Hardware Xbox 360 / Aurora / RGH**: o `NeutroX.xex` não foi executado em um console.
  Roteiro do teste: [INSTALACAO_XBOX.md](INSTALACAO_XBOX.md). O Xenia não apresenta a imagem
  de títulos do xsdk e por isso não foi usado como prova.
- Controle físico do Xbox 360 no PC: o caminho XInput está implementado, mas não foi
  exercitado nesta sessão (os testes usam entrada simulada).
- Áudio audível: o mixer é testado numericamente; a saída waveOut não foi ouvida aqui.
