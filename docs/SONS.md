# Sons do Neutro X

A 0.9.2 aceita múltiplos ZIPs, XEX e pastas em um único lote e pode montar a
instalação USB com `--pacote-local`. Consulte o
[comando e inventário da 0.9.2](RELATORIO_AUDIO_092.md#reproduzir-o-lote-e-testar-no-console).

## Três conjuntos, cada um no seu lugar

| Conjunto | Onde fica | Vai para o GitHub? |
|---|---|---|
| **Sons do Neutro X** (menu, configurações, transições) | Gerados por código (`src/audio/themes/neutrox.cpp`) | Sim (são originais do projeto) |
| **Sons originais dos dashboards** (seus arquivos) | `<pasta de dados>/local_audio/<versão>/` | **Nunca** (`.gitignore`) |
| **Sons recriados por versão** (alternativa opcional) | Gerados por código (`src/audio/themes/<versão>.cpp`); exportáveis em WAV | Sim (são originais do projeto) |

A fonte usada nos dashboards é escolhida em **Configurações → Sons dos dashboards**:

- **Originais (pasta local)** — padrão. Toca apenas os arquivos que você colocou na pasta.
  **Um efeito ausente fica mudo**; ele não é trocado automaticamente por uma recriação.
  Ao abrir um dashboard incompleto aparece um aviso com a contagem (ex.: "0 de 8").
- **Recriados pelo Neutro X** — só quando você escolhe explicitamente.
- **Originais + recriados nos ausentes** (0.9.0) — escolha explícita: toca o original quando
  existe e o recriado **somente** nos efeitos sem original. A tela de estado e o log dizem
  quantos de cada.

## Sem som no Xbox 360 (0.8.0): causa e diagnóstico

**Causa encontrada:** com a fonte padrão "Originais", o Neutro X procura os arquivos em
`local_audio/`, ao lado do `.xex`. Sem essa pasta no console (ela fica no PC, em
`%LOCALAPPDATA%\NeutroX\local_audio\`), **todos os efeitos dos dashboards ficam mudos por
projeto**, sem substituição automática. Mesmo com a pasta copiada, só o Metro V2 tem
originais (7 de 8): os XEX dos outros quatro são criptografados.

**Correções na 0.9.0:**

- `tools/xbox_install.py` envia o `NeutroX.xex` e a `local_audio/` (WAV + `.ini`) para a pasta
  do Neutro X no console, por FTP (`--host <IP>`; a senha é pedida na hora e não é guardada;
  `--dry-run` mostra o que seria enviado). Assim os sons chegam ao lugar certo sem copiar um
  por um.
- Ao abrir um dashboard sem originais, o aviso agora diz exatamente o que acontece ("nenhum
  som original em local_audio; efeitos mudos"). O log registra cada efeito que ficou mudo e
  o motivo (`Som mudo: kinect_13604/voltar - arquivo não encontrado`).
- Leitura de WAV float corrigida para o processador big-endian do console (antes lia os
  bytes na ordem do PC).
- O encerramento do áudio para a thread de alimentação antes de fechar o driver.

**Diagnóstico (Configurações → Sons originais: estado):**

- linha **Saída**: dispositivo aberto ou fechado (com o código de erro do `xsdk_audio_open`)
  e o formato (48 kHz, 6 canais; saída estéreo ou 5.1);
- contadores: pedidos, tocados, mudos (arquivo ausente ou conjunto não carregado) e pico do
  sinal misturado;
- **X — Testar saída**: toca o tom próprio do Neutro X, que não depende de arquivo, e diz se
  ele foi misturado e entregue ao sistema. Isso separa "áudio ausente" de "erro de
  reprodução";
- no log, ao sair: pedidos, tocados, mudos, pico, quadros entregues ao driver e quadros de
  silêncio por atraso.

**A reprodução real no Xbox 360 ainda não foi confirmada**: depende do seu teste no console.

O tema muda sozinho com a tela: dashboard ativo → sons daquela versão; menus do Neutro X
(inclusive o menu do START por cima de um dashboard) → sons do Neutro X.

## Pasta dos sons originais

Windows (prévia): `%LOCALAPPDATA%\NeutroX\local_audio\`
Console: `local_audio/` dentro da pasta do próprio aplicativo (ex.: `Hdd1:\Apps\NeutroX\local_audio\`);
`tools/xbox_install.py` copia para lá.
A pasta antiga `sons_originais/` continua aceita quando é a única existente.

Só o conjunto do dashboard ativo fica na memória: ao trocar de dashboard o anterior é
descarregado e o novo é carregado (nos menus do Neutro X o conjunto atual é mantido).

O Neutro X cria a estrutura e um `LEIA-ME.txt` na primeira execução:

```text
local_audio/
├── blades_6717/
├── nxe_9199/
├── kinect_13604/
├── metro_v1_15574/
└── metro_v2_17559/
```

Nomes esperados em cada pasta:

| Arquivo | Quando toca |
|---|---|
| `navegar.wav` | mover a seleção |
| `confirmar.wav` | botão A |
| `voltar.wav` | botão B |
| `categoria.wav` | LB/RB (trocar seção/lâmina/canal) |
| `abrir.wav` | abrir painel/menu |
| `erro.wav` | ação inválida |
| `entrada.wav` | ao carregar o dashboard |
| `transicao.wav` | transições |

Para usar outros nomes, crie `mapeamento.ini` na pasta da versão:

```ini
navegar=nome_do_seu_arquivo.wav
confirmar=outro_arquivo.wav
```

Formatos aceitos: WAV PCM 8/16/24 bits ou float 32, mono ou estéreo, qualquer taxa
(convertido para 44,1 kHz mono), até 12 s e 16 MB. **XMA/XMA2 não é suportado** (codec
proprietário do console) — converta para WAV PCM antes.

Em **Configurações → Sons originais: estado** você vê cada versão (arquivos encontrados, não
confirmados, erros de conversão, esperados × carregados × ausentes, recursos externos, estado
do XEX) e cada efeito, com o arquivo real ou o motivo exato de cada falha (arquivo não
encontrado, XMA, formato inválido, longo demais…). Pode testar os efeitos (A) e verificar a
pasta de novo (Y) sem reiniciar. `8/8` só aparece quando os oito arquivos foram realmente
decodificados.

## Extração automática (em lote) dos sons originais

O repositório **não contém** nenhum áudio original. A ferramenta abaixo inspeciona os XEX que
você possui e extrai, **na sua máquina**, o que for acessível sem contornar proteções,
instalando direto na pasta local.

```bash
python tools/sound_extract.py --entrada <pasta_com_os_xex>
```

Destino padrão: `%LOCALAPPDATA%\NeutroX\local_audio`. Precisa do projeto compilado
(`neutrox_xex_unpack`) e de um FFmpeg com decodificadores `xma1`/`xma2` (o script escolhe
sozinho; há wrappers com o mesmo nome no PATH que não os têm).

Para cada XEX:

1. Lê o cabeçalho XEX2 (texto claro): versão pelo Execution ID, criptografia, compressão,
   pacotes de recursos, bibliotecas de áudio estáticas e importadas.
2. **Corpo criptografado → para aqui** e registra o motivo: abrir exigiria chaves do console
   (contornar proteção). Nenhuma chave é usada ou incluída.
3. Corpo não criptografado → `neutrox_xex_unpack` descomprime a imagem (LZX) e a ferramenta
   percorre os pacotes `XUIZ`: bancos **XACT3** (`.xwb` ondas + `.xsb` cues), áudios RIFF
   soltos, cenas/skins `.xur` que citam sons e referências externas (`sharedres://` = XAM).
   `.xwb`/`.xsb` soltos ao lado do XEX também são lidos.
4. Converte com o FFmpeg para WAV PCM 16 bits, **sem alterar taxa nem canais**, e valida
   canais, taxa, duração (contra a declarada no banco), integridade e silêncio. Falhas viram
   "erros de conversão" (o arquivo não é instalado). Os originais ficam em `brutos/<versão>/`.
5. Atribui funções **somente por evidência verificável**: o nome original da cue/arquivo e a
   skin/cena que o usa. Sem evidência: **não confirmado** (guardado, não usado).
6. Grava por versão `mapeamento.ini`, `ausentes.ini`, `manifesto.ini` e `relatorio.txt`
   (relatório técnico), e na raiz `manifesto.json` e `relatorio_extracao.txt`.

Regras de evidência (nome original → efeito; a primeira da lista tem prioridade, e a cue do
banco XACT do dashboard vence o arquivo solto de mesmo nome):

| Nome original | Efeito | Evidência |
|---|---|---|
| `btn_Focus` | navegar | foco de botão (estado *Focus* da skin) |
| `snd_buttonselect`, `btn_Select`, `select_button` | confirmar | seleção de botão (`selectSoundXACT`) |
| `snd_buttonback`, `btn_Back` | voltar | botão voltar |
| `tab_switch` | categoria | troca de aba |
| `snd_panelunfold` | abrir | painel se abrindo (`PanelFoldSounds`) |
| `btn_InactiveSelect` | erro | seleção de item inativo |
| `snd_transitionfrom` | transicao | transição de cena (`SceneTransitions`) |

Nenhum efeito recebe som de outra versão e nenhum é preenchido por recriação.

### Resultado com os arquivos fornecidos (11/11/2026)

| Versão | Áudios | Efeitos confirmados | Não confirmados | Erros | Situação |
|---|---:|---:|---:|---:|---|
| Blades 6717 | 0 | 0 de 8 | – | – | XEX criptografado: não aberto |
| NXE 9199 | 0 | 0 de 8 | – | – | Idem |
| Kinect 13604 | 0 | 0 de 8 | – | – | Idem |
| Metro V1 15574 | 0 | 0 de 8 | – | – | Idem |
| **Metro V2 17559** | **52** | **7 de 8** | 17 | 0 | banco `dash.xwb`/`dash.xsb` (dentro do XEX) + 16 áudios do `controlp` |

Ausente no Metro V2: **entrada** — nenhuma das 51 cues nem dos áudios soltos tem nome de
abertura. Detalhes de cada versão, cue por cue: [RELATORIO_AUDIO_XEX.md](RELATORIO_AUDIO_XEX.md).

> Correção em relação à 0.7.1: o banco `dash.xwb`/`dash.xsb` **não** é externo — ele está no
> pacote `dashcomm` dentro do XEX. Com a leitura de XACT, `navegar`, `categoria` e `erro`
> passaram a vir do próprio banco do dashboard.

Qualquer áudio original fica só na sua pasta local; `local_audio/` e `sons_originais/` estão
no `.gitignore`.

## Exportar os sons recriados em WAV

```bash
build/Release/neutrox_sound_export.exe assets/sounds/recriados
```

Gera `assets/sounds/recriados/<versão>/<efeito>.wav` (48 arquivos; seis temas × oito
efeitos). Esses arquivos são criação do Neutro X e podem ser redistribuídos.
