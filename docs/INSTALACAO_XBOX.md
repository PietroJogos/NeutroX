# Instalar e testar o Neutro X no Xbox 360 (RGH/JTAG, pelo Aurora)

> O log da 0.9.0 confirma execução no Xbox 360. A 0.9.1 tem compilação XEX
> concluída e aguarda o teste físico descrito abaixo.

## O que o aplicativo faz (e o que não faz) no console

- Roda como um título comum, iniciado pelo Aurora. **Não** modifica a NAND, não substitui
  arquivos do sistema, não instala nada, não altera a inicialização, o RGH, o DashLaunch
  (o `launch.ini` só é lido) nem o Aurora, e não escreve em partições do sistema.
- Lê, sem alterar, as pastas de jogos que você escolher.
- Só grava na **própria pasta** do `.xex`:

  | Arquivo | O que é |
  |---|---|
  | `neutrox.ini` | configurações |
  | `biblioteca.ini` | biblioteca de jogos (pastas, favoritos, datas) |
  | `capas/` | capas baixadas/importadas |
  | `NeutroX.log` | registro de execução |
  | `local_audio/` | sons originais, opcional, sempre local (não redistribua) |

## Instalação da 0.9.1

Abra o ZIP **LOCAL** no PC e copie `NeutroX.xex` e a pasta `local_audio` completa para
a mesma pasta de onde o Aurora já abre o aplicativo. No log recebido, a instalação estava
em `Usb0:\games e homebrews\neutrox\`. Preserve `neutrox.ini`, `biblioteca.ini`
e as capas existentes. Abra o XEX atualizado no Aurora.

O ZIP local inclui 52 WAVs originais verificados do Metro V2 e os arquivos de diagnóstico
das cinco versões. A expectativa é 7/8 efeitos originais no Metro V2 e 0/8 nos outros
quatro dashboards. O ZIP com áudio é para uso local. A edição redistribuível traz o
aplicativo e o instalador para importar a partir dos seus arquivos locais.

Para instalar por FTP a partir do ZIP local extraído, use o instalador incluído em
`ferramentas/`. Ajuste a pasta se o Aurora abre o XEX de outro local.

```powershell
python ferramentas/xbox_install.py --host <IP-do-console> --dest "/Usb0/games e homebrews/neutrox"
```

Use `--dry-run` para conferir as cinco contagens e o destino. Use `--no-audio`
quando quiser enviar apenas o aplicativo. Para montar a pasta de instalação com
outro acervo, use `--prepare-local <pasta>` junto de `--xex` e `--local-audio`.

## Teste físico da 0.9.1

Use por alguns minutos e saia por **Sair → Voltar ao Aurora**. Depois baixe o log:

```bash
curl.exe "ftp://<console>/Usb0/games%20e%20homebrews/neutrox/neutrox.log" --user <usuario> -o neutrox.log
```

| # | Verificação | O que observar |
|---|---|---|
| 1 | Inicialização | Logo do xsdk e depois o menu do Neutro X, sem tela preta |
| 2 | Cores e nitidez | Verde correto (não roxo), textos nítidos |
| 3 | Controle | Direcional, analógico, A/B/X/Y, LB/RB, START |
| 4 | **Navegação fluida** | Com **Mostrar FPS** ligado, percorra cada interface do primeiro ao último item e volte. Anote o FPS e os números "lógica/desenho/tela" nas trocas de lâmina/canal/página |
| 5 | Cinco interfaces | Abrir, trocar várias vezes pelo START, voltar ao menu |
| 6 | **Som** | Configurações → Sons originais: estado → **X (Testar saída)**: o tom do Neutro X deve tocar. Depois, no Metro V2 (com `local_audio`), navegar deve tocar os originais. Anote o que a linha "Saída" mostra |
| 7 | **Sair → Voltar ao Aurora** | Volta ao Aurora **sem reiniciar o console** |
| 8 | **Sair → Voltar ao Xbox Dashboard** | Abre o dashboard oficial (ou o Aurora, se o DashLaunch redirecionar) sem reiniciar |
| 9 | **Meus Jogos** | Confira quais pastas foram detectadas em Configurações → Jogos. Para um diretório personalizado, adicione a pasta e use Procurar agora. Confira nome, Title ID e caminho em Detalhes |
| 10 | Capas | Confira o Title ID em Detalhes. Para `05AB81F0`, importe uma imagem própria com esse nome em `capas_importar/` |
| 11 | Perfil | O nome do seu perfil aparece nas interfaces; trocar de perfil pelo Guia atualiza o nome |
| 12 | **Jogar** (um jogo extraído) | Detalhes → Jogar → Sim: o jogo inicia sem reiniciar o console |
| 13 | Persistência | Favorite um jogo, saia e abra de novo: favorito e pastas continuam |

Envie o `NeutroX.log` e as observações. O log traz: resolução, estado do áudio, sons
carregados e mudos (com o motivo), perfil, unidades encontradas, resumo da busca de jogos,
capas baixadas ou recusadas, quadros lentos (com a parte que pesou), tempos por tela e as
etapas do encerramento.

## Se algo der errado

- **Reiniciou ao sair ou ao jogar:** a última linha do log mostra até onde o encerramento
  chegou ([SAIDA_E_INICIALIZACAO.md](SAIDA_E_INICIALIZACAO.md)). Envie o log.
- **Sem som:** use **Testar saída**. Se o tom tocar, o problema são os arquivos (veja quais
  ficaram mudos no log). Se não tocar, a linha "Saída" mostra o erro do driver.
- **Tela preta ou volta imediata:** o log traz a etapa que falhou (ex.: `xsdk_present_open falhou`).
- **Travou:** desligue pelo botão. Nada fora da pasta do aplicativo é alterado. Envie o log.
- **Não saia pelo Guia (botão Xbox).** Use o menu Sair do Neutro X.
