/* ============================================================
 * vect_scatter4_asm : 4-way predicated constant-time support scatter
 *
 *   void vect_scatter4_asm(uint64_t *v, const uint32_t *tab,
 *                          uint32_t weight, uint32_t n4);
 *
 *   tab : table packed as {idx, bit_lo, bit_hi} per support element
 *         (3 uint32 per element, weight elements in total)
 *   n4  : number of output words to process here (multiple of 4)
 *
 * ------------------------------------------------------------
 * C equivalent of this function (the if branches become predicated orreq in asm):
 *
 *   void vect_scatter4(uint64_t *v, const uint32_t *tab,
 *                      uint32_t weight, uint32_t n4) {
 *       for (uint32_t i0 = 0; i0 < n4; i0 += 4) {       // outer: 4 words at a time
 *           uint32_t a0l=0,a0h=0, a1l=0,a1h=0,          // 8 accumulators
 *                    a2l=0,a2h=0, a3l=0,a3h=0;          //  (4 words x lo/hi)
 *           const uint32_t *p = tab;                    // rewind the table pointer
 *           for (uint32_t j = 0; j < weight; j++) {     // inner: support elements
 *               uint32_t idx    = *p++;
 *               uint32_t bit_lo = *p++;
 *               uint32_t bit_hi = *p++;
 *               int32_t  d = (int32_t)(i0 - idx);
 *               if (d == 0)  { a0l|=bit_lo; a0h|=bit_hi; }  // OR only into the matching word
 *               if (d == -1) { a1l|=bit_lo; a1h|=bit_hi; }
 *               if (d == -2) { a2l|=bit_lo; a2h|=bit_hi; }
 *               if (d == -3) { a3l|=bit_lo; a3h|=bit_hi; }
 *           }
 *           v[i0]   |= ((uint64_t)a0h<<32)|a0l;          // writeback
 *           v[i0+1] |= ((uint64_t)a1h<<32)|a1l;
 *           v[i0+2] |= ((uint64_t)a2h<<32)|a2l;
 *           v[i0+3] |= ((uint64_t)a3h<<32)|a3l;
 *       }
 *   }
 * ------------------------------------------------------------
 *
 * Operation: output words are processed in groups of four (i0..i0+3). For each
 *   element one subs/cmn chain predicates the OR into the matching word only:
 *   one compare per word, and the three table loads (idx, bit_lo, bit_hi) are
 *   amortized over four words (0.75 loads/word). Streaming through a single
 *   packed pointer frees the registers that inline asm could not claim at -O3.
 *
 * Constant time: control flow depends only on weight and n4 (public values).
 *   Every conditional OR is a predicated orreq, so there is no branch on the
 *   secret idx, and the table reads and v writes follow a sequential,
 *   data-independent pattern. The running time is therefore independent of
 *   the support values.
 *
 * Register map (inner loop):
 *   r3  = i0 (current word index)        r2  = wptr_end (tab + 12*weight)
 *   r12 = wptr (streaming table pointer, p in the C code)
 *   r4..r11 = 8 accumulators (word k -> lo=r4+2k, hi=r5+2k)
 *   r0,r1,lr = scratch (d, bit_lo, bit_hi)
 *   stack: [sp]=v base  [sp+4]=tab base  [sp+8]=n4   (keeps registers free in the inner loop)
 *
 * Assumes weight >= 1 (HQC: weight >= 66).
 * ============================================================ */
    .syntax unified
    .thumb
    .text
    .align  2
    .global vect_scatter4_asm
    .thumb_func
    .type   vect_scatter4_asm, %function
vect_scatter4_asm:
    @ ---- prologue: save callee-saved registers, keep the arguments on the stack ----
    push    {r4-r11, lr}            @ save r4-r11 (used as accumulators) and the return address
    sub     sp, sp, #12            @ 3 local slots (v, tab, n4)
    stmia   sp, {r0, r1, r3}        @ [sp]=v  [sp+4]=tab  [sp+8]=n4

    @ p_end = tab + 3*weight  (one element = 3 uint32 = 12 bytes)
    add     r2, r2, r2, lsl #1      @ r2 = weight + weight*2 = weight*3
    add     r2, r1, r2, lsl #2      @ r2 = tab + (weight*3)*4 = wptr_end
    movs    r3, #0                  @ i0 = 0   (= C: for(i0=0; ...))

