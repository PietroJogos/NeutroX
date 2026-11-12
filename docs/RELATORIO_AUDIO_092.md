# Neutro X 0.9.2-beta — lote de áudio por versão

12/11/2026

## Resultado dos arquivos locais

O lote aceitou os ZIPs de 6717, 9199, 13604 e 17559, o XEX avulso de 15574 e o
ZIP de atualização de 15574 disponível no PC. A versão dos executáveis principais
foi conferida pelo Execution ID completo e pelo Title ID `FFFE07D1`.

| Dashboard | WAVs únicos | Efeitos confirmados | Próximo material necessário |
|---|---:|---:|---|
| Blades 2.0.6717.0 | 0 | 0/8 | Recursos legíveis de 6717. O cabeçalho indica `dashcomm` (375.492 bytes). |
| NXE 2.0.9199.0 | 0 | 0/8 | Recursos legíveis de 9199. O cabeçalho indica `dashcomm` (623.631 bytes). |
| Kinect 2.0.13604.0 | 0 | 0/8 | Recursos legíveis de 13604. O cabeçalho indica `dashcomm` (996.091 bytes). |
| Metro V1 2.0.15574.0 | 0 | 0/8 | Recursos legíveis de 15574. O cabeçalho indica `dashcomm` (927.624 bytes). |
| Metro V2 2.0.17559.0 | 52 | 7/8 | Fonte da cue de entrada dessa build. O nome do arquivo ou da cue não foi identificado. |

Os quatro primeiros XEX têm corpo AES. `dashcomm` é um candidato a investigar
quando houver uma cópia legível da mesma build. Os nomes internos dos bancos e a
presença de áudio nesses quatro pacotes permanecem indeterminados. Um par XACT
exportado da build correta ou um XUIZ legível com os sons e suas cenas são
entradas aceitas pelo importador.

## Conteúdo do Metro V2

O ZIP traz dois XEX 17559 com SHA-256 distintos. Ambos produziram a mesma imagem
PE de 16.941.056 bytes com SHA-256
`06a8446849184331dc1b513a894c08ad3f160dcec8473f70cdcae9f54b7c86f9`.
Foram lidos 36 recursos. O banco `dashcomm/dash.xwb` contém 36 ondas XMA2
com nomes de cues em `dashcomm/dash.xsb`. `controlp` contém 16 áudios RIFF
adicionais. O lote consolidou as duas proveniências por SHA-256 do PCM.

| Efeito | Cue selecionada |
|---|---|
| Navegar | `btn_Focus` |
| Confirmar | `snd_buttonselect` |
| Voltar | `snd_buttonback` |
| Categoria | `tab_switch` |
| Abrir | `snd_panelunfold` |
| Erro | `btn_InactiveSelect` |
| Transição | `snd_transitionfrom` |

O banco também tem `snd_panelfold`, identificado como fechamento de painel.
Ele foi preservado entre os WAVs. A revisão mantém os oito slots existentes e
o evento de voltar continua usando sua própria cue.

As skins `dashuisk/skin.xur` e `SharedUI/MPDashSkin.xur` referenciam
`sharedres://btn_back.xma`, `sharedres://btn_focus.xma`,
`sharedres://btn_inactiveselect.xma` e `sharedres://btn_select.xma`.
`network/2004_NetworkDetails.xur` referencia `sharedres://tab_switch.xma`.
Para resolver esses URIs seriam necessários os cinco XMA compartilhados da
build 17559 ou o pacote XUIZ legível do XAM que os contém. A atualização inclui
`L.xam.xex.shrdres.xzp` com assinatura `BDES`. Esse arquivo é um candidato pelo
nome, sem associação confirmada pelo conteúdo. Os 115 XZP BDES do ZIP foram
inventariados. Os efeitos disponíveis foram mapeados pelas cues do banco interno.
Os cinco URIs observados não identificam o som de entrada.

