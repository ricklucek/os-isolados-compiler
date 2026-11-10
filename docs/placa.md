# Execução em placa — Arduino Uno

## Placa escolhida

- Arduino Uno;
- microcontrolador ATmega328P;
- arquitetura AVR 8 bits;
- clock 16 MHz;
- 2048 B de SRAM;
- 32 KiB de flash física;
- limite de aplicação adotado: 32256 B, preservando o bootloader.

O LLVM IR fonte continua portável e não contém `target triple` nem `target datalayout`. O alvo é selecionado na etapa de construção:

```text
-mtriple=avr
-mcpu=atmega328p
```

## Ferramental de construção

Dentro do Docker:

- LLVM/llc 18;
- avr-gcc;
- avr-libc;
- binutils-avr;
- avr-size;
- avr-objcopy;
- avrdude (útil em hosts Linux/WSL com USB exposto).

O Docker é responsável por **compilar**. No Windows, a gravação é feita preferencialmente com Arduino CLI no host, para evitar problemas de passthrough da porta COM.

## Construção

Após alterar o Dockerfile deste checkpoint:

```powershell
docker compose build
```

Gerar o programa de laço+vetor:

```powershell
docker compose run --rm dev sh -lc "make placa PROG=tests/placa/01_laco_vetor.mcn"
```

Saídas:

```text
placa/01_laco_vetor.ll
placa/01_laco_vetor.bc
placa/01_laco_vetor.o
placa/01_laco_vetor.elf
placa/01_laco_vetor.hex
```

O comando também imprime uso de flash e RAM estática.

## Programas obrigatórios da placa

### 1. Laço + vetor

`tests/placa/01_laco_vetor.mcn`

Saída esperada na serial:

```text
15
```

### 2. Função recursiva

`tests/placa/02_recursao.mcn`

Saída:

```text
120
```

O fatorial usa profundidade 5, deliberadamente pequena para o Uno.

### 3. Erro de execução

`tests/placa/03_erro_execucao.mcn`

O divisor é uma variável zero, portanto o check é de runtime. A serial mostra:

```text
Erro de execucao Macaronica: codigo=2, linha=<linha>
```

Depois o runtime entra em estado fatal e o LED do pino 13 pisca o código de erro: dois pulsos, pausa, repetição.

### 4. E/S física

`tests/placa/04_pisca_led.mcn`

Usa:

```text
pinomodo(13, 1);
pinoescreve(13, 1);
espera(250);
pinoescreve(13, 0);
```

O LED embutido pisca seis vezes.

## Builtins físicos

| Macaronica | Runtime | Significado |
|---|---|---|
| `pinomodo(p,m)` | `zl_pino_modo` | 0 entrada, 1 saída, 2 input pull-up |
| `pinoescreve(p,v)` | `zl_pino_escreve` | LOW/HIGH |
| `pinole(p)` | `zl_pino_le` | retorna 0 ou 1 |
| `espera(ms)` | `zl_espera` | atraso em milissegundos |

Pinos 0–13 correspondem aos digitais do Uno. 14–19 correspondem a A0–A5 usados como digitais.

## Verificação de memória por alvo

```bash
./macaronica tests/placa/05_memoria_ok.mcn --alvo=uno
```

O vetor de 400 `inteira` consome 1600 B e passa no orçamento.

```bash
./macaronica tests/placa/06_memoria_excede.mcn --alvo=uno
```

O vetor de 1000 `inteira` exigiria 4000 B e é rejeitado semanticamente antes de gerar LLVM.

O orçamento conservador é 1792 B de 2048 B; 256 B ficam reservados para runtime e overhead de pilha. O limite teórico de um único vetor `inteira` em uma função vazia é, portanto, 448 elementos, antes de considerar outras variáveis.

## duplocarpado

O Uno não recebe rebaixamento silencioso de `duplocarpado` para `float`. O teste `07_duplocarpado_rejeitado.mcn` deve retornar erro semântico em `--alvo=uno`.

## Serial

O runtime usa UART 9600 baud, 8N1.

Depois de gravar um programa, abra o monitor a 9600 e pressione RESET na placa para repetir a execução.

## Gravação no Windows sem Arduino IDE

A compilação não exige Arduino IDE. Para gravar no Windows, recomendamos apenas **Arduino CLI**.

Após instalar o Arduino CLI oficial e colocá-lo no PATH:

```powershell
arduino-cli core update-index
arduino-cli core install arduino:avr
arduino-cli board list
```

Identifique a porta, por exemplo `COM3`.

Depois:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/grava_uno.ps1 -Hex placa/01_laco_vetor.hex -Port COM3
```

O script usa `arduino-cli upload --input-file`, portanto **não recompila** o programa: envia exatamente o `.hex` produzido pelo compilador do grupo.

Para a serial:

```powershell
arduino-cli monitor -p COM3 --config baudrate=9600
```

Pressione RESET no Uno após abrir o monitor para observar a saída desde o começo.

## Alternativa Linux/WSL

Se o dispositivo USB estiver exposto como `/dev/ttyACM0`:

```bash
make grava HEX=placa/01_laco_vetor.hex PORT=/dev/ttyACM0
```

Esse alvo usa `avrdude` diretamente.

## Artefatos finais

Após validar fisicamente os quatro programas, os `.hex`/ `.elf` escolhidos devem ser preservados na pasta `placa/` para a entrega final, junto com tamanho de flash/RAM e o link do vídeo de até 1 minuto.

## Limitações/dependências de alvo

- sem SIMD no ATmega328P;
- operações `inteira` de 32 bits são implementadas sobre CPU de 8 bits;
- `flut` é software;
- `duplocarpado` é rejeitado;
- profundidade de recursão não é provada estaticamente;
- textos do usuário ainda não recebem a otimização opcional de armazenamento em flash/PROGMEM.


## Compatibilidade LLVM AVR 18: índice vetorial dinâmico

Durante a validação física, `01_laco_vetor.mcn` produziu `0` na serial do Uno, embora o mesmo programa produzisse `15` no PC. A gravação, UART e laços escalares estavam corretos.

O problema foi reproduzido automaticamente com `simavr` e isolado com três casos:

- `saida(15)`: correto;
- laço escalar: correto;
- vetor com índices constantes: correto;
- vetor local com índice dinâmico: incorreto no lowering AVR do LLVM 18.

Tentativas de usar GEP dinâmico com índice `i32` e depois `i16` mantiveram a falha. A solução adotada é específica do alvo Uno: o bounds check continua sendo gerado em LLVM IR, mas o cálculo do endereço SRAM dinâmico é delegado a:

```text
zl_vetor_endereco(base, indice, tamanho_elemento)
```

implementado em `zl_runtime_avr.c` e compilado por `avr-gcc`.

No PC e nos demais caminhos portáveis, vetores continuam usando `getelementptr` normalmente. Assim, o workaround não altera a IR usada como evidência do PjBL 3 e fica explicitamente confinado à biblioteca/runtime do alvo AVR.

A suíte `test-uno` agora usa `simavr` para impedir regressões desse caso antes da gravação física.
