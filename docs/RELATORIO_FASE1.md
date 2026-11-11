# Neutro X — Relatório técnico da Fase 1 (análise e preparação)

Data: 11/11/2026 · Versão do projeto: 0.1.0

## 1. Pacote recebido

Arquivo: `NeutroX_Pacote_5_Dashboards.zip` (28.300.618 bytes), extraído numa pasta
temporária isolada. Nenhum script do pacote foi executado (`BAIXAR_IMAGENS.bat/.ps1`
e `EMPACOTAR_COM_IMAGENS.bat` foram apenas lidos: baixam imagens e recompactam a pasta;
não tocam em XEX nem no console).

| Arquivo | Bytes | SHA-256 |
|---|---:|---|
| 01_Blades_6717/dash_6717.xex | 4.374.528 | `f8a51ea30406e3b8737e0cdbdad44867743869d59f58d4bc0f4b83e154cb87d8` |
| 02_NXE_9199/dash_9199.xex | 4.845.568 | `7dd4dd087e1ff8874951446f5870445dcaaac383ddd49818f9ac84fcc2a61c48` |
| 03_Kinect_13604/dash_13604.xex | 6.189.056 | `ba2dbb8ea029fcc9e30fd250c4e12709bd5687afa0012671e49d7a29cc773a31` |
| 04_Metro_V1_15574/dash_15574.xex | 6.545.408 | `4a4a66baf685837255756c68beee9a6bfd7e9d09d146def2814e2e63b7f0753b` |
| 05_Metro_V2_17559/dash_17559.xex | 5.971.968 | `3ad8f38d070eaf63cadac0697e3e8802c565cbfd0825a485347c5d718014f927` |
| imagens/01_Blades_6717.jpg | 90.526 | `5017a6614a060947ea8d7026804ce1fd3d6132fa4f8c33c1052e4d3a6e715e5d` |
| imagens/02_NXE_9199.webp | 35.332 | `5021021f41d2455a9fd69dffde93f15c9610edb08123811d0b46d3c4b1eb3d21` |
| imagens/03_Kinect_13604.jpg | 54.364 | `8cdd40ff4cc6e8d372018dc964c6d4973f962370048863238cb33855a7fe2406` |
| imagens/04_Metro_V1_15574.jpg | 297.803 | `f3a86cd944223a3096edf70c4d5f95b8f685f8fcd8324e0989d3a1512a962645` |
| imagens/05_Metro_V2_17559.jpg | 27.104 | `f4c0159ddfbed5f7e64ddbf755b1d07465823163ad0b409129569e3781a8fa23` |
| MANIFESTO.json | 3.292 | `c2e7574bd9ef7e11a02098164fbfbf8bb37fee903b313ce1e996e80187031920` |
| README_LEIA_PRIMEIRO.txt | 1.861 | `0ec860043ee66788320d9072e1beb533a0d68c39798e00c0be2772aa537b8e9c` |
| PROMPT_PARA_CODEX_OU_CLAUDE.txt | 1.360 | `2d7944b59bb21a459ce15038f38f9402330259facef25937448d67de04238b94` |
| REFERENCIAS_IMAGENS.html | 4.090 | `251db75e5bd578a48e340eb8344fed19d3c7b95f5e2dff7bfab810e9eb7c5681` |
| SHA256SUMS.txt | 482 | `7354f0c883eb0e7e8981996b47d4532d68e1bda5c87a8813f0a7de33c56f92dc` |
| BAIXAR_IMAGENS.bat / .ps1, EMPACOTAR_COM_IMAGENS.bat | 116 / 1.905 / 370 | (scripts — não executados) |
| 5× IMAGEM_REFERENCIA.url, 5× LEIA_SOBRE_IMAGEM.txt, imagens/LEIA.txt | — | (atalhos e notas) |

**Integridade:** os SHA-256 dos cinco XEX conferem com `SHA256SUMS.txt` e `MANIFESTO.json`.
Nenhum arquivo corrompido.

**Inconsistências encontradas**
- `README_LEIA_PRIMEIRO.txt` e `imagens/LEIA.txt` dizem que as imagens **não** estão no ZIP,
  mas as cinco estão presentes.
- Três arquivos `.jpg` são na verdade **WebP** (assinatura `RIFF....WEBPVP8`):
  `01_Blades_6717.jpg`, `03_Kinect_13604.jpg`, `05_Metro_V2_17559.jpg`. Apenas
  `04_Metro_V1_15574.jpg` é JPEG de fato. Visualizadores que confiam na extensão falham.

## 2. Cabeçalhos XEX (inspeção somente-leitura)

Ferramenta: [`tools/xex_inspect.py`](../tools/xex_inspect.py) — lê apenas o cabeçalho
XEX2 público (big-endian). Não executa, não descriptografa e não descomprime nada.

| Dashboard | Execution ID (versão) | Data PE | Título | Criptografia | Compressão | Kit (bibl. estáticas) |
|---|---|---|---|---|---|---|
| Blades | **2.0.6717.0** ✔ | 03/06/2008 | 0xFFFE07D1 | AES | LZX | XDK 2.0.6132 + libs 6717 |
| NXE | **2.0.9199.0** ✔ | 18/02/2010 | 0xFFFE07D1 | AES | LZX | XDK 2.0.7645 + libs 9199 |
| Kinect | **2.0.13604.0** ✔ | 30/09/2011 | 0xFFFE07D1 | AES | LZX | XDK 2.0.20158 + libs 13604 |
| Metro V1 | **2.0.15574.0** ✔ | 17/06/2012 | 0xFFFE07D1 | AES | LZX | 2.0.15574 |
| Metro V2 | **2.0.17559.0** ✔ | 19/10/2019 | 0xFFFE07D1 | **nenhuma** | LZX | 2.0.17559 |

