// Implemented by Ming-Shing Chen, Tung Chou and Markus Krausz.
// 
// Modification :  Myeonghoon Lee and Jihoon Jang
//
// public domain

#include "parameters.h"
#include <stddef.h>
#include <stdint.h>
#include "encode.h"
#include "btfy_ffft.h"
#include "bitconversion.h"
#include "gfmul_fft.h"
#include "symmetric.h"
#include "vector.h"
#include "pkcache.h"

#include <string.h>

//new code block
void bmul2_8192_to_16384_prepare(uint32_t * a_out, const uint8_t * a_in)
{
	uint32_t a0[65536 / 32] = { 0 };
	memcpy(a0, a_in, VEC_N_SIZE_64 * 8);
	bc_1_65536(a0);



	uint32_t temp[16];
	for (int i = 0; i < ((131072 / 32) / 32); i++) {
		for (int j = 0; j < 16; j++) temp[j] = a0[((131072 / 32) / 32)*j + i];
		encode_trunc_fft(a_out + i * 32, temp);
	}
	btfy_131072(a_out);
}

/* Unified public-key cache instance (type/extern in pkcache.h). The prepared
 * forms (h_prep, s_prep) are filled by pkcache_fill_fft() below; the hpk
 * component is filled by hash_h() in symmetric.c. */
pkcache_t pkcache;

/* Complete the unified pk cache entry by filling the prepared forms
 * (h_prep, s_prep) for pkcache.pk, then raise valid.
 *
 * hash_h() is the single decision point: it has already set pkcache.pk to the
 * current public key and (on a miss) cleared valid. So here we only test valid
 * -- no second pk comparison: valid == 1 means the entry is already present for
 * pkcache.pk and the prepared forms are used as-is; valid == 0 means we
 * (re)build them from pkcache.pk and raise valid.
 *
 * HQC-256 multiplies entirely in the FFT-prepared domain (no CRT), so only the
 * prepared forms of h and s are kept (no h_low, no raw s). r2 is never cached.
 *
 * REQUIRES: hash_h() ran first for this same pk (true in kem.c: both encap and
 * decap call hash_h() before hqc_pke_encrypt()). Does NOT re-verify the pk. */
void pkcache_fill_fft(void) {
	if (pkcache.valid) return;  /* entry already complete for pkcache.pk */
	/* h: regenerate from the seed (first SEED_BYTES of pk), then FFT-prepare. */
	shake256_xof_ctx ek_xof_ctx = {0};
	uint64_t h[VEC_N_SIZE_64] = {0};
	uint64_t s[VEC_N_SIZE_64] = {0};
	xof_init(&ek_xof_ctx, pkcache.pk, SEED_BYTES);
	vect_set_random(&ek_xof_ctx, h);
	bmul2_8192_to_16384_prepare(pkcache.h_prep, (const uint8_t *)h);
	/* s: copy out of pk into a zero-padded VEC_N_SIZE_64 buffer. prepare() reads
	 * the full VEC_N_SIZE_64*8 bytes, and in pk the bytes right after s are hpk
	 * (not zero padding), so reading s straight from pk would feed stray bytes
	 * into s_prep. */
	memcpy(s, pkcache.pk + SEED_BYTES, VEC_N_SIZE_BYTES);
	bmul2_8192_to_16384_prepare(pkcache.s_prep, (const uint8_t *)s);
	pkcache.valid = 1;
}

//new code block
void bmul2_8192_to_16384_mul(uint8_t * c, const uint32_t * a, const uint32_t * b)
{
	uint32_t a0[131072 / 32];
	uint32_t * c32 = (uint32_t*)c;

	for (int i = 0; i < (131072 / 32); i += 32) { gf232v_mul(a0 + i, a + i, b + i); }

	ibtfy_131072(a0);

	uint32_t temp[32];
	for (int i = 0; i < (131072 / 32); i += 32) {
		decode_trunc_fft(temp, a0 + i);
		for (int j = 0; j < 32; j++) c32[((131072 / 32) / 32)*j + i / 32] = temp[j];
	}


	ibc_1_131072(c32);

}

//new code block
void bmul2_8192_to_16384(uint8_t * c, const uint8_t * a, const uint8_t * b)
{
	uint32_t a_u32[131072 / 32];
	uint32_t b_u32[131072 / 32];

	bmul2_8192_to_16384_prepare(a_u32, a);
	bmul2_8192_to_16384_prepare(b_u32, b);

	bmul2_8192_to_16384_mul(c, a_u32, b_u32);
}
