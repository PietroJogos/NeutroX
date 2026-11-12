# Neutro X 0.9.1-beta — revisão do log do Xbox 360

12/11/2026

## Áudio e instalação

O `neutrox.log` da 0.9.0 identifica o executável em `\Device\Mass0\games e homebrews\neutrox\`.
O driver abriu, produziu 48.662 quadros de áudio e registrou zero silêncios por atraso.
A fonte dos dashboards era **Originais**. As aberturas em `game:\local_audio\` falharam
com `0xC0000034` e os cinco dashboards ficaram com 0/8. O ZIP da 0.9.0 e a cópia extraída
que acompanhava o log não continham `local_audio`. O acervo válido estava no PC.

A edição **LOCAL** da 0.9.1 contém `NeutroX.xex` e `local_audio` no mesmo nível.
Os 52 WAVs coincidem com os SHA-256 do manifesto do acervo. São sete efeitos mapeados
no Metro V2, com `entrada` ausente. Blades 6717, NXE 9199, Kinect 13604 e Metro V1 15574
seguem com 0/8. A fonte **Originais** mantém os efeitos ausentes em silêncio.
A edição redistribuível inclui o instalador para preparar ou transferir o acervo local.

O carregador lista os arquivos antes de abrir os WAVs e reaproveita o catálogo ao trocar
de dashboard. A troca lê apenas os WAVs já validados para o tema selecionado. Uma pasta
`local_audio` sem WAVs também deixa de ocultar os WAVs em `sons_originais`.

## Menu e desempenho

Na 0.9.0 foram 269 quadros lentos em 13.119 quadros. O MainMenu respondeu por 220 dos
269. Vários quadros do menu gastaram 36–40 ms no desenho com atualização próxima de
zero. Houve também picos de atualização de até 124 ms no menu. O log antigo informa
a etapa geral e carece de medições das chamadas que compõem a atualização.

Na 0.9.1, a tela do menu reutiliza o quadro em cache até mudar a seleção, a aparência,
o minuto exibido ou os dados mostrados da biblioteca. Sair → Blades e Blades → Sair
trocam imediatamente a seleção e desenham apenas o novo painel. As miniaturas são
preparadas na inicialização. A versão Xbox separa uma thread de hardware para o
áudio e mede separadamente entrada/tela, serviços e troca de banco de áudio em picos
de atualização. A contagem de quadros lentos precisa ser refeita no Xbox.

## Saída, biblioteca e capas

- **Aurora.** A 0.9.0 só registra o destino do `launch.ini` após verificar o arquivo,
  então `usb0:/Aurora/Aurora.xex` existia naquela execução. O log termina depois da
  contagem do áudio, antes de `xsdk_launch`. A 0.9.1 deixa de enfileirar um quadro
  após o pedido de saída, encerra as threads de rasterização, revalida o caminho
  antes da transferência e registra fechamento do áudio, cerca da GPU e apresentação.
  A transferência usa o carregador do XAM. A volta ao Aurora e a ausência de
  reinicialização continuam pendentes de teste físico.
- **Biblioteca.** O log tinha um título salvo e zero pastas monitoradas. Com a busca
  ativada, a revisão procura pastas existentes `Games`, `Jogos`,
  `games e homebrews` e `Content/0000000000000000` nas unidades montadas e
  agenda a busca em segundo plano. Um `default.xex` inválido na pasta pai
  deixa de bloquear a visita às subpastas. As configurações existentes são preservadas.
- **Capas.** Em 12/11, o XboxUnity devolveu HTTP 200 e `CoversCount: 0` para
  `05AB81F0`. O controle `4D5307E6` devolveu 40 capas. O log não traz o arquivo
  do jogo e o nome do título permanece indeterminado. Ao atualizar uma capa
  manualmente, o aplicativo confere o ID no arquivo antes de consultar. Para uma
  imagem própria, coloque `05AB81F0.jpg` ou `05AB81F0.png` em `capas_importar`
  ao lado do XEX e escolha **Meus Jogos → Y → Importar capas**.
  [Consulta do ID](http://xboxunity.net/Resources/Lib/CoverInfo.php?titleid=05AB81F0)
  [Consulta de controle](http://xboxunity.net/Resources/Lib/CoverInfo.php?titleid=4D5307E6)

## Compilação e conferência

A compilação Release para PowerPC foi feita com GNU 15.3 e xsdk `3960a1d`. O cabeçalho
do XEX confirma Title ID `4E455558` e versão `0.9.1.0`. SHA-256
`00dc502d49b871f054b218c604971c9c20f538bc95cd24a78b21a0c06c283c78`.

A tentativa de rodar a nova prévia Win32 foi bloqueada pelo Windows com um alerta
de segurança. O executável Win32 da prévia ficou fora da entrega. A validação do
núcleo usa o binário headless.

Veja as [instruções para o Xbox](INSTALACAO_XBOX.md) para instalar o pacote local
e coletar o log do teste físico.

### Resultados no PC

- Suite headless Release com o acervo real instalado em uma pasta de teste: **1.174
  verificações aprovadas, zero falhas**. O carregador validou 52 WAVs e registrou
  7/8 no Metro V2. A suíte cobriu a volta Sair → Blades, a descoberta de pastas
  com dispositivos simulados, capas via HTTP local e os destinos de saída.
- Em 1280×720, a medição headless do menu parado registrou 0,08 ms de CPU por
  quadro. A medição é do PC. O tempo do Xenon e a apresentação no Xbox devem
  ser medidos com o novo log.
- Os dois ZIPs passaram na verificação de integridade. O pacote local contém os 52
  WAVs, os cinco arquivos `ausentes.ini` e o mesmo XEX compilado. O pacote
  redistribuível contém apenas o executável do projeto, o instalador, o guia e
  as licenças. O `--dry-run` da edição local lista o destino USB e os sete efeitos.
