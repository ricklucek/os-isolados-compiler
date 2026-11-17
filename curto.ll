; LLVM IR gerado pelo compilador Macaronica
; IR portavel: sem target triple e sem target datalayout

declare void @zl_imprime_int(i32) nounwind
declare void @zl_imprime_bool(i32) nounwind
declare void @zl_imprime_flut(float) nounwind
declare void @zl_imprime_duplo(double) nounwind
declare void @zl_imprime_texto(ptr) nounwind
declare void @zl_imprime_nl() nounwind
declare i32 @zl_le_int() nounwind
declare void @zl_erro_execucao(i32, i32) nounwind

define i32 @zl_principal() nounwind {
entry:
  %a.addr = alloca i1 ; linha 3
  %b.addr = alloca i1 ; linha 4
  %c.addr = alloca i1 ; linha 5
  store i1 false, ptr %a.addr ; linha 7
  store i1 true, ptr %b.addr ; linha 8
  %t0 = load i1, ptr %a.addr
  br i1 %t0, label %and.rhs.0, label %and.end.1
and.rhs.0:
  %t1 = load i1, ptr %b.addr
  br label %and.end.1
and.end.1:
  %t2 = phi i1 [ false, %entry ], [ %t1, %and.rhs.0 ]
  store i1 %t2, ptr %c.addr ; linha 10
  %t3 = load i1, ptr %c.addr
  %t4 = zext i1 %t3 to i32
  call void @zl_imprime_bool(i32 %t4)
  call void @zl_imprime_nl() ; linha 11
  %t5 = load i1, ptr %b.addr
  br i1 %t5, label %or.end.3, label %or.rhs.2
or.rhs.2:
  %t6 = load i1, ptr %a.addr
  br label %or.end.3
or.end.3:
  %t7 = phi i1 [ true, %and.end.1 ], [ %t6, %or.rhs.2 ]
  store i1 %t7, ptr %c.addr ; linha 13
  %t8 = load i1, ptr %c.addr
  %t9 = zext i1 %t8 to i32
  call void @zl_imprime_bool(i32 %t9)
  call void @zl_imprime_nl() ; linha 14
  ret i32 0 ; linha 16
}

