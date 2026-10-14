#ifndef CRT_FFT_H_
#define CRT_FFT_H_

#include <stdint.h>
void CRT_fft_HQC3(uint32_t *res, const uint32_t *A, const uint32_t *B);
void CRT_fft_HQC3_pair(uint32_t *res1, uint32_t *res2,
                       const uint32_t *A_common,
                       const uint32_t *B1, const uint32_t *B2);

/* Public-key cached variants: the public operand B (h or s) is supplied as its
 * bmul2-prepared FFT form (*_fft) plus its low LOW_LEN u32 (*_low). Only the
 * ephemeral operand A is prepared internally. Filled by pkcache_fill_fft(). */
void CRT_fft_HQC3_cached(uint32_t *res, const uint32_t *A,
                         const uint32_t *B_fft, const uint32_t *B_low);
void CRT_fft_HQC3_pair_cached(uint32_t *res1, uint32_t *res2,
                              const uint32_t *A,
                              const uint32_t *h_fft, const uint32_t *h_low,
                              const uint32_t *s_fft, const uint32_t *s_low);

void bit_shift_xor_193_output_asm(uint32_t *res, const uint32_t *t, const int32_t *voff, const uint32_t *cnt, uint32_t nblocks, const int32_t *tail_voff, uint32_t tail_cnt);

#endif