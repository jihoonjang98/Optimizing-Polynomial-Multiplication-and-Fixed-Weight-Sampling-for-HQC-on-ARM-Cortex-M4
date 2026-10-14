#ifndef PKCACHE_H
#define PKCACHE_H

#include <stdint.h>
#include "parameters.h"

/* Unified cache of the public key's derived forms. `pk` holds the public key
 * the cache currently describes (the full key, seed || s), and the single
 * `valid` flag describes that `pk`:
 *   valid == 1 : hpk, h_fft, h_low and s_fft are all populated for `pk`.
 *   valid == 0 : the entry is incomplete -- either empty, or pk + hpk are set
 *                but the FFT forms (h_fft/h_low/s_fft) are still pending.
 * hash_h() aligns `pk` to the public key of the current operation;
 * once it has, the cached h_fft/h_low/s_fft are used directly whenever
 * valid == 1, with no further pk comparison.
 *
 * Only public data is stored, so there is no secret-dependent timing.
 *
 * HQC-3 (Jang framework) multiplies a * b (mod x^n - 1) with a CRT split:
 *   - FFT part  : bmul2_8192_to_8192, whose prepared operand is
 *                 65536/32 = 2048 u32 words (the btfy_65536 transform size).
 *                 h_fft / s_fft hold that prepared form of h / s.
 *   - residue   : radix16_193(rx, A, B), which reads the low LOW_LEN = 193 u32
 *                 words of each raw operand. h_low holds those low 193 words of
 *                 the raw h polynomial (the exact operand radix16_193 consumes).
 * For s the raw low part is read straight from the public key at the call site
 * (s lives at pk + SEED_BYTES), so no s_low is cached -- only s_fft. The secret
 * operand r2 (encrypt) / y (keygen) is never cached. */
#define PKC_FAFFT_OUT_U32 (65536 / 32)  /* 2048: bmul2_8192_to_8192 prepared size */
#define PKC_HLOW_U32      193           /* LOW_LEN: words radix16_193 reads */

typedef struct {
    uint8_t  pk[PUBLIC_KEY_BYTES];       /* key: full public key (seed || s) */
    uint8_t  hpk[32];                    /* H(pk) */
    uint32_t h_fft[PKC_FAFFT_OUT_U32];   /* bmul2_8192_to_8192-prepared form of h */
    uint32_t s_fft[PKC_FAFFT_OUT_U32];   /* bmul2_8192_to_8192-prepared form of s */
    uint32_t h_low[PKC_HLOW_U32];        /* low 193 u32 of raw h (CRT residue part) */
    int      valid;
} pkcache_t;

extern pkcache_t pkcache;  /* the single instance is defined in gfmul_fft.c */

#endif /* PKCACHE_H */
