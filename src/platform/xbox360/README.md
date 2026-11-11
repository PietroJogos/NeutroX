# Porta para o Xbox 360 (Fase 10)

Esta pasta receberá a implementação de `IPlatform` para o console, sobre o SDK homebrew
**xsdk** (MIT), compilado em Linux/WSL. Ainda não há código aqui de propósito: escrever
contra uma API que não foi instalada e verificada seria inventar.

Tarefas previstas (ver `docs/TOOLCHAIN.md`):

1. Instalar a cadeia do xsdk no WSL (GCC 15 `powerpc-xsdk-elf`) — com autorização.
2. Compilar `neutrox_core` com `-DCMAKE_TOOLCHAIN_FILE=cmake/xsdk.cmake`.
3. `platform_xbox360.cpp`:
   - vídeo: enviar o buffer do `SoftRenderer` como textura (depois: renderizador Xenos);
   - entrada: controles → `RawPadState`;
   - áudio: `Mixer::mix()` → saída de áudio do xsdk;
   - dados: pasta do próprio app; nunca partições de sistema;
   - sair: voltar ao Aurora.
4. Gerar o XEX, testar no Xenia e depois no console pelo Aurora.

Regras: não gravar na NAND, não substituir arquivos do sistema, não alterar a
inicialização nem o RGH. Desinstalar = apagar a pasta do Neutro X.
