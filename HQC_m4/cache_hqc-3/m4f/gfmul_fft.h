#ifndef _GFMUL_FFT_H_
#define _GFMUL_FFT_H_

#include <stdint.h>


void bmul2_2048_to_4096_prepare(uint32_t * a_out, const uint8_t * a_in);
void bmul2_2048_to_4096_mul(uint8_t * c, const uint32_t * a, const uint32_t * b);
void bmul2_2048_to_4096(uint8_t * c, const uint8_t * a, const uint8_t * b);
void bmul2_4096_to_8192_prepare(uint32_t * a_out, const uint8_t * a_in);
void bmul2_4096_to_8192_mul(uint8_t * c, const uint32_t * a, const uint32_t * b);
void bmul2_4096_to_8192(uint8_t * c, const uint8_t * a, const uint8_t * b);

void bmul2_4096_to_4096_prepare(uint32_t * a_out, const uint8_t * a_in);
void bmul2_4096_to_4096(uint8_t * c, const uint8_t * a, const uint8_t * b);

void bmul2_8192_to_8192_prepare(uint32_t * a_out, const uint8_t * a_in);
void bmul2_8192_to_8192(uint8_t * c, const uint8_t * a, const uint8_t * b);

/* Complete the unified pk cache entry: fill h_fft/h_low/s_fft for pkcache.pk
 * and raise valid. Assumes hash_h() already set pkcache.pk for the current pk
 * (see pkcache.h); takes no argument and does not re-check the public key. */
void pkcache_fill_fft(void);

#endif
