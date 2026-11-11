# Ferramentas de compilação e porta para o Xbox 360

## O que existe nesta máquina (verificado em 11/11/2026)

| Ferramenta | Versão | Uso no projeto |
|---|---|---|
| Visual Studio 2022 Community (MSVC) | 17.14 | Compila a prévia Windows e os testes |
| Visual Studio 2026 Community | 18.8 | Alternativa |
| MSYS2 UCRT64 GCC | 16.1 | Alternativa (compila o núcleo) |
| CMake | 4.4 | Sistema de build |
| Python | 3.12 | Ferramentas de análise (`tools/xex_inspect.py`) |
| WSL (Ubuntu) | WSL 2 | Necessário para a cadeia do console (abaixo) |

Nenhuma dessas ferramentas gera `.xex`. Um compilador C++ para Windows produz `.exe` x86-64;
o Xbox 360 usa PowerPC (Xenon) big-endian e o formato XEX2.

## Opções para gerar XEX (pesquisa)

1. **SDK oficial da Microsoft (XDK)** — proprietário; só com licença. **Não usado.**
2. **libxenon / Xenon Toolchain (Free60)** — homebrew aberto, mas gera **ELF** para o XeLL
   (carregador que substitui o sistema). Não roda a partir do Aurora e exige reinicializar no
   XeLL. Não atende ao requisito de rodar pelo Aurora.
3. **OpenXeChain** (LLVM/Clang `ppc32-xbox360` + SynthXEX, AGPL-3.0) — gera XEX, mas o próprio
   README do SynthXEX avisa que está em desenvolvimento inicial e que faltam recursos.
4. **xsdk (xenosworks)** — SDK homebrew MIT, GCC 15 + newlib (`powerpc-xsdk-elf`), empacota
   ELF em XEX2 (`tools/elf2xex.py`), declara rodar "no kernel e dashboard retail de um console
   RGH", com camada de GPU (Xenos), áudio, controles, arquivos (HDD/USB) e lançamento de outro
   `.xex`. Compila em Linux ou WSL. Declara não usar material do SDK vazado.
   *As afirmações são do próprio projeto; não foram verificadas em hardware aqui.*

Fontes: [xsdk](https://github.com/xenosworks/xsdk) ·
[OpenXeChain](https://github.com/openxechain) ·
[SynthXEX](https://github.com/OpenXeChain/SynthXEX) ·
[LibXenon (Free60)](https://free60.org/Development/LibXenon)

## Decisão

- **Agora:** todo o aplicativo vive em `neutrox_core` (C++17, sem APIs de sistema). A prévia
  Windows e a plataforma *headless* de testes apenas fornecem janela/entrada/áudio.
- **Fase 10:** criar `src/platform/xbox360/` sobre o **xsdk**, compilado no WSL:
  - vídeo: o `SoftRenderer` desenha num buffer 1280×720 que é enviado como textura à GPU
    pelo xsdk (primeiro passo); depois, um `IRenderer` acelerado pela Xenos;
  - entrada: controles via xsdk → `RawPadState`;
  - áudio: `Mixer::mix()` alimenta o canal de áudio do xsdk (48 kHz → reamostrar ou trocar
    `kAudioRate`);
  - arquivos: pasta de dados = pasta do próprio app (`game:\`), nunca partições do sistema;
  - saída: voltar ao inicializador (Aurora) pela API de lançamento do xsdk.
- **Bloqueio atual para gerar o XEX:** a cadeia do xsdk precisa ser baixada e compilada no
  WSL (GCC 15 + binutils + newlib — dezenas de minutos). Isso não foi feito sem sua
  autorização, porque envolve baixar e compilar código de terceiros.

## Endianness e desempenho (pontos de atenção para a porta)

- O framebuffer usa `uint32_t` 0xAARRGGBB; no PowerPC (big-endian) a ordem dos bytes em
  memória muda — a conversão fica na camada de plataforma.
- `writeWav`/`writePng` já gravam bytes explicitamente (portáveis).
- O renderizador em software mede ~1 ms/quadro nos dashboards e ~4,6 ms no menu (720p,
  CPU de PC). No Xenon (PowerPC in-order) espera-se várias vezes mais lento; por isso os
  fundos estáticos já são cacheados em textura e o caminho de GPU está previsto.
