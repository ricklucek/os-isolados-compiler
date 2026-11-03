# Geração LLVM IR — Checkpoint 9

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
- `respost`.

Ainda não suportado pelo codegen:

- `cond` e `casocontrario`;
- `durante` e `repete`;
- funções de usuário e recursão;
- vetores;
- registros;
- `^` e `raiz()`;
- curto-circuito de `E` e `OU`.

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
