#ifndef PKCACHE_H
#define PKCACHE_H

#include <stdint.h>
#include "parameters.h"

/* Unified cache of the public key's derived forms. `pk` holds the public key
 * the cache currently describes (the full key, seed || s), and the single
 * `valid` flag describes that `pk`:
 *   valid == 1 : hpk, h_prep and s_prep are all populated for `pk`.
 *   valid == 0 : the entry is incomplete -- either empty, or pk + hpk are set
 *                but the prepared forms (h_prep/s_prep) are still pending.
 * hash_h() aligns `pk` to the public key of the current operation;
 * once it has, the cached h_prep/s_prep are used directly whenever valid == 1,
 * with no further pk comparison.
 *
 * Only public data is stored, so there is no secret-dependent timing. HQC-256
 * multiplies entirely in the FFT-prepared domain (no CRT), so only the prepared
 * forms are kept -- no h_low and no raw-s low part are needed. */
#define PKC_PREP_U32 (131072 / 32)

typedef struct {
    uint8_t  pk[PUBLIC_KEY_BYTES];   /* key: full public key (seed || s) */
    uint8_t  hpk[32];                /* H(pk) */
    uint32_t h_prep[PKC_PREP_U32];   /* bmul2 FFT-prepared form of h */
    uint32_t s_prep[PKC_PREP_U32];   /* bmul2 FFT-prepared form of s */
    int      valid;
} pkcache_t;

extern pkcache_t pkcache;  /* the single instance is defined in gfmul_fft.c */

#endif /* PKCACHE_H */
