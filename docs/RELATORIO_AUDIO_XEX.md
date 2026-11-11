# Relatório técnico — áudio nos cinco XEX dos dashboards

Gerado com `tools/sound_extract.py` em 11/11/2026, a partir dos arquivos fornecidos pelo usuário.
Este documento traz **apenas metadados** (nomes, tamanhos, formatos, contagens). Nenhum áudio, imagem
ou código proprietário está no repositório; os WAV extraídos ficam só em
`%LOCALAPPDATA%\NeutroX\local_audio\` na máquina do usuário. Cada pasta de versão também tem o próprio
`relatorio.txt`, com todos os detalhes.

## Método

1. **Cabeçalho XEX2.** É texto claro e foi lido sem descriptografar nada (`tools/xex_inspect.py`):
   Execution ID, criptografia, compressão, pacotes de recursos, bibliotecas estáticas e importadas.
2. **XEX com corpo criptografado (AES).** A análise para no cabeçalho. Abrir o conteúdo exigiria as
   chaves do console, e isso seria contornar uma proteção, então não foi feito.
3. **XEX sem criptografia.** A imagem é descomprimida pelo nosso decodificador LZX
   (`neutrox_xex_unpack`). Em seguida, a ferramenta lê os pacotes de recursos `XUIZ` e, dentro deles:
   - os bancos **XACT3** (`.xwb` = ondas, `.xsb` = cues com o nome de cada uso do som);
   - os áudios RIFF soltos;
   - as cenas e skins XUI (`.xur`) que referenciam sons.
4. **Função do som.** Só é atribuída por evidência que dá para verificar: o nome original da cue ou
   do arquivo e a cena/skin que o usa. Um som sem essa evidência fica como **não confirmado**
   (é guardado, mas não é usado). Sons de outra versão nunca são misturados.
5. **Conversão e validação.** O FFmpeg converte cada onda para WAV PCM 16 bits (XMA/XMA2 → PCM). Depois
   a ferramenta confere:
   - canais (1 ou 2);
   - taxa;
   - duração (diferença de no máximo 1024 amostras em relação à declarada no banco);
   - se o arquivo não está vazio nem silencioso.

## Resumo

| Versão | Execution ID | Data do PE | Corpo | Bibliotecas de áudio (estáticas) | Pacotes | Áudios | Efeitos confirmados |
|---|---|---|---|---|---:|---:|---:|
| Blades | 2.0.6717.0 ✔ | 2008-06-03 | AES + LZX | XAUD, XHV | 29 | 0 | 0 de 8 |
| NXE | 2.0.9199.0 ✔ | 2010-02-18 | AES + LZX | XAUD, XHV | 29 | 0 | 0 de 8 |
| Kinect | 2.0.13604.0 ✔ | 2011-09-30 | AES + LZX | XAUD, XHV | 28 | 0 | 0 de 8 |
| Metro V1 | 2.0.15574.0 ✔ | 2012-06-17 | AES + LZX | XAUD, XAUDIO2, XMCORE, XACT | 35 | 0 | 0 de 8 |
| **Metro V2** | 2.0.17559.0 ✔ | 2019-10-19 | **sem criptografia** + LZX | XAUD, XAUDIO2, XMCORE, XACT | 36 | **52** | **7 de 8** |

✔ = a versão no cabeçalho confere com a pasta. Em todas, o nome PE original é `dash.exe` e o
title id é `0xFFFE07D1`.

## Blades, NXE, Kinect e Metro V1: criptografados

- O conteúdo **não foi analisado**. Todos têm o pacote `dashcomm`, que é onde o Metro V2 guarda o
  banco de sons:

  | Versão | Tamanho do `dashcomm` |
  |---|---:|
  | Blades | 375 492 bytes |
  | NXE | 623 631 bytes |
  | Kinect | 996 091 bytes |
  | Metro V1 | 927 624 bytes |

  Ele é o candidato provável, mas isso **não pôde ser verificado**.
- Blades, NXE e Kinect não ligam a biblioteca XACT, só XAudio (`XAUD`). Os sons dessas versões
  provavelmente são arquivos XMA soltos tocados pelo XAudio, e não um banco `.xwb`/`.xsb`.
  O Metro V1 já liga a XACT. Isso também é inferência a partir do cabeçalho.
- **O que falta para extrair:** uma cópia **não criptografada** do mesmo XEX, obtida de forma
  legítima pelo próprio usuário. A ferramenta processa qualquer XEX cujo corpo esteja em texto
  claro. Ela também aceita `.xwb`/`.xsb` soltos colocados ao lado do XEX.

## Metro V2 (2.0.17559.0): aberto

**Correção do relatório 0.7.1.** O banco `dash.xwb`/`dash.xsb` **não é externo**: ele está
dentro do XEX, no pacote `dashcomm`. As cenas o referenciam como `common://dash.xsb` e
`common://dash.xwb`.

| Recurso | Formato | Conteúdo |
|---|---|---|
| `dashcomm/dash.xwb` | XACT3 v43, big-endian, banco `UI_sounds` | 36 ondas XMA2 (mono/estéreo, ~44,1 kHz) |
| `dashcomm/dash.xsb` | XACT3 v43, banco de sons `interaction` | 51 cues simples → ondas (sem tabelas de variação, exceto `vk_select`) |
| `controlp/*.xma`, `controlp/../handles/*.xma` | RIFF XMA (1 fluxo, 44,1 kHz) | 16 áudios do "control pack" (painéis e gestos do Kinect) |

