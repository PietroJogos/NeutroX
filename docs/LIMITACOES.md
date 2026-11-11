# Limitações conhecidas (versão 0.3.0-beta)

## Plataforma
- **Não existe executável para o Xbox 360 ainda.** Nenhuma ferramenta desta máquina gera
  XEX. O caminho escolhido (xsdk em WSL) exige baixar e compilar a cadeia GCC do projeto,
  o que depende da sua autorização. Ver [TOOLCHAIN.md](TOOLCHAIN.md).
- **Nada foi testado em hardware.** Não há declaração de compatibilidade com o console,
  o Aurora ou o RGH até um teste real.
- O renderizador é em software. No PC sobra folga (≈1–5 ms por quadro em 720p); no
  processador do Xbox 360 o custo será maior. O caminho por GPU está planejado.

## Interfaces
- **Blades (Fase 4) e NXE (Fase 5) estão completos.** Kinect, Metro V1 e Metro V2 seguem em
  **versão provisória** (fundo representativo + seções LB/RB + menu START) até as Fases 6 a 8.
- Blades e NXE: as opções da interface (fundo, relógio, avatar, camiseta) valem só durante a sessão.
- Quadros com animação são desenhados ao vivo pelo renderizador em software; no PC ficam
  abaixo de 16,7 ms em 720p, mas no Xbox 360 exigirão o caminho por GPU (Fase 10).
- Kinect: sem reconhecimento corporal; a interface funciona só com o controle.
- Avatar: figura genérica própria; sem ligação com o serviço oficial de avatares.
- Biblioteca de jogos: somente demonstração (Fase 8); não lança jogos.

## Sons
- Os sons originais **não** acompanham o projeto. Os XEX fornecidos não têm áudio
  acessível sem descriptografia (4 de 5) ou descompressão (Metro V2); veja
  [SONS.md](SONS.md). O carregamento a partir de pasta local está pronto.
- XMA não é suportado; é preciso converter para WAV PCM.
- Com a fonte "Originais" (padrão) e a pasta vazia, os dashboards ficam sem efeitos
  sonoros — por decisão de projeto, não há substituição automática.

## Outros
- Idioma: apenas português do Brasil.
- Tamanho de janela livre no Windows; a área útil mantém 16:9 com barras.
- A fonte própria cobre ASCII, acentos do português e alguns símbolos; outros caracteres
  aparecem como um retângulo.