A análise estática de `luaxbox/xuisound.lub` encontrou duas instruções `CALL`
em Lua 5.1, para `Play` no protótipo 0.4 e `Stop` no protótipo 0.7, ambas
na posição 3 a partir de zero. Isso localiza o encaminhamento de reprodução
e parada em Lua. Os vínculos entre ação e cue vêm dos bancos e referências
XUI. O REA inventariou o ZIP do Kinect. O provedor de PE do REA recusou
a imagem do Metro V2 por arquitetura PowerPC 0x01f2, então a inspeção de
chamadas ficou restrita ao bytecode Lua legível. [Formato XEX](https://free60.org/System-Software/Formats/XEX/) · [Bytecode Lua 5.1](https://www.lua.org/source/5.1/lopcodes.h.html)

## Módulos auxiliares

A inspeção complementar recusou os 17 módulos candidatos criptografados em
13604 e os 18 em 15574. Em 17559, abriu 19 imagens auxiliares sem criptografia.
Foram observadas 11 ocorrências RIFF/XMA e obtidos 8 WAVs únicos. O importador
mantém esses sons em `diagnostico/auxiliares` do pacote USB. Eles não são
carregados pelo aplicativo e não entram nos 40 slots.

| Módulo 17559 | Recursos encontrados |
|---|---|
| `Dash.FieldCalibration.lex` | `successSound.xma`, `completedSound.xma` |
| `Dash.NuiTroubleshooter.lex` | `countdown_sound.xma`, `volumesound.xma` |
| `Dash.Search.xex` | `vk_Focus.xma`, `vk_Select.xma` |
| `Guide.AccountRecovery.xex` | `btn_backG.xma`, `btn_focusG.xma`, `btn_selectG.xma`, `vk_Focus.xma`, `vk_Select.xma` |

## Reproduzir o lote e testar no console

Use Python 3.12 e FFmpeg com decodificadores XMA/XMA2. O beta redistribuível traz
o extrator de XEX sem criptografia e os scripts. Execute da pasta descompactada:

```powershell
python ferramentas/sound_extract.py --entrada "C:\Acervo\6717.zip" --entrada "C:\Acervo\9199.zip" --entrada "C:\Acervo\13604.zip" --entrada "C:\Acervo\15574.xex" --entrada "C:\Acervo\17559.zip" --destino ".\catalogo\local_audio" --pacote-local ".\NeutroX-USB" --xex-app ".\NeutroX.xex"
```

`--entrada` aceita ZIP, XEX ou diretório. Para recursos já exportados cuja
origem é conhecida, passe `--recursos "13604=C:\Acervo\Kinect"`. O relatório
registra a versão declarada. Recursos sem associação ficam fora dos catálogos.
Se houver mapeamentos com evidência equivalente e conteúdos distintos, o
efeito fica ausente. O manifesto JSON reúne a cadeia de origem e os hashes.

No pacote USB local, copie `NeutroX.xex` e `local_audio/` para a mesma pasta
que o Aurora usa. No menu de áudio, confira 7/8 no Metro V2 e selecione
**Originais**. Teste navegação, confirmação, retorno, categoria, abertura,
transição e a troca de dashboards. Após sair para o Aurora, confira
`Original validado` e `Original com sinal no mixer` no `neutrox.log`.
O primeiro registro indica WAV validado no disco. O segundo conta vozes
originais com contribuição PCM, que pode ser comparada às estatísticas do
dispositivo. A validação física do novo XEX está pendente. O retorno ao Aurora
e os cinco quadros lentos relatados pelo usuário foram observados na 0.9.1.

## Validação no PC

- 1.292 verificações C++ e três testes Python passaram. O teste com o acervo
  real exercitou os 40 slots, obteve PCM para os sete efeitos mapeados e
  silêncio para os 33 ausentes. A troca entre versões e a saída para o
  Aurora simulado também fazem parte da suíte.
- O XEX Release foi compilado para PowerPC com xsdk. O cabeçalho confirma a
  versão `0.9.2.0` e Title ID `4E455558`. SHA-256
  `fe2f32e229e18f9d98d9a0cb1676cfc1fc519d18bc2bec3efb15652c7ba6d5f2`.
- O pipeline de atualização do menu e a transferência para o Aurora
  permanecem com a implementação da 0.9.1. O mixer acrescenta os
  contadores por efeito original. O teste de áudio da 0.9.2 no hardware
  e a medição de quadros lentos continuam pendentes.
