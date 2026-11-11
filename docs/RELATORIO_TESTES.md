# Relatório de testes — 0.1.0-beta

Executado em 11/11/2026 · Windows 11 · MSVC 2022 (Release, x64) · plataforma *headless*.
Comando: `build/Release/neutrox_tests.exe build/test_output`

**Resultado: 483 verificações aprovadas, 0 falharam.**

| # | Teste obrigatório | Como foi verificado | Resultado |
|---|---|---|---|
| 1 | Inicialização sem travamento | `App::init` + abertura → menu; também com configuração corrompida | ✅ |
| 2 | Funcionamento do controle | Bordas, repetição (6–8 eventos/s segurando), analógico com histerese, sem rajada após quadro longo | ✅ |
| 3 | Seleção das cinco interfaces | Pelo menu (setas + A), cada uma carrega e vira a tela ativa | ✅ |
| 4 | Navegação entre seções | LB/RB em cada interface | ✅ |
| 5 | Troca repetida | 100 trocas seguidas; pilha sempre com 1 tela | ✅ |
| 6 | Uso de memória | Heap vivo antes/depois das 100 trocas: **diferença 0 bytes**; texturas voltam a 11 (8,2 MB) | ✅ |
| 7 | Taxa de quadros | Tempo de CPU por quadro (tabela abaixo); 720p < 16,7 ms em todas as telas | ✅ (PC) |
| 8 | Integridade das configurações | Ida e volta, valores inválidos, arquivo binário, gravação atômica, alteração pela interface | ✅ |
| 9 | Recuperação de erros | Dashboard que falha e que lança exceção → tela de erro → menu → outra interface abre | ✅ |
| 10 | Encerramento seguro | Diálogo com "Não" por padrão; após sair, 0 texturas restantes | ✅ |
| — | Controle desconectado | 2 s desconectado dentro de um dashboard, aviso exibido, sem travar; reconexão | ✅ |
| — | Sons por versão | Tema muda com o dashboard; menu START usa sons do Neutro X; WAV/XMA/mapeamento; ausente = mudo | ✅ |
| — | Resoluções | 1280×720, 1920×1080 (escala 1,5) e 640×480 (barras) | ✅ |

## Desempenho (CPU, renderizador em software, PC de desenvolvimento)

| Tela | 1280×720 | 1920×1080 |
|---|---:|---:|
| Menu principal | 5,2 ms | 13,8 ms |
| Blades | 1,3 ms | 4,0 ms |
| NXE | 1,4 ms | 3,2 ms |
| Kinect | 1,3 ms | 3,0 ms |
| Metro V1 | 1,2 ms | 3,7 ms |
| Metro V2 | 1,2 ms | 4,0 ms |

Valores variam entre execuções. **Não representam o Xbox 360**, cujo processador é
bem mais lento para esse tipo de trabalho.

## Teste da prévia Windows real
O executável `NeutroX.exe` aceita `--frames N --screenshot arquivo.png --mute` para
rodar sozinho e sair; veja a seção da release.

## Não testado
- **Hardware Xbox 360 / Aurora / RGH**: não há executável para o console ainda.
- Controle físico do Xbox 360 no PC: o caminho XInput está implementado, mas não foi
  exercitado nesta sessão (os testes usam entrada simulada).
- Áudio audível: o mixer é testado numericamente; a saída waveOut não foi ouvida aqui.