- Todos têm nome PE original `dash.exe`, flag de módulo `TITLE`, base 0x92000000 e
  Title ID 0xFFFE07D1 (dashboard). **As cinco versões declaradas foram confirmadas.**
- Observação: `dash_17559.xex` não é criptografado (somente comprimido). Isso difere dos
  outros quatro; pode indicar um arquivo já processado por ferramenta de terceiros.
  Não altera a análise, mas vale saber a origem do arquivo.

## 3. Recursos de áudio nos XEX

Pedido: verificar se os XEX contêm áudio identificável e acessível e documentar quais
sons pertencem a cada versão.

**Método (sem contornar proteções):**
1. Leitura da tabela `RESOURCE_INFO` (fica no cabeçalho, que não é criptografado).
2. Varredura do corpo bruto por assinaturas de áudio em texto claro
   (`RIFF`, `WAVE`, `XMA2`, bancos XACT `WBND`/`SDBK`, `OggS`, nomes `.wav/.xma/.xwb/.xsb`…).
3. Entropia do corpo (≈ 8 bits/byte indica dado criptografado e/ou comprimido).

**Resultados:**

| Versão | Recursos no cabeçalho | Áudio em texto claro | Entropia | Acessível? |
|---|---:|---|---:|---|
| Blades 6717 | 29 pacotes | nenhum¹ | 8,000 | **Não** — corpo criptografado (AES) |
| NXE 9199 | 29 pacotes | nenhum | 8,000 | **Não** — corpo criptografado (AES) |
| Kinect 13604 | 28 pacotes | nenhum | 8,000 | **Não** — corpo criptografado (AES) |
| Metro V1 15574 | 35 pacotes | nenhum¹ | 8,000 | **Não** — corpo criptografado (AES) |
| Metro V2 17559 | 36 pacotes | nenhum¹ | 7,974 | **Não diretamente** — sem criptografia, mas comprimido (LZX) |

¹ Houve 1 ocorrência isolada de uma sequência de 3–4 bytes (`ID3`/`XWB`), o que é ruído
estatístico esperado em dados aleatórios desse tamanho — não é um arquivo de áudio.

- Os nomes dos pacotes de recursos (ex.: `dashuisk`, `dashcomm`, `SharedUI`, `hubui`,
  `consoles`, `music`) são pacotes de interface XUI; **nenhum tem nome que identifique
  áudio**, e o conteúdo não pode ser listado sem descriptografar/descomprimir.
  Portanto **não é possível afirmar quais sons pertencem a cada versão** a partir do pacote.
  Também não há garantia de que os sons de navegação estejam no `dash.xex`
  (no console eles podem residir em outros módulos do sistema).
- **Por que não foi extraído nada:**
  - 6717, 9199, 13604 e 15574: extrair exigiria descriptografar com chaves do console,
    o que é contornar uma proteção — **não feito**.
  - 17559: tecnicamente acessível descomprimindo LZX (compressão não é proteção), mas o
    conteúdo é material da Microsoft; mesmo que extraído, **não pode ir para o repositório
    público**, e a permissão de uso é decisão sua. A descompressão não foi feita nesta fase.
- **O que foi implementado:** carregamento de sons originais a partir de uma pasta local
  **fora do repositório**, uma subpasta por versão, troca automática ao mudar de dashboard,
  e tela de estado que mostra, por efeito, o que carregou e o motivo exato das falhas.
  Detalhes em [`SONS.md`](SONS.md).

## 4. Imagens de referência

| Imagem | Formato real | Tamanho | Corresponde a | Observação |
|---|---|---|---|---|
| 01_Blades_6717 | WebP | 1280×720 | Família **Blades** | Tela "Xbox Live Marketplace" (lâmina laranja); não autentica a build 6717 |
| 02_NXE_9199 | WebP | 1280×720 | **NXE** | Canal "My Xbox", cartões em profundidade e avatar |
| 03_Kinect_13604 | WebP | 1280×720 | **Kinect Hub** (era 2010–2011) | Grade 4×2 de cartões, ondas roxas/verdes |
| 04_Metro_V1_15574 | JPEG | 1200×678 | **Metro (2011–2012)** | Interface em francês, fundo claro |
| 05_Metro_V2_17559 | WebP | 900×506 | **Metro final** | Fundo cinza escuro; marca d'água de site no canto (não faz parte da UI) |

As imagens contêm artes de jogos e marcas de terceiros; foram usadas **somente** como
referência de layout e organização. Nada delas é copiado para o Neutro X, e elas não são
publicadas no repositório.

## 5. Ferramentas de compilação

Detalhes e fontes em [`TOOLCHAIN.md`](TOOLCHAIN.md). Resumo:

| Opção | Gera XEX? | Situação |
|---|---|---|
| SDK oficial (XDK) | Sim | **Descartado**: proprietário, sem licença |
| libxenon (Free60) | Não — gera ELF para XeLL | Não roda pelo Aurora |
| OpenXeChain / SynthXEX (LLVM) | Sim | Inicial; README diz que faltam recursos (exports) |
| **xsdk (xenosworks)** | **Sim** (ELF → XEX2) | MIT, GCC 15, compila em Linux/WSL, diz rodar no kernel/dashboard retail de RGH |

**Decisão:** núcleo em C++17 portátil + renderizador em software, testado no Windows; porta
para o console via **xsdk em WSL** (Ubuntu já instalado nesta máquina). O ambiente Windows
desta máquina (MSVC 2022/2026, MinGW GCC 16, CMake 4.4) **não** gera XEX.

## 6. Limitações registradas

Ver [`LIMITACOES.md`](LIMITACOES.md).
