# Meus Jogos: biblioteca de jogos reais

Na 0.9.1, o primeiro uso com busca ao iniciar e sem pastas configuradas procura
`Games`, `Jogos`, `games e homebrews` e `Content/0000000000000000` nas unidades
montadas. Só adiciona diretórios que existem. Se você usa outra pasta, selecione-a
em Configurações → Jogos. **Procurar agora** também tenta detectar essas pastas
quando a lista está vazia.

## O que aparece

Só títulos **reais** encontrados nas pastas que você escolher. Nenhum jogo de teste ou de
demonstração aparece como instalado. Se a biblioteca estiver vazia, as cinco interfaces
mostram "Meus Jogos (vazio)" e levam à configuração de pastas.

Um arquivo só vira jogo se o **cabeçalho real** for válido:

| Formato | Onde costuma estar | O que é lido (só o cabeçalho, em texto claro) | Recusado quando |
|---|---|---|---|
| Jogo extraído | `...\NomeDoJogo\default.xex` | XEX2: Title ID, Media ID, versão, disco N de M, flags do módulo | não é XEX2, é DLL, é patch/atualização, não tem a marca de título ou não tem Execution ID |
| Games on Demand | `Content\0000000000000000\<TitleID>\00007000\<arquivo>` | STFS (`LIVE`/`CON `/`PIRS`): tipo 0x7000, Title ID, nome exibido, miniatura PNG | outro tipo de conteúdo (saves, perfis, DLC, temas: ignorados sem aviso) |
| Xbox Live Arcade | `...\000D0000\<arquivo>` | STFS tipo 0xD0000 | idem |
| Aplicativo (`.xex` com outro nome) | qualquer | XEX2 | desligado por padrão |

Nada da imagem criptografada/comprimida é lido e nada é executado durante a busca. O
próprio Neutro X (Title ID `4E455558`) é ignorado.

**Nome do jogo:** o nome da pasta, nos extraídos (uma pasta `Disc 1`/`Disc 2` usa a de
cima, mais "(disco N)"), ou o nome do pacote, nos GOD/XBLA. O cabeçalho XEX não traz o nome
em texto claro.

## Funções

| Função | Onde |
|---|---|
| Adicionar pasta de jogos (navegando pelo HDD, USB e unidades de memória) | Configurações → Jogos: gerenciar pastas → + Adicionar pasta |
| Procurar automaticamente ao iniciar / Procurar agora | idem (a busca roda em segundo plano, sem travar a tela) |
| Profundidade máxima (por pasta e padrão, 1–12) | idem (←/→) |
| Tipos de conteúdo (extraídos, GOD, XBLA, aplicativos) | idem |
| Pastas ignoradas (nome, como `Cache`, ou caminho completo) | idem |
| Detectar e ocultar duplicatas (mesmo Title ID + Media ID + disco) | idem: a primeira cópia aparece; os detalhes dela mostram quantas cópias existem |
| Remover pasta monitorada (as entradas dela saem; os arquivos ficam) | idem → A na pasta |
| Adicionar jogo manualmente (escolher um `.xex` ou pacote) | idem, ou Meus Jogos → Y |
| Atualizar a biblioteca | Meus Jogos → Y → Atualizar |
| Ordenar por nome (A–Z, sem diferenciar maiúsculas e acentos) | sempre |
| Pesquisar (teclado na tela) | Meus Jogos → Y → Pesquisar, ou LB/RB até "Pesquisa" |
| Favoritar | X na lista ou nos detalhes; filtro "Favoritos" |
| Adicionados recentemente | filtro "Recentes" (os 24 mais novos) |
| Jogos indisponíveis (arquivo sumiu) | ficam marcados "INDISPONÍVEL" e no filtro "Indisponíveis" (não somem sozinhos) |
| Detalhes: nome, Title ID, Media ID, versão, tipo, disco, caminho, origem, datas, estado, duplicatas, origem da capa | A em um jogo |
| Jogar / Favoritar / Atualizar capa / Remover da biblioteca / Configurações | botões dos detalhes |
| Remover da biblioteca **sem apagar o jogo** | a entrada sai e a pasta vai para os ignorados (não volta na próxima busca) |

