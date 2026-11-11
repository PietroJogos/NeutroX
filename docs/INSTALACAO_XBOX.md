# Instalar e testar o Neutro X no Xbox 360 (RGH/JTAG, pelo Aurora)

> **Situação: não testado em hardware.** O `NeutroX.xex` é gerado e tem um cabeçalho XEX2
> válido. Como o Xenia não apresenta a imagem dos títulos do xsdk, a única prova real é o
> teste no console. Até lá, não há declaração de compatibilidade com o console, o Aurora ou o
> RGH.

## O que o aplicativo faz (e o que não faz) no console

- Roda como um título comum, iniciado pelo Aurora. **Não** modifica a NAND, não substitui
  arquivos do sistema, não instala nada, não altera a inicialização nem a configuração do
  RGH e não escreve em partições do sistema.
- Só grava arquivos na **própria pasta** do `.xex` (`game:\`):

  | Arquivo | O que é |
  |---|---|
  | `neutrox.ini` | configurações |
  | `NeutroX.log` | registro de execução |
  | `local_audio/` | sons originais, opcional e sempre local |

- **Para sair:** use o menu do Neutro X (**Sair** → Sim). O título termina e o console volta ao
  dashboard padrão dele. O Neutro X não muda qual é o dashboard padrão.

## Requisitos

- Console RGH ou JTAG cujo dashboard (Aurora) inicie `.xex` não assinados e tenha servidor FTP.
- Um PC na mesma rede, com cliente FTP. O `curl.exe` do Windows serve.
- O arquivo `NeutroX.xex`, que vem no pacote da release ou é compilado com o
  [TOOLCHAIN.md](TOOLCHAIN.md).

## Instalação

1. Crie uma pasta **só para o Neutro X** no HDD, por exemplo `Hdd1:\Apps\NeutroX\`.
2. Copie o `NeutroX.xex` para lá por FTP. Use o endereço do console e o usuário/senha do
   servidor FTP do Aurora (digite-os você mesmo):

   ```bash
   curl.exe -T NeutroX.xex --ftp-create-dirs ftp://<console>/Hdd1/Apps/NeutroX/ --user <usuario>:<senha>
   ```

3. (Opcional) Para usar os sons originais extraídos no seu PC, copie a pasta
   `%LOCALAPPDATA%\NeutroX\local_audio\` para `Hdd1:\Apps\NeutroX\local_audio\`, mantendo as
   subpastas por versão. Esses arquivos são para uso seu: não redistribua.
4. No Aurora, abra o `.xex` pelo gerenciador de arquivos, ou adicione a pasta `Hdd1:\Apps`
   às pastas de busca de conteúdo para o Neutro X aparecer na lista.

## Primeiro teste: o que verificar e me enviar

Depois de abrir e usar o aplicativo por alguns minutos, saia pelo menu e baixe o log:

```bash
curl.exe ftp://<console>/Hdd1/Apps/NeutroX/NeutroX.log --user <usuario>:<senha> -o NeutroX.log
```

| # | Verificação | O que observar |
|---|---|---|
| 1 | Inicialização | A tela de abertura do xsdk aparece e depois o logo/menu do Neutro X. Não pode haver tela preta nem travamento |
| 2 | Cores | Verde do Xbox correto. Se o verde aparecer **roxo/rosa** ou o azul aparecer laranja, a ordem dos canais está trocada |
| 3 | Nitidez | Textos e bordas nítidos, sem borrão nem deslocamento de meio pixel |
| 4 | Controle | Direcional, analógico esquerdo, A/B/X/Y, LB/RB e START respondem. Segurar um direcional repete sem disparar |
| 5 | Cinco interfaces | Blades, NXE, Kinect, Metro V1 e Metro V2 abrem pelo menu e voltam |
| 6 | Desempenho | Ative **Configurações → Mostrar FPS** e anote o valor no menu e em cada interface (as animações de troca são as mais pesadas) |
| 7 | Som | Os efeitos do Neutro X tocam nos menus. Com `local_audio/`, os efeitos do Metro V2 tocam na interface Metro V2 |
| 8 | Configurações | Mude uma opção, saia e abra de novo: ela deve continuar como você deixou (`neutrox.ini` na pasta) |
| 9 | Saída | **Sair** → Sim volta ao dashboard do console sem travar |

O log registra a resolução da tela, o carregamento dos sons, o número de quadros e o
**pior quadro** em milissegundos. Envie o `NeutroX.log` e as observações da tabela. Só com
esse resultado a Fase 10 é dada como concluída.

## Se algo der errado

- **Tela preta ou volta imediata ao dashboard:** baixe o `NeutroX.log`. Ele traz a etapa que
  falhou, por exemplo `xsdk_present_open falhou: 0x...`.
- **Travou:** desligue o console pelo botão. Nada fora da pasta do aplicativo é alterado.
  Envie o log, se ele tiver sido gravado.
- **Não abra o Guia (botão Xbox) para sair.** Um título que termina com o Guia aberto pode
  travar ao encerrar (limitação documentada do xsdk). Saia pelo menu do Neutro X.
