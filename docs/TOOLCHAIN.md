# Ferramentas de compilação e porta para o Xbox 360

## O que existe nesta máquina (verificado em 11/11/2026)

| Ferramenta | Versão | Uso no projeto |
|---|---|---|
| Visual Studio 2022 Community (MSVC) | 17.14 | Compila a prévia Windows e os testes |
| Visual Studio 2026 Community | 18.8 | Alternativa |
| MSYS2 UCRT64 GCC | 16.1 | Alternativa (compila o núcleo) |
| CMake | 4.4 | Sistema de build |
| Python | 3.12 | Ferramentas de análise (`tools/xex_inspect.py`) |
| WSL (Ubuntu) | WSL 2 | Cadeia do console (abaixo) |
| `powerpc-xsdk-elf-gcc` (xsdk) | GCC 15.3.0 + binutils + newlib | Compila o `NeutroX.xex` (construído em 11/11/2026, em `~/xsdk-gcc`) |
| glslang-xenon | 11:16.3.0 | Compila os shaders GLSL para a Xenos |
| xsdk | commit `3960a1d` (06/11/2026) | Biblioteca do console (`libxsdk.a`, `libxsc.so`); exemplos compilados |

Os compiladores do Windows produzem `.exe` x86-64; o Xbox 360 usa PowerPC (Xenon) big-endian e
o formato XEX2, gerado aqui pela cadeia do xsdk no WSL.

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
- **Feito (Fase 10):** a cadeia foi baixada (fontes GNU oficiais com SHA-256 conferido pelo
  script do xsdk) e compilada no WSL com autorização do usuário.

## Fase 10: compilar o `NeutroX.xex`

Uma vez (no Ubuntu/WSL; pacotes: `build-essential bison flex texinfo libgmp-dev libmpfr-dev
libmpc-dev cmake ninja-build python3 git curl`):

```bash
git clone https://github.com/xenosworks/xsdk.git ~/neutrox-xbox/xsdk
```

```bash
cd ~/neutrox-xbox/xsdk && PREFIX=$HOME/xsdk-gcc bash tools/toolchain/build.sh
```

Copie para `~/xsdk-gcc/bin/` o `glslang` do glslang-xenon (ramo `xenon`, compilado conforme
`docs/GETTING_STARTED.md` do xsdk, seção 4). Depois compile o xsdk:

```bash
cd ~/neutrox-xbox/xsdk && make -j$(nproc) && make samples
```

E o Neutro X (a pasta de build fica fora do repositório):

```bash
cmake -S /mnt/g/Documentos/NeutroX -B ~/neutrox-xbox/build_xbox -G Ninja -DCMAKE_TOOLCHAIN_FILE=$HOME/neutrox-xbox/xsdk/cmake/xsdk.cmake -DXSDK_TOOLCHAIN=$HOME/xsdk-gcc -DCMAKE_BUILD_TYPE=Release
```

```bash
cmake --build ~/neutrox-xbox/build_xbox
```

Resultado: `~/neutrox-xbox/build_xbox/NeutroX.xex` (≈2,4 MB; título `0x4E455558`, "Neutro X",
versão igual à do projeto). Instalação e teste: [INSTALACAO_XBOX.md](INSTALACAO_XBOX.md).

### Como a plataforma do console funciona (`src/platform/xbox360/`)

- **Vídeo:** o `SoftRenderer` desenha 1280×720 na CPU. A cada quadro, o buffer é copiado para uma
  textura linear `A8R8G8B8`, e um quad de tela cheia (`blit.vert`/`blit.frag`, amostragem por
  ponto, meio pixel de deslocamento como no D3D9) a desenha. Em seguida vêm `xsdk_gpu_resolve`
  para o buffer de apresentação (que é *tiled*: a CPU não pode escrever nele direto) e uma cerca
  de GPU antes do próximo envio.
- **Entrada:** `XamInputGetState` dos 4 controles → `RawPadState` (zona morta 7849 no analógico).
- **Áudio:** uma thread alimenta `xsdk_audio_*` em quadros de 256 amostras, 48 kHz, 6 canais em
  float planar. O mixer de 44,1 kHz é reamostrado e vai para frente esquerda/direita.
- **Arquivos:** `game:/` (a pasta do `.xex`). Log: `NeutroX.log` ao lado do `.xex` (o `stderr`
  do núcleo vai para o log do xsdk).
- **Saída:** `main` retorna e o console volta ao dashboard padrão dele.

## Endianness e desempenho (pontos de atenção para a porta)

- O framebuffer usa `uint32_t` 0xAARRGGBB. O formato `XSDK_FORMAT_A8R8G8B8` recebe palavras
  nativas nessa mesma ordem (como o exemplo `cube` do xsdk), então não há conversão por pixel.
  **A ser confirmado no console** (verificação 2 de [INSTALACAO_XBOX.md](INSTALACAO_XBOX.md)).
- `writeWav`/`writePng` já gravam bytes explicitamente (portáveis).
- O renderizador em software mede ~1 ms/quadro nos dashboards e ~4,6 ms no menu (720p,
  CPU de PC). No Xenon (PowerPC in-order) espera-se várias vezes mais lento; por isso os
  fundos estáticos já são cacheados em textura e o caminho de GPU está previsto.
