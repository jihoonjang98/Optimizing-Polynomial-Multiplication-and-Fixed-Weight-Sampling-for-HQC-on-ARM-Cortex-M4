#include <string.h>
#include "gfmul_fft.h"
#include "parameters.h"
#include "radix16.h"
#include "reduce.h"
#include "CRT_fft.h"
#include "outstat_index.h"

#define N_FAFFT 65536
#define N_FAFFT_32 2048

#define LOW_LEN 193
const uint32_t p_inv[LOW_LEN] = {
    0x4734f4db, 0x724c78d4, 0xed65ef78, 0x3a5c4643, 0x30a978ea, 0x1ea1e8c0, 0x850df915, 0xc3062d30,
    0xc679c19b, 0x5a024833, 0x13c34cdf, 0x0018f2fa, 0x7fbb4f61, 0x97e78a34, 0x147a9424, 0x6300aa02,
    0x4b8c2fea, 0xb279c9d3, 0xc4b23c93, 0x07cade36, 0x896b8938, 0x41895469, 0x48e8f5ef, 0xf4c5a8f3,
    0x5f3cf1c6, 0xbc5430b6, 0xd939bab7, 0x917374f9, 0x6b5686da, 0x8cab5a8c, 0x3a5d969a, 0x4890356f,
    0x07cd9528, 0x27454274, 0xc9c34915, 0xda46e16d, 0xab551ef6, 0x05a8962b, 0xc67801f2, 0xf092af5e,
    0xaedb0fb1, 0x32d606e4, 0xeed6a53c, 0x7b93d252, 0x8b52f992, 0xe5ebdc52, 0x3982d227, 0xc5057dd2,
    0x4b56a91e, 0xb6618f53, 0xc713e4cc, 0x072ae188, 0x747a5ddb, 0x19aac371, 0xf75d12e7, 0xe82ac0fc,
    0x3d1c426a, 0xcd81ed0e, 0x0fb46ca4, 0xb6b9c98d, 0x207fb6c1, 0xf285cb54, 0xb0409d37, 0x5a94007c,
    0x84a3dc78, 0x0d524cff, 0xbb5a3555, 0xb3019df3, 0x75500437, 0xe953c26e, 0x6737bb7f, 0xbe33a40a,
    0xf265979d, 0x436825c0, 0x63532e2a, 0x9bb2167d, 0x0d3d6aef, 0xd6618e20, 0x58569d1b, 0xc24663ca,
    0xab6ff46a, 0x7c00cdc3, 0x9c39ded2, 0x8fad82f7, 0x28d24730, 0x3f09ecad, 0xf5c5e2e0, 0x72da53fc,
    0xba18dac1, 0x8ce83abc, 0xf9ed8dc5, 0xc1730e4f, 0x0576398c, 0x6268b0f8, 0x597be1b8, 0xd4096dc4,
    0xb78ed43b, 0x6486fb7e, 0x0500700b, 0x81eb2f76, 0x7146ed51, 0xe347b78f, 0xfb95371d, 0xe48f842a,
    0x6d9f0143, 0x66093349, 0x45543f0a, 0x998844c5, 0x2884a1f1, 0x03b71454, 0xb26a96b1, 0xa20f3324,
    0x703847ee, 0xe656abe1, 0x2f9c7dd5, 0x886e5ac2, 0xd7252a1d, 0x354fe131, 0x6eda230e, 0x79604512,
    0xe750c0b2, 0x82268d2e, 0xb15d63c4, 0x8c28ab0f, 0x8f92758b, 0x3073cbef, 0xe0ffd6f7, 0x7aa182c6,
    0xff078f76, 0x026ddebf, 0x9bfc93ca, 0x9b8de3bd, 0x8ada1f7b, 0xf6856914, 0xce4493f8, 0x2fd5590c,
    0xdaf6db84, 0x348f38e7, 0x9091cb51, 0xd926e1c7, 0xf6bd1222, 0xb80025cf, 0x91da1aff, 0xf08b8502,
    0x5c86d5e5, 0xe5fb873f, 0x270d87f1, 0x334397ad, 0x88845685, 0x5f484771, 0x7da7dd8b, 0x6ac20613,
    0xbfc250d0, 0x7b48d664, 0x57ba042f, 0x8bf3d136, 0xbf495817, 0xbd184ac3, 0x114bc996, 0xb3503d32,
    0x230fbceb, 0xc1364ff7, 0xee89f9bc, 0xab7f43ba, 0xdacdb928, 0x64da7f65, 0x4b9fb97a, 0xe4193a7b,
    0x053068e3, 0x06a2b1ad, 0xb5e06a78, 0xa6da4d58, 0xde2e64f8, 0x16095fde, 0x4f1ff846, 0xb5dcaaa8,
    0x89daca65, 0x53c1bd62, 0xefc9c382, 0xd8cfb542, 0xe7081209, 0xcb650d73, 0xde9e0789, 0x4ab7f439,
    0xa856128f, 0x7ea68bcb, 0x65cf7a81, 0x0b0ddc8f, 0x434765a1, 0x08a0d52b, 0x489dffbf, 0x1bf8e60b,
    0xb077b668};

