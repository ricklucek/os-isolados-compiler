# Geração LLVM IR — Checkpoints 9 e 10

## Escopo

Este checkpoint prova o primeiro caminho executável do compilador. Ele não tenta cobrir toda a linguagem de uma vez.

Suportado:

- `inteira principal() @ [ ... ]`, sem parâmetros;
- variáveis globais e locais escalares primitivas;
- `inteira`, `flut`, `duplocarpado`, `bool` e `palavra`;
- literais;
- atribuição a identificador;
- `+`, `-`, `*`, `/`, `//`;
- comparações numéricas e igualdade bool;
- `XOR`, `NÃO`, sinal unário;
- promoções `inteira -> flut -> duplocarpado`;
- `saida(expr)`;
- `entrada(x)` para `inteira`;
- `respost`;
- `cond`, `casocontrario cond` e `casocontrario`;
- `durante`;
- `repete`;
- `E` e `OU` com curto-circuito;
- escopos locais aninhados e sombreamento de variáveis escalares.

Ainda não suportado pelo codegen:

- funções de usuário e recursão;
- vetores;
- registros;
- `^` e `raiz()`.

Essas construções continuam válidas no front-end. Ao usar `-o`, o gerador informa que o lowering ainda não existe e a CLI remove a saída parcial.

## Exemplo mínimo

Fonte:

```text
inteira principal() @
[
    inteira a;
    inteira b;
    inteira resultado;
    a receba 6;
    b receba 7;
    resultado receba a * b;
    respost resultado;
]
```

Forma esperada da IR, de maneira simplificada:

```llvm
define i32 @zl_principal() nounwind {
entry:
  %a.addr = alloca i32
  %b.addr = alloca i32
  %resultado.addr = alloca i32
  store i32 6, ptr %a.addr
  store i32 7, ptr %b.addr
  %t0 = load i32, ptr %a.addr
  %t1 = load i32, ptr %b.addr
  %t2 = mul i32 %t0, %t1
  store i32 %t2, ptr %resultado.addr
  %t3 = load i32, ptr %resultado.addr
  ret i32 %t3
}
```

## Validação

```bash
./macaronica programa.mcn -o programa.ll
llvm-as-18 programa.ll -o programa.bc
opt-18 -passes=verify programa.ll -disable-output
lli-18 --entry-function=zl_principal programa.ll
```

Para execução com a biblioteca de plataforma:

```bash
clang-18 programa.ll runtime/zl_runtime_posix.c -Iruntime -o programa
./programa
```

## Runtime

`runtime/zl_runtime.h` é o contrato entre IR e plataforma. `zl_runtime_posix.c` é a primeira implementação concreta e fornece também o `main` que chama `zl_principal`.

Quando chegarmos ao Arduino Uno, a IR continuará chamando as mesmas funções `zl_*`; a implementação será substituída por `zl_runtime_avr.c`.


## Controle de fluxo — Checkpoint 10

Cada estrutura recebe rótulos únicos. Um `durante` assume a forma:

```llvm
br label %while.cond.N

while.cond.N:
  %cond = ...
  br i1 %cond, label %while.body.N, label %while.end.N

while.body.N:
  ...
  br label %while.cond.N

while.end.N:
```

`repete` acrescenta um bloco `for.step.N` entre corpo e condição.

### Curto-circuito

`E` e `OU` não são emitidos como simples `and/or`. O operando direito fica em um bloco próprio e só é alcançado quando necessário. O valor da expressão se encontra no bloco de merge por `phi i1`.

Para `a E b`, conceitualmente:

```llvm
br i1 %a, label %and.rhs.N, label %and.end.N

and.rhs.N:
  ; só aqui b é avaliado
  br label %and.end.N

and.end.N:
  %resultado = phi i1 [ false, %bloco_anterior ],
                       [ %b, %and.rhs.N ]
```

### Escopos e allocas

Todas as variáveis escalares, inclusive as declaradas em blocos internos, recebem `alloca` no `entry`, para permanecerem elegíveis ao `mem2reg`. A visibilidade lexical é controlada pelo gerador. Em caso de sombreamento, os nomes LLVM são únicos, por exemplo `%x.addr` e `%x.1.addr`.

## mem2reg e PHI

O gerador continua sem construir manualmente SSA para variáveis mutáveis. Execute:

```bash
make show-mem2reg FILE=tests/execucao/05_lacos.mcn
```

O arquivo `.llvm-before.ll` contém `alloca/load/store`; `.llvm-after.ll` mostra a promoção e os `phi i32` inseridos pelo LLVM nos cabeçalhos dos laços. O `phi i1` de curto-circuito é diferente: ele é produzido diretamente pelo gerador porque representa o valor de uma expressão que converge de dois caminhos de controle.