**Visual:** capas grandes em **carrossel** horizontal (com volta do último ao primeiro a
partir de 9 jogos) ou em **grade** de 6 colunas (Y → Visualização). As capas entram com
transição suave quando ficam prontas. A biblioteca é **uma só**, e a aparência segue a
interface ativa ou a última aberta: Blades (escuro com laranja), NXE (céu claro), Kinect
(claro com verde), Metro V1 e Metro V2 (escuros, títulos finos e em minúsculas). Também
abre pelo menu principal, pelo menu START e pelos atalhos "Meus Jogos" de cada interface.

## Capas

**Fonte automática: XboxUnity**, o serviço usado pelo Aurora. Verificado em 11/11/2026: o
site responde. Endpoints usados (HTTP simples; o servidor também aceita HTTPS):

```
http://xboxunity.net/Resources/Lib/CoverInfo.php?titleid=<TitleID>     -> JSON {"Covers":[{"CoverID","Official","Rating",...}]}
http://xboxunity.net/Resources/Lib/Cover.php?size=large&cid=<CoverID>   -> JPEG (o servidor diz image/png; o conteúdo é JPEG)
```

- Escolha: capa **oficial** primeiro, depois a mais bem avaliada. Se a grande falhar, tenta
  a pequena.
- **Validação:** status 200, tamanho de 512 bytes a 6 MB, assinatura PNG/JPEG,
  decodificação completa e no mínimo 48×48. Um HTML de erro, por exemplo, é recusado.
- **Gravação segura:** `capas/<TitleID>.jpg`, gravado em arquivo temporário e renomeado
  depois. Sem internet, as capas já salvas continuam aparecendo.
- **Respeito ao serviço:** uma requisição por vez, só para os seus títulos, só quando falta
  capa. Falhas não se repetem na mesma sessão. Pode ser desligado em Configurações → Jogos.
  O site não tem página de termos de uso ou de API; os endpoints são os mesmos que o Aurora
  usa. Só imagens e metadados são baixados, nunca jogos, executáveis ou conteúdo com DRM.
- **Exibição:** a capa completa do XboxUnity é verso + lombada + frente. O Neutro X recorta a
  frente (lado direito, proporção 0,705) e a reduz para 256×360 numa thread separada. Ficam
  no máximo 40 capas na memória (as menos usadas saem).
- **Importação manual:** copie imagens para `capas_importar/` com o nome do Title ID
  (`4D5307E6.jpg` ou `.png`) e use Meus Jogos → Y → Importar capas. Arquivos inválidos são
  recusados com o motivo no log.
- **GOD/XBLA sem rede:** a miniatura PNG do próprio pacote vira capa provisória.
- **Atualizar capa** (detalhes) baixa de novo, ignorando o cache.
- **PC → console:** `tools/xbox_install.py --covers <pasta>` envia as imagens para `capas_importar/`. Depois use **Importar capas**.

Decodificadores próprios, sem bibliotecas externas: PNG de 8 bits (inflate próprio) e JPEG
baseline. JPEG progressivo é recusado com o motivo. Ambos foram conferidos contra o Pillow
nas capas reais do XboxUnity: diferença média abaixo de 2 níveis de 255.

## Arquivos (todos na pasta do Neutro X)

| Arquivo | Conteúdo | Quando é gravado |
|---|---|---|
| `biblioteca.ini` | pastas, opções, ignorados e uma seção `[jogo]` por título (favorito, datas, capa) | só quando algo mudou, no máximo a cada 2 s, e ao sair; gravação atômica |
| `capas/` | capas baixadas, importadas e miniaturas | ao baixar/importar |
| `capas_importar/` | onde você coloca capas para importar | nunca pelo Neutro X |

Apagar o `biblioteca.ini` recomeça a biblioteca. **Nenhum jogo, save ou perfil é apagado ou
alterado pelo Neutro X.**