Strings de áudio no código, ou seja, as classes usadas para tocar sons:

- `XuiSoundXACT`, `XuiSoundXAudio`, `SoundXACT`, `PeekSoundXACT`;
- `Xbox.XuiSoundImpl` (Lua, em `luaxbox/xuisound.lub`).

### Efeitos do Neutro X

| Efeito | Arquivo | Cue / evidência |
|---|---|---|
| navegar | `btn_Focus.wav` (onda 6) | cue `btn_Focus`. A skin `dashuisk/skin.xur` usa `sharedres://btn_Focus.xma` no estado *Focus* dos botões |
| confirmar | `snd_buttonselect.wav` (onda 24) | cue `snd_buttonselect` (`selectSoundXACT` na skin). Mesma onda de `btn_Select` e `select_button` |
| voltar | `snd_buttonback.wav` (onda 23) | cue `snd_buttonback` (skin). Mesma onda de `btn_Back` |
| categoria | `tab_switch.wav` (onda 5) | cue `tab_switch`. A cena `network/2004_NetworkDetails.xur` nomeia `sharedres://tab_switch.xma` |
| abrir | `snd_panelunfold.wav` (onda 0) | cue `snd_panelunfold`, de `PanelFoldSounds` em `controlp/Variables.xur` |
| erro | `btn_InactiveSelect.wav` (onda 14) | cue `btn_InactiveSelect`: seleção de item inativo (skin) |
| transicao | `snd_transitionfrom.wav` (onda 1) | cue `snd_transitionfrom`, de `SceneTransitions` em `controlp/Variables.xur` |
| **entrada** | **ausente** | Nenhuma das 51 cues nem dos 16 áudios soltos tem nome de abertura/entrada |

Mudança em relação à 0.7.1: **navegar** era `snd_panelright` (movimento entre painéis do
control pack) e passou a ser `btn_Focus`. **Categoria** era `snd_channelup` e passou a ser
`tab_switch`. Os dois agora vêm do banco do próprio dashboard.

### Os 52 áudios instalados

- **7 confirmados**, um para cada efeito da tabela acima.
- **4 alternativas**, com a mesma função de um confirmado: as versões do control pack de
  `snd_buttonselect`, `snd_buttonback`, `snd_panelunfold` e `snd_transitionfrom`.
- **24 com função conhecida, mas sem evento no Neutro X**, entre eles:
  - `snd_panelfold` (fechar painel);
  - `dl_complete` (notificação de download concluído);
  - teclado virtual, perfis, área social, canais, páginas e gestos do Kinect.
- **17 não confirmados**:
  - nomes sem função verificável: `swipe_*`, `peek_*`, `roll_on`, `slot_roll_on`, `VNI_*`/`VUI_*`
    (voz), `bing_results`, `live_tiles`;
  - 2 ondas sem nenhuma cue (25 e 26);
  - `btn_InactiveFocus`: o banco declara taxa de 46 651 Hz, que foge mais de 1% das taxas do XMA,
    então a decodificação é incerta.

**Erros de conversão: 0.** Em todas as ondas, a duração bate com a declarada no banco
(diferença ≤ 768 amostras, que é o atraso do codificador XMA). O campo de taxa do banco traz
valores próximos de 44 100 (por exemplo 44 136 e 44 041). Esses valores foram decodificados à taxa
XMA padrão mais próxima, e o valor original fica registrado no relatório.

**Recursos externos referenciados** (não estão neste XEX; são recursos compartilhados do sistema,
o XAM):

- `sharedres://btn_Focus.xma`, `btn_Select.xma`, `btn_Back.xma`, `btn_InactiveSelect.xma`, usados
  por `dashuisk/skin.xur` e `SharedUI/MPDashSkin.xur`;
- `sharedres://tab_switch.xma`.

O próprio banco do dashboard tem cues com esses mesmos nomes, e são elas que o Neutro X usa.

## Integração no Neutro X

- **Pasta:** `<dados>/local_audio/<versão>/`, com `mapeamento.ini` (só funções confirmadas),
  `ausentes.ini` (causa de cada ausência), `manifesto.ini` (contagens + uma linha por arquivo com
  status, origem, codec, canais, taxa, duração e SHA-256) e `relatorio.txt`. Na raiz ficam
  `manifesto.json` e `relatorio_extracao.txt`. A pasta antiga `sons_originais/` continua sendo
  aceita.
- **Memória:** só o conjunto do dashboard ativo fica carregado. Ao trocar de dashboard, o conjunto
  anterior é descarregado (as vozes dele param) e o novo é carregado. Nos menus do Neutro X, o
  conjunto atual é mantido.
- **Tela de estado** (Configurações → Sons originais: estado), por versão:
  - arquivos encontrados, não confirmados e erros de conversão;
  - efeitos esperados × carregados × ausentes;
  - recursos externos;
  - estado do XEX;
  - para cada efeito, o arquivo real ou a causa exata da ausência.

  `8/8` só aparece quando os oito arquivos foram realmente decodificados.
