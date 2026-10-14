#ifndef _GFMUL_FFT_H_
#define _GFMUL_FFT_H_

#include <stdint.h>

void bmul2_8192_to_16384_prepare(uint32_t * a_out, const uint8_t * a_in);
/* Complete the unified pk cache entry: fill h_prep/s_prep for pkcache.pk and
 * raise valid. Assumes hash_h() already set pkcache.pk for the current pk
 * (see pkcache.h); takes no argument and does not re-check the public key. */
void pkcache_fill_fft(void);
void bmul2_8192_to_16384_mul(uint8_t * c, const uint32_t * a, const uint32_t * b);
void bmul2_8192_to_16384(uint8_t * c, const uint8_t * a, const uint8_t * b);

#endif
