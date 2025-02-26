; NOTE: Assertions have been autogenerated by utils/update_test_checks.py
; RUN: opt -dse -S %s | FileCheck %s

@b = local_unnamed_addr global i32 0, align 4

; Reduced test case for PR47285.

; `store i32 9, ptr @b` in %interesting is not killed by `store i32 23, ptr @b`
; in %killer, because it is not overwritten before reaching the end of the
; function via %bb.2 -> %no.overwrite.exit.

define void @test(i1 %c.0, i1 %c.2, i1 %c.3, i1 %c.4, i1 %c.5, i1 %c.6) {
; CHECK-LABEL: @test(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    br label [[BB_2:%.*]]
; CHECK:       bb.2:
; CHECK-NEXT:    br i1 [[C_0:%.*]], label [[BB_3:%.*]], label [[NO_OVERWRITE_EXIT:%.*]]
; CHECK:       no.overwrite.exit:
; CHECK-NEXT:    ret void
; CHECK:       bb.3:
; CHECK-NEXT:    br i1 [[C_2:%.*]], label [[BB_4:%.*]], label [[BB_7:%.*]]
; CHECK:       bb.4:
; CHECK-NEXT:    br i1 [[C_4:%.*]], label [[BB_5:%.*]], label [[BB_6:%.*]]
; CHECK:       bb.5:
; CHECK-NEXT:    store i32 99, ptr @b, align 4
; CHECK-NEXT:    br i1 [[C_3:%.*]], label [[BB_5]], label [[BB_2]]
; CHECK:       bb.6:
; CHECK-NEXT:    store i32 91, ptr @b, align 4
; CHECK-NEXT:    br i1 [[C_5:%.*]], label [[SPLIT_CRIT_EDGE_2:%.*]], label [[BB_2]]
; CHECK:       split_crit_edge.2:
; CHECK-NEXT:    store i32 27, ptr @b, align 4
; CHECK-NEXT:    br label [[EXIT:%.*]]
; CHECK:       bb.7:
; CHECK-NEXT:    br i1 [[C_4]], label [[INTERESTING:%.*]], label [[BB_8:%.*]]
; CHECK:       interesting:
; CHECK-NEXT:    store i32 9, ptr @b, align 4
; CHECK-NEXT:    br i1 [[C_6:%.*]], label [[KILLER:%.*]], label [[BB_2]]
; CHECK:       killer:
; CHECK-NEXT:    store i32 23, ptr @b, align 4
; CHECK-NEXT:    ret void
; CHECK:       bb.8:
; CHECK-NEXT:    store i32 19, ptr @b, align 4
; CHECK-NEXT:    br i1 [[C_4]], label [[EXIT]], label [[BB_2]]
; CHECK:       exit:
; CHECK-NEXT:    ret void
;
entry:
  br label %bb.2

bb.2:                                             ; preds = %bb.8, %interesting, %bb.6, %bb.5, %entry
  br i1 %c.0, label %bb.3, label %no.overwrite.exit

no.overwrite.exit:                                ; preds = %bb.2
  ret void

bb.3:                                             ; preds = %bb.2
  br i1 %c.2, label %bb.4, label %bb.7

bb.4:                                             ; preds = %bb.3
  br i1 %c.4, label %bb.5, label %bb.6

bb.5:                                             ; preds = %bb.5, %bb.4
  store i32 99, ptr @b, align 4
  br i1 %c.3, label %bb.5, label %bb.2

bb.6:                                             ; preds = %bb.4
  store i32 91, ptr @b, align 4
  br i1 %c.5, label %split_crit_edge.2, label %bb.2

split_crit_edge.2:                                ; preds = %bb.6
  store i32 27, ptr @b, align 4
  br label %exit

bb.7:                                             ; preds = %bb.3
  br i1 %c.4, label %interesting, label %bb.8

interesting:                                      ; preds = %bb.7
  store i32 9, ptr @b, align 4
  br i1 %c.6, label %killer, label %bb.2

killer:                                           ; preds = %interesting
  store i32 23, ptr @b, align 4
  ret void

bb.8:                                             ; preds = %bb.7
  store i32 19, ptr @b, align 4
  br i1 %c.4, label %exit, label %bb.2

exit:                                             ; preds = %bb.8, %split_crit_edge.2
  ret void
}
