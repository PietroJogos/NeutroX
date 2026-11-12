# Limitações conhecidas (versão 0.9.0-beta)

## Plataforma
- O `NeutroX.xex` existe e é compilado pela cadeia do xsdk ([TOOLCHAIN.md](TOOLCHAIN.md)), mas
  **nada foi testado em hardware.** Não há declaração de compatibilidade com o console, o
  Aurora ou o RGH até um teste real ([INSTALACAO_XBOX.md](INSTALACAO_XBOX.md)).
- O Xenia não serve para validar: ele não apresenta a imagem dos títulos do xsdk.
- No console, a imagem é sempre desenhada pela CPU (1280×720; se a saída for outra, o xsdk
  informa o tamanho e o renderizador se ajusta). O custo por quadro no Xenon é desconhecido
  até o teste (no PC, 1–10 ms).
- O renderizador é em software, agora paralelo (6 threads no console) e otimizado
  ([DESEMPENHO.md](DESEMPENHO.md)). Os 60 FPS no Xenon não estão confirmados: só o teste no
  console mede isso.
- **Saída:** a troca de aplicativo para o Aurora ou o dashboard oficial ainda não foi
  testada. Se o DashLaunch redirecionar o `dash.xex`, "Voltar ao Xbox Dashboard" pode abrir o
  Aurora ([SAIDA_E_INICIALIZACAO.md](SAIDA_E_INICIALIZACAO.md)).

## Interfaces
- **As cinco interfaces estão completas (Fases 4–8).**
- As opções de cada interface são salvas; **favoritos e blocos fixados** valem só durante a sessão.
- Quadros com animação são desenhados ao vivo pelo renderizador em software (paralelo).
- Kinect: sem reconhecimento corporal; a interface funciona só com o controle.
- **Perfil e avatar:** o nome (gamertag) do perfil conectado é lido pelo XAM
  (`XamUserGetSigninState`/`XamUserGetName`, somente leitura) e atualizado a cada 2 s quando o
  perfil muda. **O avatar 3D original e a imagem do jogador não são exibidos:** renderizar o
  avatar exige as bibliotecas de avatar do XDK, que são proprietárias, e a imagem do jogador
  exige as funções de *tiles* do XAM, que o xsdk não expõe. As interfaces continuam mostrando
  a figura genérica própria do Neutro X, identificada como tal. Gamerscore, lema e zona não
  são lidos ("não lido").
- **Biblioteca:** jogos extraídos (`default.xex`), GOD e XBLA são detectados pelos cabeçalhos
  reais ([BIBLIOTECA.md](BIBLIOTECA.md)). **Iniciar GOD/XBLA é experimental e vem desligado.**
  Nenhum início de jogo foi testado no console. O nome dos jogos extraídos vem da pasta (o
  cabeçalho XEX não traz o nome em texto claro).
- **Capas:** dependem do XboxUnity (HTTP) para o download automático. Só funcionam com
  internet no console; offline valem as capas salvas, as importadas e as miniaturas dos
  pacotes. Só JPEG baseline e PNG de 8 bits; JPEG progressivo é recusado.
- Conquistas, amigos, mensagens e mídia continuam ilustrativos e identificados como tal
  (nenhum dado de conta é lido).
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
  sonoros — por decisão de projeto, não há substituição automática. A opção explícita
  "Originais + recriados nos ausentes" existe para quem preferir som em todas as interfaces.

## Outros
- Idioma: apenas português do Brasil.
- Tamanho de janela livre no Windows; a área útil mantém 16:9 com barras.
- A fonte própria cobre ASCII, acentos do português e alguns símbolos; outros caracteres
  aparecem como um retângulo.
