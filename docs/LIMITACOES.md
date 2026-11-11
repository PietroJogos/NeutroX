# Limitações conhecidas (versão 0.7.0-beta)

## Plataforma
- **Não existe executável para o Xbox 360 ainda.** Nenhuma ferramenta desta máquina gera
  XEX. O caminho escolhido (xsdk em WSL) exige baixar e compilar a cadeia GCC do projeto,
  o que depende da sua autorização. Ver [TOOLCHAIN.md](TOOLCHAIN.md).
- **Nada foi testado em hardware.** Não há declaração de compatibilidade com o console,
  o Aurora ou o RGH até um teste real.
- O renderizador é em software. No PC sobra folga em 720p (≈1–10 ms por quadro); em 1080p
  a troca de canal do NXE fica em ≈22 ms (≈45 FPS durante a animação). No processador do
  Xbox 360 o custo será maior. O caminho por GPU está planejado (Fase 10).

## Interfaces
- **As cinco interfaces estão completas (Fases 4–8).**
- As opções de cada interface são salvas; **favoritos e blocos fixados** valem só durante a sessão.
- Quadros com animação são desenhados ao vivo pelo renderizador em software; no PC ficam
  abaixo de 16,7 ms em 720p, mas no Xbox 360 exigirão o caminho por GPU (Fase 10).
- Kinect: sem reconhecimento corporal; a interface funciona só com o controle.
- Avatar: figura genérica própria; sem ligação com o serviço oficial de avatares.
- Biblioteca de jogos: somente demonstração (8 jogos fictícios); **não detecta nem lança
  jogos reais**. Detectar jogos do console exigirá estudar mecanismos documentados do
  ambiente homebrew e validar a segurança do fluxo (após a Fase 10).
- Painéis "informações locais" mostram dados do Neutro X e do aparelho que o app já conhece;
  nenhuma configuração do console é lida.

## Sons
- Os sons originais **não** acompanham o projeto. A extração automática local
  (`tools/sound_extract.py`) obtém **52 áudios do Metro V2** (banco XACT `dash.xwb`/`dash.xsb`
  + pacote `controlp`), com **7 dos 8 efeitos** confirmados; falta "entrada", que não existe
  entre os sons do XEX. Os outros quatro XEX estão criptografados e não são abertos (seria
  contornar proteção). Veja [SONS.md](SONS.md) e [RELATORIO_AUDIO_XEX.md](RELATORIO_AUDIO_XEX.md).
- Leitura de XACT: só bancos XACT3 (v42–46) não compactos e cues simples; cues complexas e
  outros eventos são relatados, não usados. Ondas ADPCM/WMA em bancos não são convertidas.
- Sons com função conhecida mas sem evento no Neutro X (fechar painel, notificação de
  download…) ficam guardados e não tocam.
- XMA não é suportado; é preciso converter para WAV PCM.
- Com a fonte "Originais" (padrão) e a pasta vazia, os dashboards ficam sem efeitos
  sonoros — por decisão de projeto, não há substituição automática.

## Outros
- Idioma: apenas português do Brasil.
- Tamanho de janela livre no Windows; a área útil mantém 16:9 com barras.
- A fonte própria cobre ASCII, acentos do português e alguns símbolos; outros caracteres
  aparecem como um retângulo.
