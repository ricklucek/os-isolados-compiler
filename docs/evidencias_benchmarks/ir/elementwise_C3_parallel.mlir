module {
  func.func @zl_mlir_kernel_l7_c7(%arg0: memref<1000000xf32>, %arg1: memref<1000000xf32>, %arg2: memref<1000000xf32>) {
    affine.parallel (%arg3) = (0) to (1000000) {
      %0 = affine.load %arg0[%arg3] : memref<1000000xf32>
      %1 = affine.load %arg1[%arg3] : memref<1000000xf32>
      %2 = arith.addf %0, %1 : f32
      affine.store %2, %arg2[%arg3] : memref<1000000xf32>
    }
    return
  }
}

