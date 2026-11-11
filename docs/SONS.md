# Sons do Neutro X

## Três conjuntos, cada um no seu lugar

| Conjunto | Onde fica | Vai para o GitHub? |
|---|---|---|
| **Sons do Neutro X** (menu, configurações, transições) | Gerados por código (`src/audio/themes/neutrox.cpp`) | Sim (são originais do projeto) |
| **Sons originais dos dashboards** (seus arquivos) | `<pasta de dados>/sons_originais/<versão>/` | **Nunca** (`.gitignore`) |
| **Sons recriados por versão** (alternativa opcional) | Gerados por código (`src/audio/themes/<versão>.cpp`); exportáveis em WAV | Sim (são originais do projeto) |

A fonte usada nos dashboards é escolhida em **Configurações → Sons dos dashboards**:

- **Originais (pasta local)** — padrão. Toca apenas os arquivos que você colocou na pasta.
  **Um efeito ausente fica mudo**; ele não é trocado automaticamente por uma recriação.
  Ao abrir um dashboard incompleto aparece um aviso com a contagem (ex.: "0 de 8").
- **Recriados pelo Neutro X** — só quando você escolhe explicitamente.

O tema muda sozinho com a tela: dashboard ativo → sons daquela versão; menus do Neutro X
(inclusive o menu do START por cima de um dashboard) → sons do Neutro X.

## Pasta dos sons originais

Windows (prévia): `%LOCALAPPDATA%\NeutroX\sons_originais\`
Console (Fase 10): dentro da pasta do próprio aplicativo.

O Neutro X cria a estrutura e um `LEIA-ME.txt` na primeira execução:

```text
sons_originais/
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

Em **Configurações → Sons originais: estado** você vê cada versão e cada efeito, com o
motivo exato de cada falha (arquivo não encontrado, XMA, formato inválido, longo demais…),
pode testar os efeitos (A) e verificar a pasta de novo (Y) sem reiniciar.

## Extração automática (em lote) dos sons originais

O repositório **não contém** nenhum áudio original. A ferramenta abaixo extrai, **na sua
máquina**, o que for acessível nos XEX que você possui e instala direto na pasta local.

```bash
python tools/sound_extract.py --entrada <pasta_com_os_xex>
```

Padrão de destino: `%LOCALAPPDATA%\NeutroX\sons_originais`. Precisa do projeto compilado
(`neutrox_xex_unpack`) e de um FFmpeg com decodificador XMA (o script escolhe sozinho um
FFmpeg que tenha `xma1`/`xma2`; há wrappers com o mesmo nome no PATH que não têm).

O que a ferramenta faz, para cada XEX:

1. Identifica a versão pelo Execution ID do cabeçalho (6717, 9199, 13604, 15574, 17559).
2. **Corpo criptografado → recusa** e registra o motivo: abrir exigiria chaves do console
   (contornar proteção). Nenhuma chave é usada ou incluída.
3. Corpo não criptografado → `neutrox_xex_unpack` monta o fluxo LZX e descomprime a imagem
   (verificada: cabeçalho MZ/PE válido; o resultado é idêntico byte a byte nas versões
   MSVC e GCC da ferramenta) e grava os 36 pacotes de recursos `XUIZ`.
4. Lê a tabela de arquivos de cada pacote e extrai os áudios embutidos (RIFF; XMA 44,1 kHz).
5. Converte para WAV PCM 16 bits **sem alterar taxa nem canais**; o original fica em
   `sons_originais/brutos/<versão>/`.
6. Atribui funções **somente pelo nome original do arquivo** (regras abaixo); o que não
   tem nome correspondente fica na pasta como áudio original sem função.
7. Grava `mapeamento.ini`, `ausentes.ini` (efeito → causa da ausência) e
   `relatorio_extracao.txt`. O Neutro X lê tudo sozinho e troca o conjunto inteiro ao trocar
   de dashboard.

Regras de evidência (nome original → efeito):

| Nome original | Efeito | Evidência |
|---|---|---|
| `btn_focus` | navegar | foco do botão |
| `snd_panelright` | navegar | movimento entre painéis (se não houver `btn_focus`) |
| `btn_select`, `snd_buttonselect` | confirmar | botão selecionar |
| `btn_back`, `snd_buttonback` | voltar | botão voltar |
| `tab_switch`, `snd_channelup` | categoria | troca de aba/canal |
| `snd_panelunfold` | abrir | painel se abrindo |
| `btn_inactiveselect` | erro | seleção de item inativo |
| `snd_transitionfrom` | transicao | transição de tela |

Nenhum efeito recebe som de outra versão e nenhum é preenchido por recriação.

### Resultado com os arquivos fornecidos (11/11/2026)

| Versão | Arquivos originais | Efeitos com função | Situação |
|---|---:|---:|---|
| Blades 6717 | 0 | 0 de 8 | XEX criptografado: não aberto (seria contornar proteção) |
| NXE 9199 | 0 | 0 de 8 | Idem |
| Kinect 13604 | 0 | 0 de 8 | Idem |
| Metro V1 15574 | 0 | 0 de 8 | Idem |
| **Metro V2 17559** | **16** | **6 de 8** | Extraídos do pacote `controlp` dentro do XEX |

Metro V2 — os 16 áudios encontrados (todos XMA 44,1 kHz, convertidos e conferidos: sinal
contínuo, autocorrelação ≈ 1, canais estéreo correlacionados — não é ruído):

| Arquivo original | Função atribuída |
|---|---|
| `snd_panelright` | **navegar** |
| `snd_buttonselect` | **confirmar** |
| `snd_buttonback` | **voltar** |
| `snd_channelup` | **categoria** |
| `snd_panelunfold` | **abrir** |
| `snd_transitionfrom` | **transicao** |
| `snd_channeldown`, `snd_panelleft`, `snd_panelfold`, `progress_meter` | sem função própria no Neutro X (guardados) |
| `handles/engage`, `disengage`, `handle_grab_1`, `wave_success`, `snd_pageleft`, `snd_pageright` | sons de gestos do Kinect presentes no 17559 (guardados) |

Ausentes no Metro V2: **erro** e **entrada**. O XEX não tem áudio com esses nomes; ele
**referencia** um banco de sons externo, `dash.xwb`/`dash.xsb` (onde ficam `btn_Focus`,
`btn_Select`, `btn_Back`, `btn_InactiveSelect`, `tab_Switch`), que **não faz parte** dos
arquivos fornecidos. Se você tiver esse banco, coloque-o na mesma pasta do XEX: a ferramenta
o detecta e registra, mas a leitura de bancos XACT ainda não está implementada.

Qualquer áudio original fica só na sua pasta local; `sons_originais/` está no `.gitignore`.

## Exportar os sons recriados em WAV

```bash
build/Release/neutrox_sound_export.exe assets/sounds/recriados
```

Gera `assets/sounds/recriados/<versão>/<efeito>.wav` (48 arquivos; seis temas × oito
efeitos). Esses arquivos são criação do Neutro X e podem ser redistribuídos.
