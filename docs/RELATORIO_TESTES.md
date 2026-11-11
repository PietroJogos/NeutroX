# Relatório de testes — 0.4.0-beta

Executado em 11/11/2026 · Windows 11 · MSVC 2022 (Release, x64) · plataforma *headless*.
Comando: `build/Release/neutrox_tests.exe build/test_output`

**Resultado: 605 verificações aprovadas, 0 falharam.**

| # | Teste obrigatório | Como foi verificado | Resultado |
|---|---|---|---|
| 1 | Inicialização sem travamento | `App::init` + abertura → menu; também com configuração corrompida e com dashboard padrão | ✅ |
| 2 | Funcionamento do controle | Bordas, repetição (6–8 eventos/s segurando), analógico com histerese, sem rajada após quadro longo | ✅ |
| 3 | Seleção das cinco interfaces | Pelo menu (setas + A), cada uma carrega e vira a tela ativa | ✅ |
| 4 | Navegação entre seções | LB/RB em todas; no Blades: lâminas, lista, subpainéis, opções e limites | ✅ |
| 5 | Troca repetida | 100 trocas seguidas; pilha sempre com 1 tela | ✅ |
| 6 | Uso de memória | Heap vivo antes/depois das 100 trocas: **diferença 0 bytes**; texturas voltam a 11 (8,2 MB); pico 11,7 MB (só uma interface carregada por vez) | ✅ |
| 7 | Taxa de quadros | Tempo de CPU por quadro (tabelas abaixo) | ✅ (PC) |
| 8 | Integridade das configurações | Ida e volta, valores inválidos, arquivo binário, gravação atômica, alteração pela interface | ✅ |
| 9 | Recuperação de erros | Dashboard que falha e que lança exceção → tela de erro → menu → outra interface abre | ✅ |
| 10 | Encerramento seguro | Diálogo com "Não" por padrão; após sair, 0 texturas restantes | ✅ |
| — | Controle desconectado | 2 s desconectado dentro de um dashboard, aviso exibido, sem travar; reconexão | ✅ |
| — | Sons por versão | Tema muda com o dashboard; menu START usa sons do Neutro X; WAV/XMA/mapeamento; ausente = mudo | ✅ |
| — | Resoluções | 1280×720, 1920×1080 (escala 1,5) e 640×480 (barras) | ✅ |
| — | **Kinect (Fase 6)** | Grade com limites, passagem de página pelas bordas, Baixo sob linha curta, LB/RB, favoritos, página de botões (Jogar não executa, Voltar), cartão do sensor, atalhos do Início, 3 fundos sem vazar textura, **os 38 cartões das 6 seções abrem e fecham**, nenhum erro registrado | ✅ |
| — | **NXE (Fase 5)** | Canais e cartões com limites, esteira e troca de canal animadas, avatar acena (X) e não acena oculto, painel de jogo ("Jogar" não executa), editor de avatar, 4 fundos sem vazar textura, todos os cartões de todos os canais abrem/fecham, nenhum erro registrado | ✅ |
| — | **Blades (Fase 4)** | 4 lâminas, limites, animação de troca, 6 subpainéis por lâmina abrem/fecham, rolagem até o último jogo, opções (fundo grafite recria a textura sem vazar), atalhos para Configurações/Sobre, nenhum erro registrado | ✅ |

## Desempenho (CPU, renderizador em software, PC de desenvolvimento)

Média por quadro logo após abrir cada tela (inclui a animação de entrada):

| Tela | 1280×720 | 1920×1080 |
|---|---:|---:|
| Menu principal | 4,5 ms | 10,3 ms |
| Blades | 3,0 ms | 6,8 ms |
| NXE | 1,8 ms | 4,0 ms |
| Kinect | 1,6 ms | 3,7 ms |
| Metro V1 (provisório) | 1,2 ms | 2,8 ms |
| Metro V2 (provisório) | 1,2 ms | 3,0 ms |

Pior caso realista, ao vivo durante animações (720p):

| Animação | Mediana | p90 |
|---|---:|---:|
| Blades — troca de lâmina | 6,4 ms | 7,3 ms |
| NXE — troca de canal | 9,1 ms | 11,1 ms |
| NXE — esteira de cartões | 3,7 ms | 4,4 ms |
| Kinect — deslize de página | 4,4 ms | 5,5 ms |
| Kinect — destaque na grade | 3,3 ms | 3,8 ms |

Com a interface parada, o quadro vem do cache de cena (cópia de memória); no NXE só o
avatar é desenhado ao vivo por cima.

Os valores variam entre execuções (a máquina estava com outros programas abertos) e
**não representam o Xbox 360**.

## Prévia Windows real
`NeutroX.exe --frames 300 --mute --data <pasta> --screenshot final.png` com
`default_dashboard=kinect`: abriu direto no Kinect, 300 quadros, pior quadro 11,2 ms, saída limpa.

## Não testado
- **Hardware Xbox 360 / Aurora / RGH**: não há executável para o console ainda.
- Controle físico do Xbox 360 no PC: o caminho XInput está implementado, mas não foi
  exercitado nesta sessão (os testes usam entrada simulada).
- Áudio audível: o mixer é testado numericamente; a saída waveOut não foi ouvida aqui.
