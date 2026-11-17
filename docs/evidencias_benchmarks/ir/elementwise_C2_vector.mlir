module {
  func.func @zl_mlir_kernel_l7_c7(%arg0: memref<1000000xf32>, %arg1: memref<1000000xf32>, %arg2: memref<1000000xf32>) {
    %cst = arith.constant 0.000000e+00 : f32
    affine.for %arg3 = 0 to 1000000 step 8 {
      %0 = vector.transfer_read %arg0[%arg3], %cst : memref<1000000xf32>, vector<8xf32>
      %1 = vector.transfer_read %arg1[%arg3], %cst : memref<1000000xf32>, vector<8xf32>
      %2 = arith.addf %0, %1 : vector<8xf32>
      vector.transfer_write %2, %arg2[%arg3] : vector<8xf32>, memref<1000000xf32>
    }
    return
  }
}