static void poly_xor(uint32_t *res, uint32_t *A, uint32_t *B, uint32_t len){
	for(uint32_t i=0; i<len; ++i){
		res[i] = A[i] ^ B[i];
	}
} 

void CRT_fft_HQC3(uint32_t *res, const uint32_t *A, const uint32_t *B){
    uint32_t A_FAFFT[N_FAFFT_32] = {0}, B_FAFFT[N_FAFFT_32] = {0};
    uint32_t rx[LOW_LEN << 1];
    uint32_t tmp[LOW_LEN];
    uint32_t t[LOW_LEN << 1];
    uint32_t r[2 * VEC_N_SIZE_32] = {0};
    memcpy(A_FAFFT, A, VEC_N_SIZE_32 * 4);
    memcpy(B_FAFFT, B, VEC_N_SIZE_32 * 4);
    bmul2_8192_to_8192((uint8_t *)r, (uint8_t *)A_FAFFT, (uint8_t *)B_FAFFT);
    radix16_193(rx, A, B);
    poly_xor(tmp, rx, r, LOW_LEN);
    radix16_193(t, tmp, p_inv);
    bit_shift_xor_193_output_asm(r, t, blk_voff, blk_cnt, NBLOCKS, tail_voff, TAIL_CNT);
    reduce(res, r);
}

/* Pair variant: res1 = A_common * B1, res2 = A_common * B2 (mod x^n - 1),
 * sharing the FAFFT prepare of A_common (Chen-style saves ~1 fafft_input). */
void CRT_fft_HQC3_pair(uint32_t *res1, uint32_t *res2,
                       const uint32_t *A_common,
                       const uint32_t *B1, const uint32_t *B2) {
    uint32_t A_FAFFT[N_FAFFT_32] = {0};
    uint32_t B1_FAFFT[N_FAFFT_32] = {0};
    uint32_t B2_FAFFT[N_FAFFT_32] = {0};
    uint32_t A_prep[N_FAFFT_32];
    uint32_t B1_prep[N_FAFFT_32];
    uint32_t B2_prep[N_FAFFT_32];
    uint32_t rx[LOW_LEN << 1];
    uint32_t tmp[LOW_LEN];
    uint32_t t[LOW_LEN << 1];
    uint32_t r[2 * VEC_N_SIZE_32];

    memcpy(A_FAFFT, A_common, VEC_N_SIZE_32 * 4);
    memcpy(B1_FAFFT, B1, VEC_N_SIZE_32 * 4);
    memcpy(B2_FAFFT, B2, VEC_N_SIZE_32 * 4);

    /* Prepare all three FAFFTs once (A_common shared across both mults). */
    bmul2_8192_to_8192_prepare(A_prep, (uint8_t *)A_FAFFT);
    bmul2_8192_to_8192_prepare(B1_prep, (uint8_t *)B1_FAFFT);
    bmul2_8192_to_8192_prepare(B2_prep, (uint8_t *)B2_FAFFT);

    /* Mult 1: A_common * B1 */
    memset(r, 0, sizeof(r));
    bmul2_4096_to_8192_mul((uint8_t *)r, A_prep, B1_prep);
    radix16_193(rx, A_common, B1);
    poly_xor(tmp, rx, r, LOW_LEN);
    radix16_193(t, tmp, p_inv);
    bit_shift_xor_193_output_asm(r, t, blk_voff, blk_cnt, NBLOCKS, tail_voff, TAIL_CNT);
    reduce(res1, r);

    /* Mult 2: A_common * B2 (reuses A_prep) */
    memset(r, 0, sizeof(r));
    bmul2_4096_to_8192_mul((uint8_t *)r, A_prep, B2_prep);
    radix16_193(rx, A_common, B2);
    poly_xor(tmp, rx, r, LOW_LEN);
    radix16_193(t, tmp, p_inv);
    bit_shift_xor_193_output_asm(r, t, blk_voff, blk_cnt, NBLOCKS, tail_voff, TAIL_CNT);
    reduce(res2, r);
}

