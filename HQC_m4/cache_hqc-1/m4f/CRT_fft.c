#include <string.h>
#include "gfmul_fft.h"
#include "parameters.h"
#include "radix_mul.h"
#include "reduce.h"
#include "CRT_fft.h"
#include "outstat_index.h"

#define N_FAFFT 32768
#define N_FAFFT_32 1024

#define LOW_LEN 81 
const uint32_t p_inv[LOW_LEN] = {
    0xad99c721, 0x50c1b96a, 0x5c5a471f, 0xe5dae107, 0x593cd97d, 0x01fbccab, 0xb1e24ccb, 0xa7784e33,
    0x392abb85, 0xd9f5158e, 0x9671575a, 0xae387f3f, 0xc178b564, 0x88b22068, 0x8916e321, 0x93be7bdc,
    0x5032c2e7, 0x83fc1f90, 0xed2c14cf, 0xa39becc4, 0x84b91cec, 0x9a44da93, 0x9852db2c, 0x44712f82,
    0x98f61f2a, 0xa2f802c8, 0x18103e5c, 0xf3b73061, 0xbf139b38, 0xff29b84e, 0x2dfcca06, 0x0a2a67fe,
    0x13739dff, 0xa7fed1ac, 0xbae50081, 0xf3a99326, 0x5f5728e7, 0xfe15b9b2, 0x7127b6eb, 0xb0f9808f,
    0xe646634e, 0x94da5953, 0xca54066d, 0x27593fad, 0x54611c89, 0x258cb619, 0xbc2c41eb, 0x627f3fe9,
    0x302546a2, 0xdc258e2d, 0x78f14ff6, 0x33e4d645, 0x9f7f75f0, 0x30b92a1b, 0xb0aad4dc, 0x70031e69,
    0x454cc888, 0xe3e9da76, 0x6da6b580, 0x114aae04, 0x6d6cd048, 0x8938396f, 0xdb940a8b, 0xf953d57c,
    0xe4da843d, 0xa9548b0b, 0x54a73f94, 0x74d72911, 0x8f6574ba, 0x5483d5b4, 0xcbd9b78a, 0xf378811f,
    0x90ada6a9, 0xc43b521b, 0x61566589, 0x68071261, 0xd27c0809, 0x8b88f9db, 0x84d78b57, 0xcf5c557e,
    0x5cfecfc0};

static void poly_xor(uint32_t *res, uint32_t *A, uint32_t *B, uint32_t len){
	for(uint32_t i=0; i<len; ++i){
		res[i] = A[i] ^ B[i];
	}
}

void CRT_fft_HQC1(uint32_t *res, const uint32_t *A, const uint32_t *B){
    uint32_t A_FAFFT[N_FAFFT_32] = {0}, B_FAFFT[N_FAFFT_32] = {0};
    uint32_t rx[LOW_LEN << 1];
    uint32_t tmp[LOW_LEN];
    uint32_t t[LOW_LEN << 1];
    uint32_t r[OUT_WORDS] = {0};
    /* Chen radix-16 residue mult (gf2x_mul_3072): 96-word input, low 81 used.
     * residue operands are the low LOW_LEN(=81) words, zero-padded to 96. */
    uint32_t a96[96] = {0}, b96[96] = {0}, tmp96[96] = {0}, pinv96[96] = {0};
    memcpy(A_FAFFT, A, VEC_N_SIZE_32 * 4);
    memcpy(B_FAFFT, B, VEC_N_SIZE_32 * 4);
    bmul2_4096_to_4096((uint8_t *)r, (uint8_t *)A_FAFFT, (uint8_t *)B_FAFFT);
    memcpy(a96, A, LOW_LEN * 4);
    memcpy(b96, B, LOW_LEN * 4);
    gf2x_mul_3072(rx, a96, b96);
    poly_xor(tmp, rx, r, LOW_LEN);
    memcpy(tmp96, tmp, LOW_LEN * 4);
    memcpy(pinv96, p_inv, LOW_LEN * 4);
    gf2x_mul_3072(t, tmp96, pinv96);
    bit_shift_xor_81_output_asm(r, t, blk_voff, blk_cnt, NBLOCKS);
    reduce(res, r);
}

/* Pair variant: res1 = A_common * B1, res2 = A_common * B2 (mod x^n - 1),
 * sharing the FAFFT prepare of A_common. */
