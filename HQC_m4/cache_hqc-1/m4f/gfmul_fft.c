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

#define bmul2_4096_to_4096_mul bmul2_2048_to_4096_mul
#define bmul2_8192_to_8192_mul bmul2_4096_to_8192_mul

/* Unified public-key cache instance (type/extern in pkcache.h). The FFT-form
 * components (h_fft, h_low, s_fft) are filled by pkcache_fill_fft() below; the
 * hpk component is filled by hash_h() in symmetric.c. */
pkcache_t pkcache;

/* Complete the unified pk cache entry by filling the FFT-form components
 * (h_fft, h_low, s_fft) for pkcache.pk, then raise valid.
 *
 * hash_h() is the single decision point: it has already set pkcache.pk to the
 * current public key and (on a miss) cleared valid. So here we only test valid
 * -- no second pk comparison: valid == 1 means the entry is already present for
 * pkcache.pk and the FFT forms are used as-is; valid == 0 means we (re)build
 * them from pkcache.pk and raise valid.
 *
 * What is cached for HQC-1's CRT multiply (see CRT_fft.c / pkcache.h):
 *   h_fft : bmul2_4096_to_4096-prepared form of h (1024 u32), FFT part.
 *   h_low : low LOW_LEN = 81 u32 of the raw h polynomial, residue part
 *           (the exact operand gf2x_mul_3072's b96 buffer consumes).
 *   s_fft : bmul2_4096_to_4096-prepared form of s.
 * s's raw low part is read from the public key at the call site (s lives at
 * pk + SEED_BYTES); the secret operand r2 / y is never cached.
 *
 * bmul2_4096_to_4096_prepare() reads a full 4096-byte (1024 u32) operand, so h
 * and s are staged into zero-padded 1024-u32 buffers first -- exactly what
 * CRT_fft_HQC1 does with its A_FAFFT/B_FAFFT staging.
 *
 * REQUIRES: hash_h() ran first for this same pk (true in kem.c: both encap and
 * decap call hash_h() before hqc_pke_encrypt()). Does NOT re-verify the pk. */
void pkcache_fill_fft(void) {
	if (pkcache.valid) return;  /* entry already complete for pkcache.pk */

	/* h: regenerate from the seed (first SEED_BYTES of pk). */
	shake256_xof_ctx ek_xof_ctx = {0};
	uint64_t h[VEC_N_SIZE_64] = {0};
	uint32_t stage[32768 / 32] = {0};   /* 4096-byte staging buffer for prepare() */

	xof_init(&ek_xof_ctx, pkcache.pk, SEED_BYTES);
	vect_set_random(&ek_xof_ctx, h);

	/* residue part: low 81 u32 of raw h (gf2x_mul_3072's b96 reads exactly these). */
	memcpy(pkcache.h_low, h, sizeof(uint32_t) * PKC_HLOW_U32);

	/* FFT part of h: zero-pad h to 1024 u32, then bmul2-prepare. */
	memset(stage, 0, sizeof(stage));
	memcpy(stage, h, VEC_N_SIZE_32 * 4);
	bmul2_4096_to_4096_prepare(pkcache.h_fft, (const uint8_t *)stage);

	/* FFT part of s: s = pk + SEED_BYTES (VEC_N_SIZE_BYTES bytes), zero-padded. */
	memset(stage, 0, sizeof(stage));
	memcpy(stage, pkcache.pk + SEED_BYTES, VEC_N_SIZE_BYTES);
	bmul2_4096_to_4096_prepare(pkcache.s_fft, (const uint8_t *)stage);

	pkcache.valid = 1;
}

/* Chen truncated forward basis conversion: processes 18432 bits (16384+2048)
 * instead of the full 32768. Valid because the HQC-1 input (17669 bits) has a
 * zero upper half, so bc_1_32768 would produce zero novel coeffs there anyway. */
extern void bc_1_16384_plus_2048(uint32_t *poly);

void bmul2_2048_to_4096_prepare(uint32_t * a_out, const uint8_t * a_in)
{
	uint32_t a0[16384 / 32];  // 16384 bits

	memcpy(a0, a_in, 2048);
	bc_1_16384(a0);

	uint32_t temp[16];
	for (int i = 0; i < ((32768 / 32) / 32); i++) {
		for (int j = 0; j < 16; j++) temp[j] = a0[((32768 / 32) / 32)*j + i];
				encode_trunc_fft(a_out + i * 32, temp);
	}

	btfy_32768(a_out);
}