/* ---------------------------------------------------------------------------
 * Public-key cached variants.
 *
 * The public operand (h or s) is supplied already bmul2-prepared (B_fft, the
 * FFT part) together with its low LOW_LEN u32 (B_low, the CRT residue part).
 * Only the ephemeral operand A (r2 in encrypt, y in keygen) is prepared here,
 * so a cache hit removes one bmul2_8192_to_8192_prepare per public multiply.
 *
 * B_fft MUST be exactly what bmul2_8192_to_8192_prepare() produces for B, and
 * B_low MUST be the low LOW_LEN u32 of the raw B polynomial -- radix16_193
 * reads only those. pkcache_fill_fft() builds both. Nothing here touches secret
 * state beyond A itself; B_fft/B_low are public. ------------------------- */

/* res = A * B (mod x^n - 1), with B given via its cached FFT form (B_fft) and
 * its low LOW_LEN u32 (B_low). */
void CRT_fft_HQC3_cached(uint32_t *res, const uint32_t *A,
                         const uint32_t *B_fft, const uint32_t *B_low) {
    uint32_t A_FAFFT[N_FAFFT_32] = {0};
    uint32_t A_prep[N_FAFFT_32];
    uint32_t rx[LOW_LEN << 1];
    uint32_t tmp[LOW_LEN];
    uint32_t t[LOW_LEN << 1];
    uint32_t r[2 * VEC_N_SIZE_32] = {0};

    memcpy(A_FAFFT, A, VEC_N_SIZE_32 * 4);
    bmul2_8192_to_8192_prepare(A_prep, (uint8_t *)A_FAFFT);

    /* FFT part: A_prep (fresh) * B_fft (cached). */
    bmul2_4096_to_8192_mul((uint8_t *)r, A_prep, B_fft);
    /* residue part: raw A low-193 * cached B low-193. */
    radix16_193(rx, A, B_low);
    poly_xor(tmp, rx, r, LOW_LEN);
    radix16_193(t, tmp, p_inv);
    bit_shift_xor_193_output_asm(r, t, blk_voff, blk_cnt, NBLOCKS, tail_voff, TAIL_CNT);
    reduce(res, r);
}

/* Pair variant used by encrypt: res1 = A * h, res2 = A * s (mod x^n - 1), where
 * A is the shared ephemeral operand (r2). h is supplied fully cached (h_fft +
 * h_low); s is supplied as its cached FFT form (s_fft) plus the raw s low part
 * (s_low, read straight from the public key). A is prepared once and reused. */
void CRT_fft_HQC3_pair_cached(uint32_t *res1, uint32_t *res2,
                              const uint32_t *A,
                              const uint32_t *h_fft, const uint32_t *h_low,
                              const uint32_t *s_fft, const uint32_t *s_low) {
    uint32_t A_FAFFT[N_FAFFT_32] = {0};
    uint32_t A_prep[N_FAFFT_32];
    uint32_t rx[LOW_LEN << 1];
    uint32_t tmp[LOW_LEN];
    uint32_t t[LOW_LEN << 1];
    uint32_t r[2 * VEC_N_SIZE_32];

    memcpy(A_FAFFT, A, VEC_N_SIZE_32 * 4);
    bmul2_8192_to_8192_prepare(A_prep, (uint8_t *)A_FAFFT);  /* shared prepare */

    /* Mult 1: A * h  (h fully cached: h_fft for FFT part, h_low for residue) */
    memset(r, 0, sizeof(r));
    bmul2_4096_to_8192_mul((uint8_t *)r, A_prep, h_fft);
    radix16_193(rx, A, h_low);
    poly_xor(tmp, rx, r, LOW_LEN);
    radix16_193(t, tmp, p_inv);
    bit_shift_xor_193_output_asm(r, t, blk_voff, blk_cnt, NBLOCKS, tail_voff, TAIL_CNT);
    reduce(res1, r);

    /* Mult 2: A * s  (s_fft cached for FFT part, raw s_low for residue) */
    memset(r, 0, sizeof(r));
    bmul2_4096_to_8192_mul((uint8_t *)r, A_prep, s_fft);
    radix16_193(rx, A, s_low);
    poly_xor(tmp, rx, r, LOW_LEN);
    radix16_193(t, tmp, p_inv);
    bit_shift_xor_193_output_asm(r, t, blk_voff, blk_cnt, NBLOCKS, tail_voff, TAIL_CNT);
    reduce(res2, r);
}