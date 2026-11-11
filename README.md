# Neutro X — Xbox 360 Dashboard Experience

Aplicativo independente que apresenta **recriações visuais e interativas** de cinco gerações
de dashboards do Xbox 360, navegáveis pelo controle e trocáveis sem reiniciar:

| Interface | Referência histórica | Ano |
|---|---|---|
| Blades | 2.0.6717.0 | 2008 |
| NXE | 2.0.9199.0 | 2010 |
| Kinect | 2.0.13604.0 | 2011 |
| Metro V1 | 2.0.15574.0 | 2012 |
| Metro V2 | 2.0.17559.0 | 2019 |

> **Status: beta 0.3.0 (Fases 1–5 concluídas).** Roda hoje como **prévia no Windows**.
> O executável para o console (`.xex`) ainda **não** existe — ver [Fase 10](docs/FASES.md)
> e [docs/TOOLCHAIN.md](docs/TOOLCHAIN.md). **Blades** e **NXE** já estão completos; Kinect, Metro V1 e
> Metro V2 seguem com telas provisórias navegáveis até as Fases 6 a 8.

![Menu principal](docs/img/menu_principal.png)

![Blades](docs/img/blades.png)

![NXE](docs/img/nxe.png)

## Princípios

- **Tudo original:** gráficos, ícones, fonte e sons são criados por código para o Neutro X.
  Nenhum recurso, código ou arquivo proprietário dos dashboards é usado ou redistribuído.
- **Seguro e reversível:** não grava na NAND, não altera arquivos do sistema, não mexe na
  inicialização nem no RGH. Desinstalar = apagar a pasta do aplicativo.
- **Sons originais só por conta do usuário:** se você tiver os áudios originais e permissão
  para usá-los, coloque-os numa pasta local; eles nunca entram no repositório.
  Ver [docs/SONS.md](docs/SONS.md).

## Executar a prévia no Windows

Baixe o ZIP da [página de releases](../../releases) e execute `NeutroX.exe`
(Windows 10/11 x64; não precisa instalar nada).

| Controle Xbox 360 | Teclado | Ação |
|---|---|---|
| Direcional / analógico esq. | Setas ou W A S D | Mover seleção |
| A | Enter / Espaço | Confirmar |
| B | Esc / Backspace | Voltar |
| X | X | Ação contextual (no menu: definir dashboard padrão) |
| Y | Y | Opções |
| LB / RB | Q / E | Trocar categoria |
| START | Tab | Menu do Neutro X (dentro de um dashboard) |
| BACK | F1 | Mostrar/ocultar FPS |
| — | F12 | Captura de tela (PNG na pasta de dados) |

Pasta de dados (configurações, log, capturas, sons originais): `%LOCALAPPDATA%\NeutroX\`.

Opções de linha de comando: `--res 1920x1080`, `--data PASTA`, `--mute`,
`--frames N --screenshot arquivo.png` (teste automatizado).

## Compilar

Requisitos: CMake ≥ 3.20 e um compilador C++17 (testado com Visual Studio 2022).

```bash
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
```

```bash
cmake --build build --config Release
```

```bash
build/Release/neutrox_tests.exe build/test_output
```

Os testes rodam sem janela, gravam capturas PNG de cada tela em `build/test_output/` e
verificam inicialização, controle, as cinco interfaces, troca repetida (100×), memória,
desempenho, configurações, recuperação de erros e encerramento.

## Estrutura

```text
src/core/        loop principal, pilha de telas, transições, log, PNG
src/graphics/    IRenderer, renderizador em software, fonte vetorial própria, ícones, UI
src/input/       abstração do controle (repetição, analógico, desconexão)
src/audio/       mixer, temas sonoros por versão, carregador de sons originais (WAV)
src/settings/    configurações (arquivo próprio neutrox.ini, gravação atômica)
src/dashboards/  registro e telas das cinco interfaces
src/shell/       menu inicial, configurações, sobre, menu START, diálogos, erro
src/platform/    win32 (prévia), headless (testes), xbox360 (porta futura)
assets/          sons recriados exportados em WAV, por versão
tools/           xex_inspect.py (análise de cabeçalho), exportador de sons
docs/            relatórios, arquitetura, toolchain, limitações, fases
tests/           testes automatizados
```

## Documentação

- [Relatório da Fase 1](docs/RELATORIO_FASE1.md) — análise do pacote, XEX, imagens, áudio
- [Arquitetura](docs/ARQUITETURA.md)
- [Ferramentas de compilação e porta para o console](docs/TOOLCHAIN.md)
- [Sons](docs/SONS.md)
- [Relatório de testes](docs/RELATORIO_TESTES.md)
- [Limitações](docs/LIMITACOES.md)
- [Fases](docs/FASES.md)

## Aviso legal

Xbox, Xbox 360, Xbox LIVE e Kinect são marcas da Microsoft Corporation. O Neutro X é um
projeto de fã, sem afiliação ou endosso da Microsoft. Código sob licença [MIT](LICENSE).
