# Geração e lowering via MLIR — Checkpoint 15

## Estratégia

O projeto usa integração híbrida:

```text
AST anotada
├── código escalar -> codegen_llvm -> módulo LLVM do programa
└── DSL vetorial  -> codegen_mlir -> kernels MLIR -> LLVM IR dos kernels
```

Neste checkpoint é fechada a segunda linha até LLVM IR. A união dos dois módulos será a próxima etapa de integração.

## Dialetos emitidos

O gerador usa somente dialetos padrão:

- `func`;
- `arith`;
- `memref`;
- `affine` para operações elemento a elemento;
- `scf` para reduções/produto escalar.

### Elemento a elemento

Exemplo conceitual:

```mlir
func.func @zl_mlir_kernel_0_l7(
    %v_a: memref<4xi32>,
    %v_b: memref<4xf32>,
    %out: memref<4xf32>) {
  affine.for %i = 0 to 4 {
    %a = affine.load %v_a[%i] : memref<4xi32>
    %af = arith.sitofp %a : i32 to f32
    %b = affine.load %v_b[%i] : memref<4xf32>
    %r = arith.addf %af, %b : f32
    affine.store %r, %out[%i] : memref<4xf32>
  }
  return
}
```

### Reduções

`somavet`, `maxvet` e `prodesc` usam `scf.for` com `iter_args`, porque o acumulador é um valor transportado entre iterações.

## Pipeline versionado

`scripts/pipeline.sh` produz quatro representações:

1. `01_canonical.mlir`: affine/scf + canonicalize/cse;
2. `02_scf.mlir`: affine rebaixado para scf/memref;
3. `03_llvm.mlir`: dialeto LLVM;
4. `04_kernel.ll`: LLVM IR textual.

Isso fornece diretamente os três níveis de evidência pedidos no enunciado e uma saída final verificável por `llvm-as`.

## Limite deste checkpoint

O LLVM IR dos kernels usa a ABI de memref produzida pelo MLIR. Ainda não existe chamada automática do módulo escalar para esses símbolos. O próximo passo de integração deve gerar/adaptar a ponte e usar `llvm-link`, mantendo a separação híbrida.