.Lgroup:                            @ === outer loop head: start of a group (4 words) ===
    ldr     r12, [sp, #4]           @ p = tab            (rewind the pointer)
    movs    r4,  #0                 @ a0l = 0
    movs    r5,  #0                 @ a0h = 0
    movs    r6,  #0                 @ a1l = 0
    movs    r7,  #0                 @ a1h = 0
    movs    r8,  #0                 @ a2l = 0
    movs    r9,  #0                 @ a2h = 0
    movs    r10, #0                 @ a3l = 0
    movs    r11, #0                 @ a3h = 0

.Lelem:                             @ === inner loop: one support element ===
    ldmia   r12!, {r0, r1, lr}      @ idx=*p++; bit_lo=*p++; bit_hi=*p++;  (p+=12)
    subs    r0,  r3, r0            @ d = i0 - idx ;  Z set if d==0  -> word0 matches
    itt     eq                     @  if (d == 0) {
    orreq   r4,  r4,  r1           @      a0l |= bit_lo
    orreq   r5,  r5,  lr           @      a0h |= bit_hi    }   (predicated, no branch)
    cmn     r0,  #1               @ d + 1 == 0 ?  -> d == -1 -> idx == i0+1
    itt     eq                     @  if (d == -1) {
    orreq   r6,  r6,  r1           @      a1l |= bit_lo
    orreq   r7,  r7,  lr           @      a1h |= bit_hi    }
    cmn     r0,  #2               @ d == -2 -> idx == i0+2
    itt     eq                     @  if (d == -2) {
    orreq   r8,  r8,  r1           @      a2l |= bit_lo
    orreq   r9,  r9,  lr           @      a2h |= bit_hi    }
    cmn     r0,  #3               @ d == -3 -> idx == i0+3
    itt     eq                     @  if (d == -3) {
    orreq   r10, r10, r1           @      a3l |= bit_lo
    orreq   r11, r11, lr           @      a3h |= bit_hi    }
    cmp     r12, r2               @ p != p_end ?      (= C: loop condition j<weight)
    bne     .Lelem                @  if not, next element

    @ ---- writeback: v[i0+k] |= (a*h<<32)|a*l   (read-modify-write) ----
    ldr     r0, [sp, #0]           @ v base
    add     r0, r0, r3, lsl #3     @ r0 = &v[i0]   (word i0 * 8 bytes)
    ldmia   r0, {r1, lr}           @ lo = low half of v[i0], hi = high half
    orr     r1, r1, r4             @ lo |= a0l
    orr     lr, lr, r5             @ hi |= a0h
    stmia   r0!, {r1, lr}          @ write back v[i0] ; r0 -> &v[i0+1]
    ldmia   r0, {r1, lr}           @ v[i0+1]
    orr     r1, r1, r6
    orr     lr, lr, r7
    stmia   r0!, {r1, lr}          @ r0 -> &v[i0+2]
    ldmia   r0, {r1, lr}           @ v[i0+2]
    orr     r1, r1, r8
    orr     lr, lr, r9
    stmia   r0!, {r1, lr}          @ r0 -> &v[i0+3]
    ldmia   r0, {r1, lr}           @ v[i0+3]
    orr     r1, r1, r10
    orr     lr, lr, r11
    stmia   r0!, {r1, lr}

    @ ---- outer loop tail ----
    adds    r3, r3, #4             @ i0 += 4
    ldr     r0, [sp, #8]           @ n4
    cmp     r3, r0
    bcc     .Lgroup               @ next group if i0 < n4  (bcc = unsigned lower)

    @ ---- epilogue: release the stack, restore, return ----
    add     sp, sp, #12
    pop     {r4-r11, pc}           @ restore and return via pc
    .size   vect_scatter4_asm, .-vect_scatter4_asm
