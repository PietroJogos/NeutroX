# Instalar e testar o Neutro X no Xbox 360 (RGH/JTAG, pelo Aurora)

> **Situação (0.9.0-beta): não testado em hardware.** A 0.9.0 corrige o que foi relatado no
> primeiro uso no console (reinício ao sair, travadas, falta de som) e traz a biblioteca de
> jogos reais. Nada disso foi verificado no Xbox 360 ainda; a Fase 10 só fecha com o seu
> teste.

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

## Instalação (recomendado: ferramenta do PC)

1. No Aurora, anote o IP do console e o usuário/senha do FTP.
2. No PC, na pasta do projeto ou do pacote, rode:

   ```bash
   python tools/xbox_install.py --host <IP-do-console> --xex NeutroX.xex
   ```

   A ferramenta pede a senha (não a guarda), cria `Hdd1:\Apps\NeutroX\` e envia o `.xex` e
   a pasta `local_audio/` gerada por `tools/sound_extract.py`. Use `--dry-run` para só ver
   a lista e `--dest /Hdd1/OutraPasta` para mudar o destino. Ela nunca apaga nada no console.
3. No Aurora, abra o `NeutroX.xex` pelo gerenciador de arquivos (ou inclua `Hdd1:\Apps` nas
   pastas de busca de conteúdo).

Manual, sem a ferramenta: copie o `NeutroX.xex` (e, se quiser, a pasta
`%LOCALAPPDATA%\NeutroX\local_audio\` inteira) para `Hdd1:\Apps\NeutroX\` com um cliente FTP.

## Teste da 0.9.0: o que verificar e me enviar

Use por alguns minutos e saia por **Sair → Voltar ao Aurora**. Depois baixe o log:

```bash
curl.exe ftp://<console>/Hdd1/Apps/NeutroX/NeutroX.log --user <usuario>:<senha> -o NeutroX.log
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
| 9 | **Meus Jogos** | Configurações → Jogos: gerenciar pastas → adicionar `Hdd1:\Games` (ou a sua pasta). Os jogos aparecem com nome, Title ID e caminho; sem jogos falsos |
| 10 | Capas | Com internet, as capas aparecem sozinhas em alguns segundos; sem rede, ficam as já salvas |
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
