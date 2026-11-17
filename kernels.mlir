// MLIR gerado pela DSL vetorial Macaronica
// estrategia hibrida: apenas kernels vetoriais
module {

  // kernel 0: atribuicao vetorial da linha 7
  func.func @zl_mlir_kernel_0_l7(%v_a: memref<4xi32>, %v_b: memref<4xf32>, %out: memref<4xf32>) {
    affine.for %i = 0 to 4 {
      %v0 = affine.load %v_a[%i] : memref<4xi32>
      %v1 = affine.load %v_b[%i] : memref<4xf32>
      %v2 = arith.sitofp %v0 : i32 to f32
      %v3 = arith.addf %v2, %v1 : f32
      affine.store %v3, %out[%i] : memref<4xf32>
    }
    return
  }

  // kernel 1: atribuicao vetorial da linha 8
  func.func @zl_mlir_kernel_1_l8(%v_c: memref<4xf32>, %v_b: memref<4xf32>, %out: memref<4xf32>) {
    affine.for %i = 0 to 4 {
      %v0 = affine.load %v_c[%i] : memref<4xf32>
      %v1 = affine.load %v_b[%i] : memref<4xf32>
      %v2 = arith.subf %v0, %v1 : f32
      affine.store %v2, %out[%i] : memref<4xf32>
    }
    return
  }

  // kernel 2: atribuicao vetorial da linha 9
  func.func @zl_mlir_kernel_2_l9(%v_a: memref<4xi32>, %v_b: memref<4xf32>, %out: memref<4xf32>) {
    affine.for %i = 0 to 4 {
      %v0 = affine.load %v_a[%i] : memref<4xi32>
      %v1 = affine.load %v_b[%i] : memref<4xf32>
      %v2 = arith.sitofp %v0 : i32 to f32
      %v3 = arith.mulf %v2, %v1 : f32
      affine.store %v3, %out[%i] : memref<4xf32>
    }
    return
  }

}
