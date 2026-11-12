# Saída do Neutro X e início de jogos no Xbox 360

Versão 0.9.0-beta. **Nada aqui foi testado no console ainda**: a explicação vem do código do
xsdk (MIT) e da documentação dele (`docs/GUIDE.md`, `docs/HARDWARE.md`, `src/exit.c`,
`src/launch.c`). O teste real está em [INSTALACAO_XBOX.md](INSTALACAO_XBOX.md).

## Três coisas diferentes

| O que é | Como acontece | Quem faz | O Neutro X usa? |
|---|---|---|---|
| **Reinicialização completa** | o console é desligado/religado (no RGH, o glitch roda de novo) | `HalReturnToFirmware`, botão de força, ou o kernel após um erro fatal (*bugcheck*) | **Não.** Nenhuma função de reinício é chamada |
| **Reinicialização parcial** | o kernel continua; o XAM descarrega o título e carrega o dashboard | `XamLoaderTerminateTitle` | Só como último recurso (saída padrão) |
| **Troca de aplicativo** | o título atual é encerrado e o XAM carrega **outro `.xex`** diretamente | `XamLoaderLaunchTitle(caminho)` | **Sim:** "Voltar ao Aurora", "Voltar ao Xbox Dashboard" e "Jogar" |

## Por que o console reiniciava ao sair (0.8.0)

Na 0.8.0, "Sair" fazia `main` retornar. O xsdk então chama `exit()` →
`XamLoaderTerminateTitle()` → dashboard do sistema. Isso, sozinho, **não** reinicia o
console. Um reinício completo nessa hora indica que **algo travou ou falhou ao encerrar**, e
o kernel reiniciou o console. Sem o `NeutroX.log` dessa execução não dá para afirmar qual
foi a falha. Estes pontos do encerramento eram frágeis e foram corrigidos:

1. **Áudio fechado com o alimentador ainda ativo.** A 0.8.0 chamava `xsdk_audio_close()`
   enquanto a thread de áudio podia estar no meio de um quadro. Agora a thread é parada e
   aguardada **antes** de fechar o cliente do driver. Os contadores de áudio vão para o log.
2. **Ordem de liberação.** Agora é sempre: aplicativo (telas, texturas, biblioteca salva) →
   áudio → espera da GPU → tela → GPU. Só depois disso o próximo título é iniciado. Cada
   etapa escreve uma linha no log (`neutrox: encerrando o aplicativo`,
   `neutrox: liberando audio, GPU e tela`, `neutrox: iniciando ...`). Se o console reiniciar
   de novo, a última linha mostra onde parou.
3. **Destino explícito.** Em vez de depender de para onde o sistema/DashLaunch manda depois
   de `XamLoaderTerminateTitle`, o Neutro X inicia diretamente o `.xex` escolhido.

## O menu Sair

| Opção | O que faz |
|---|---|
| **Voltar ao Aurora** | Procura o `Aurora.xex`, nesta ordem: (1) caminho definido em Configurações → Jogos e pastas; (2) `Default` do `launch.ini` do DashLaunch, se apontar para um arquivo do Aurora (o `launch.ini` é **só lido**, nunca alterado); (3) locais comuns (`Hdd1:\Aurora\`, `Hdd1:\Apps\Aurora\`, `Usb0:\Aurora\`…). Encontrou: troca de aplicativo. Não encontrou: a opção aparece desabilitada, com o motivo |
| **Voltar ao Xbox Dashboard** | Inicia `\SystemRoot\dash.xex`, o dashboard oficial que o kernel aponta (flash ou `$SystemUpdate`), lido só para conferir que existe |
| **Cancelar** | Volta ao Neutro X |

**Limitação: DashLaunch.** Se o DashLaunch estiver configurado para substituir o dashboard
oficial (por exemplo, `Default = Aurora` e redirecionamento do `dash.xex`), ele pode
interceptar "Voltar ao Xbox Dashboard" e abrir o Aurora. Isso é configuração do DashLaunch,
que o Neutro X não lê além do `launch.ini` e nunca altera.

Na prévia do Windows, as duas opções apenas fecham a janela.

## Iniciar jogos ("Jogar")

1. **Detalhes** do jogo → **Jogar** → confirmação.
2. O Neutro X lê de novo o cabeçalho do arquivo e confere que ainda existe e que o Title ID
   é o mesmo da biblioteca.
3. Salva a biblioteca (data em que foi jogado) e encerra os próprios recursos, como na saída.
4. `xsdk_launch(caminho)` → `XamLoaderLaunchTitle`. O xsdk liga uma unidade `launch:` à
   pasta do jogo, porque o XAM não aceita caminhos de dispositivo.

| Tipo | Situação |
|---|---|
| Jogo extraído (`default.xex`) | Caminho normal do loader do sistema. **Não testado no console** |
| GOD / XBLA (pacote STFS) | **Desativado por padrão** (Configurações → Jogos → "Iniciar GOD/XBLA (experimental)"). Quando ligado, o cabeçalho do pacote é entregue ao mesmo carregador; se o XAM não aceitar, o console volta ao dashboard. Não foi verificado |

O Neutro X não contorna licenças, DRM nem assinaturas: quem carrega o título é o próprio
sistema, com as regras dele. Se um tipo de título não puder ser iniciado, a tela de detalhes
explica o motivo; nenhuma execução bem-sucedida é simulada.

## Como diagnosticar

Baixe o `NeutroX.log` (pasta do `.xex`) logo depois do teste. Linhas úteis:

```
neutrox: encerrando o aplicativo
neutrox: audio: N quadros tocados, M de silencio por atraso
neutrox: liberando audio, GPU e tela
neutrox: iniciando hdd:/Aurora/Aurora.xex
exit: status 0, launching launch:\Aurora.xex      (linha do proprio xsdk)
```

Se a última linha for anterior a `exit:`, o problema ocorreu no encerramento. Envie o log.