//a_in, a_out: 4096 byte = 32768 bit
void bmul2_4096_to_4096_prepare(uint32_t * a_out, const uint8_t * a_in)
{
	uint32_t a0[32768 / 32] = { 0 };  // 32768 bits (upper half stays zero for truncated bc)

	memcpy(a0, a_in, 32768 / 8);
	bc_1_16384_plus_2048(a0);

	uint32_t temp[32];
	for (int i = 0; i < ((32768 / 32) / 32); i++) {
		for (int j = 0; j < 32; j++) temp[j] = a0[((32768 / 32) / 32)*j + i];
		encode_full(a_out + i * 32, temp);
		//encode_trunc_fft(a_out + i * 32, temp);
	}
	btfy_32768(a_out);
}


void bmul2_2048_to_4096_mul(uint8_t * c, const uint32_t * a, const uint32_t * b)
{
	uint32_t a0[32768 / 32];
	uint32_t * c32 = (uint32_t*)c;

	for (int i = 0; i < (32768 / 32); i += 32) { gf232v_mul(a0 + i, a + i, b + i); }

	ibtfy_32768(a0);

	uint32_t temp[32];
	for (int i = 0; i < (32768 / 32); i += 32) {
				decode_trunc_fft(temp, a0 + i);
		for (int j = 0; j < 32; j++) c32[((32768 / 32) / 32)*j + i / 32] = temp[j];
	}

	ibc_1_32768(c32);

}


void bmul2_2048_to_4096(uint8_t * c, const uint8_t * a, const uint8_t * b)
{
	uint32_t a_u32[32768 / 32];
	uint32_t b_u32[32768 / 32];

	bmul2_2048_to_4096_prepare(a_u32, a);
	bmul2_2048_to_4096_prepare(b_u32, b);

	bmul2_2048_to_4096_mul(c, a_u32, b_u32);
}

void bmul2_4096_to_4096(uint8_t * c, const uint8_t * a, const uint8_t * b)
{
	uint32_t a_u32[32768 / 32];
	uint32_t b_u32[32768 / 32];

	bmul2_4096_to_4096_prepare(a_u32, a);
	bmul2_4096_to_4096_prepare(b_u32, b);

	bmul2_4096_to_4096_mul(c, a_u32, b_u32);
}

void bmul2_4096_to_8192_prepare(uint32_t * a_out, const uint8_t * a_in)
{
	uint32_t a0[32768 / 32] = { 0 };
	memcpy(a0, a_in, 2216);
	bc_1_32768(a0);

	uint32_t temp[16];
	for (int i = 0; i < ((65536 / 32) / 32); i++) {
		for (int j = 0; j < 16; j++) temp[j] = a0[((65536 / 32) / 32)*j + i];
		encode_trunc_fft(a_out + i * 32, temp);
	}
	btfy_65536(a_out);
}

//a_in, a_out: 8192 byte = 65536 bit
void bmul2_8192_to_8192_prepare(uint32_t * a_out, const uint8_t * a_in)
{
	uint32_t a0[65536 / 32];  // 65536 bits

	memcpy(a0, a_in, 65536 / 8);
	bc_1_65536(a0);

	uint32_t temp[32];
	for (int i = 0; i < ((65536 / 32) / 32); i++) {
		for (int j = 0; j < 32; j++) temp[j] = a0[((65536 / 32) / 32)*j + i];
		encode_full(a_out + i * 32, temp);
		//encode_trunc_fft(a_out + i * 32, temp);
	}
	btfy_65536(a_out);
}


void bmul2_4096_to_8192_mul(uint8_t * c, const uint32_t * a, const uint32_t * b)
{
	uint32_t a0[65536 / 32];
	uint32_t * c32 = (uint32_t*)c;

	for (int i = 0; i < (65536 / 32); i += 32) { gf232v_mul(a0 + i, a + i, b + i); }

	ibtfy_65536(a0);

	uint32_t temp[32];
	for (int i = 0; i < (65536 / 32); i += 32) {
		decode_trunc_fft(temp, a0 + i);
		for (int j = 0; j < 32; j++) c32[((65536 / 32) / 32)*j + i / 32] = temp[j];
	}



	ibc_1_65536(c32);
}


void bmul2_4096_to_8192(uint8_t * c, const uint8_t * a, const uint8_t * b)
{
	uint32_t a_u32[65536 / 32];
	uint32_t b_u32[65536 / 32];

	bmul2_4096_to_8192_prepare(a_u32, a);
	bmul2_4096_to_8192_prepare(b_u32, b);

	bmul2_4096_to_8192_mul(c, a_u32, b_u32);
}


void bmul2_8192_to_8192(uint8_t * c, const uint8_t * a, const uint8_t * b)
{
	uint32_t a_u32[65536 / 32];
	uint32_t b_u32[65536 / 32];

	bmul2_8192_to_8192_prepare(a_u32, a);
	bmul2_8192_to_8192_prepare(b_u32, b);

	bmul2_8192_to_8192_mul(c, a_u32, b_u32);
}
