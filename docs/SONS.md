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

## Por que os sons originais não vêm extraídos

Resumo da análise dos cinco XEX (detalhes em [`RELATORIO_FASE1.md`](RELATORIO_FASE1.md#3-recursos-de-áudio-nos-xex)):

| Versão | Situação | Motivo |
|---|---|---|
| Blades 6717 | Não extraível | Corpo criptografado (AES); exigiria chaves do console = contornar proteção |
| NXE 9199 | Não extraível | Idem |
| Kinect 13604 | Não extraível | Idem |
| Metro V1 15574 | Não extraível | Idem |
| Metro V2 17559 | Não verificado | Não criptografado, mas comprimido (LZX); nenhum nome de recurso identifica áudio, e o conteúdo é material da Microsoft |

Nenhum dos cinco arquivos contém áudio em texto claro, e nenhum pacote de recursos tem nome
de áudio — não é possível dizer, a partir do pacote, quais sons pertencem a cada versão.
Qualquer áudio original que você usar fica só na sua pasta local e fora do repositório.

## Exportar os sons recriados em WAV

```bash
build/Release/neutrox_sound_export.exe assets/sounds/recriados
```

Gera `assets/sounds/recriados/<versão>/<efeito>.wav` (48 arquivos; seis temas × oito
efeitos). Esses arquivos são criação do Neutro X e podem ser redistribuídos.