void CRT_fft_HQC1_pair(uint32_t *res1, uint32_t *res2,
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
    uint32_t r[OUT_WORDS];
    uint32_t a96[96], b96[96], tmp96[96], pinv96[96];

    memcpy(A_FAFFT, A_common, VEC_N_SIZE_32 * 4);
    memcpy(B1_FAFFT, B1, VEC_N_SIZE_32 * 4);
    memcpy(B2_FAFFT, B2, VEC_N_SIZE_32 * 4);

    bmul2_4096_to_4096_prepare(A_prep, (uint8_t *)A_FAFFT);
    bmul2_4096_to_4096_prepare(B1_prep, (uint8_t *)B1_FAFFT);
    bmul2_4096_to_4096_prepare(B2_prep, (uint8_t *)B2_FAFFT);

    /* Mult 1: A_common * B1 */
    memset(r, 0, sizeof(r));
    bmul2_2048_to_4096_mul((uint8_t *)r, A_prep, B1_prep);
    memset(a96, 0, sizeof(a96)); memcpy(a96, A_common, LOW_LEN * 4);
    memset(b96, 0, sizeof(b96)); memcpy(b96, B1, LOW_LEN * 4);
    gf2x_mul_3072(rx, a96, b96);
    poly_xor(tmp, rx, r, LOW_LEN);
    memset(tmp96, 0, sizeof(tmp96)); memcpy(tmp96, tmp, LOW_LEN * 4);
    memset(pinv96, 0, sizeof(pinv96)); memcpy(pinv96, p_inv, LOW_LEN * 4);
    gf2x_mul_3072(t, tmp96, pinv96);
    bit_shift_xor_81_output_asm(r, t, blk_voff, blk_cnt, NBLOCKS);
    reduce(res1, r);

    /* Mult 2: A_common * B2 (reuses A_prep) */
    memset(r, 0, sizeof(r));
    bmul2_2048_to_4096_mul((uint8_t *)r, A_prep, B2_prep);
    memset(a96, 0, sizeof(a96)); memcpy(a96, A_common, LOW_LEN * 4);
    memset(b96, 0, sizeof(b96)); memcpy(b96, B2, LOW_LEN * 4);
    gf2x_mul_3072(rx, a96, b96);
    poly_xor(tmp, rx, r, LOW_LEN);
    memset(tmp96, 0, sizeof(tmp96)); memcpy(tmp96, tmp, LOW_LEN * 4);
    memset(pinv96, 0, sizeof(pinv96)); memcpy(pinv96, p_inv, LOW_LEN * 4);
    gf2x_mul_3072(t, tmp96, pinv96);
    bit_shift_xor_81_output_asm(r, t, blk_voff, blk_cnt, NBLOCKS);
    reduce(res2, r);
}

/* ---------------------------------------------------------------------------
 * Public-key cached variant.
 *
 * The public operand (h or s) is supplied already bmul2-prepared (B_fft, the
 * FFT part) together with its low LOW_LEN u32 (B_low, the CRT residue part).
 * Only the ephemeral operand A (r2 in encrypt) is prepared here, so a cache hit
 * removes one bmul2_4096_to_4096_prepare per public multiply.
 *
 * B_fft MUST be exactly what bmul2_4096_to_4096_prepare() produces for B, and
 * B_low MUST be the low LOW_LEN u32 of the raw B polynomial -- gf2x_mul_3072
 * (via the zero-padded b96 buffer) reads only those. pkcache_fill_fft() builds
 * both. Nothing here touches secret state beyond A itself; B_fft/B_low are
 * public. ---------------------------------------------------------------- */

/* Pair variant used by encrypt: res1 = A * h, res2 = A * s (mod x^n - 1), where
 * A is the shared ephemeral operand (r2). h is supplied fully cached (h_fft +
 * h_low); s is supplied as its cached FFT form (s_fft) plus the raw s low part
 * (s_low, read straight from the public key). A is prepared once and reused. */
void CRT_fft_HQC1_pair_cached(uint32_t *res1, uint32_t *res2,
                              const uint32_t *A,
                              const uint32_t *h_fft, const uint32_t *h_low,
                              const uint32_t *s_fft, const uint32_t *s_low) {
    uint32_t A_FAFFT[N_FAFFT_32] = {0};
    uint32_t A_prep[N_FAFFT_32];
    uint32_t rx[LOW_LEN << 1];
    uint32_t tmp[LOW_LEN];
    uint32_t t[LOW_LEN << 1];
    uint32_t r[OUT_WORDS];
    uint32_t a96[96], b96[96], tmp96[96], pinv96[96];

    memcpy(A_FAFFT, A, VEC_N_SIZE_32 * 4);
    bmul2_4096_to_4096_prepare(A_prep, (uint8_t *)A_FAFFT);  /* shared prepare */

    /* Mult 1: A * h  (h fully cached: h_fft for FFT part, h_low for residue) */
    memset(r, 0, sizeof(r));
    bmul2_2048_to_4096_mul((uint8_t *)r, A_prep, h_fft);
    memset(a96, 0, sizeof(a96)); memcpy(a96, A, LOW_LEN * 4);
    memset(b96, 0, sizeof(b96)); memcpy(b96, h_low, LOW_LEN * 4);
    gf2x_mul_3072(rx, a96, b96);
    poly_xor(tmp, rx, r, LOW_LEN);
    memset(tmp96, 0, sizeof(tmp96)); memcpy(tmp96, tmp, LOW_LEN * 4);
    memset(pinv96, 0, sizeof(pinv96)); memcpy(pinv96, p_inv, LOW_LEN * 4);
    gf2x_mul_3072(t, tmp96, pinv96);
    bit_shift_xor_81_output_asm(r, t, blk_voff, blk_cnt, NBLOCKS);
    reduce(res1, r);

    /* Mult 2: A * s  (s_fft cached for FFT part, raw s_low for residue) */
    memset(r, 0, sizeof(r));
    bmul2_2048_to_4096_mul((uint8_t *)r, A_prep, s_fft);
    memset(a96, 0, sizeof(a96)); memcpy(a96, A, LOW_LEN * 4);
    memset(b96, 0, sizeof(b96)); memcpy(b96, s_low, LOW_LEN * 4);
    gf2x_mul_3072(rx, a96, b96);
    poly_xor(tmp, rx, r, LOW_LEN);
    memset(tmp96, 0, sizeof(tmp96)); memcpy(tmp96, tmp, LOW_LEN * 4);
    memset(pinv96, 0, sizeof(pinv96)); memcpy(pinv96, p_inv, LOW_LEN * 4);
    gf2x_mul_3072(t, tmp96, pinv96);
    bit_shift_xor_81_output_asm(r, t, blk_voff, blk_cnt, NBLOCKS);
    reduce(res2, r);
}
