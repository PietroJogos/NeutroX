# Desempenho: travadas, instrumentação e otimizações (0.9.0-beta)

## O que foi encontrado

Testes automatizados percorrem cada eixo de cada interface até o fim e voltam ao começo
(inclusive dando a volta), medindo separadamente **lógica** e **desenho** de cada quadro.

- **A lógica nunca pesa:** o pior quadro de lógica, em todas as interfaces, é de 0,14 ms.
  Navegação, entrada do controle e transições não fazem operações pesadas.
- **O custo está no desenho.** O renderizador é em software (CPU) e, na 0.8.0, rodava numa
  única thread. Os retângulos arredondados calculavam uma raiz quadrada para cada pixel de
  toda a largura dos cantos, em todas as linhas: 5–6 ms por quadro no PC só nisso. As
  misturas com transparência faziam três multiplicações separadas por pixel. O Xenon
  executa esse tipo de código bem mais devagar que um PC, o que explica as travadas e as
  quedas sentidas no console.
- **Voltar ao começo não custa mais que ir ao fim** no PC: o percentil 95 da volta fica no
  mesmo patamar da ida (ver testes). No console, a travada relatada ao voltar coincidia
  com quadros de desenho pesados no limite de 60 Hz.
- Texturas não são recarregadas na navegação: cada interface cria suas texturas uma vez,
  ao abrir (fundo e cache de cena), e só as recria ao mudar uma opção de fundo. Fontes:
  os glifos ficam em cache, e o cache não é mais esvaziado no meio de um quadro.

## Otimizações

| Mudança | Efeito medido (PC, 1 thread) |
|---|---|
| Retângulo arredondado: a faixa sólida de cada linha é calculada de forma analítica; a distância (com raiz) só nos pixels da borda | 5,25 → 0,98 ms por quadro no Blades |
| Mistura com cor constante: vermelho e azul numa só operação de 32 bits, mesmo resultado exato | gradientes: 3,14 → 0,33 ms no NXE |
| **Rasterização paralela por faixas:** o quadro é gravado como uma lista de comandos e redesenhado em faixas horizontais, uma por thread. No console são 6 threads (a principal e uma em cada thread de hardware 1–5 do Xenon). Cada primitiva só escreve dentro do recorte, então o resultado é **idêntico pixel a pixel** ao modo de uma thread | NXE, troca de canal: 3,35 → 1,20 ms com 4 threads no PC (2,8×; 3,4–3,8× em outras medições) |

**Antes e depois** (PC, 1280×720, mediana de quadros ao vivo durante a animação):

| Cena | 0.8.0 | 0.9.0 (1 thread) |
|---|---:|---:|
| Blades, troca de lâmina | 9,82 ms | 3,55 ms |
| NXE, troca de canal | 11,69 ms | 3,59 ms |
| Kinect, deslize de página | 5,98 ms | 2,08 ms |
| Metro V1, troca de categoria | 3,27 ms | 1,08 ms |
| Metro V2, troca de categoria | 3,49 ms | 1,95 ms |
| Menu principal (média) | 8,31 ms | 2,95 ms |

O ganho das threads se soma a isso no console. **Os números do Xbox 360 só existem depois
do teste no hardware.** O objetivo é 60 FPS estáveis onde o Xenon permitir; sem a medição
real, nenhum valor de FPS no console é afirmado.

## Instrumentação (para achar as travadas no console)

- **Mostrar FPS** (Configurações, ou botão Back): FPS, ms do quadro e as partes — lógica,
  desenho e tela (envio + espera do vblank) —, mais a contagem de quadros lentos e as texturas.
- **Log** (`NeutroX.log`):
  - `Quadro lento em <tela>: X ms (atualizacao A, desenho D, apresentacao P) ao navegar`:
    cada quadro acima de 33 ms (no máximo uma linha por segundo);
  - ao sair, uma linha por tela com quadros, média, pior quadro, pior lógica, pior desenho
    e quadros lentos;
  - `Dashboard <x> carregado; texturas: N (KB)` mostra quanto cada interface cria ao abrir.

Com esse log dá para saber se a travada vem de lógica, de desenho, de carregamento ou da
apresentação, e em qual tela.
