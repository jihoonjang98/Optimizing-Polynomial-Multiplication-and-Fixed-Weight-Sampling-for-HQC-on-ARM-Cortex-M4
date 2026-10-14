#include "code.h"
#include "parameters.h"
#include "reed_muller.h"
#include "reed_solomon.h"
#include <stdint.h>
/**
 * @file code.c
 * @brief Implementation of concatenated code
 */



/**
 *
 * @brief Encoding the message m to a code word em using the concatenated code
 *
 * First we encode the message using the Reed-Solomon code, then with the duplicated Reed-Muller code we obtain
 * a concatenated code word.
 *
 * @param[out] em Pointer to an array that is the tensor code word
 * @param[in] m Pointer to an array that is the message
 */
void PQCLEAN_HQC128_CLEAN_code_encode(uint64_t *em, const uint8_t *m) {
    uint8_t tmp[VEC_N1_SIZE_BYTES] = {0};

    PQCLEAN_HQC128_CLEAN_reed_solomon_encode(tmp, m);
    PQCLEAN_HQC128_CLEAN_reed_muller_encode(em, tmp);

}



/**
 * @brief Decoding the code word em to a message m using the concatenated code
 *
 * @param[out] m Pointer to an array that is the message
 * @param[in] em Pointer to an array that is the code word
 */
void PQCLEAN_HQC128_CLEAN_code_decode(uint8_t *m, const uint64_t *em) {
    uint8_t tmp[VEC_N1_SIZE_BYTES] = {0};

    PQCLEAN_HQC128_CLEAN_reed_muller_decode(tmp, em);
    PQCLEAN_HQC128_CLEAN_reed_solomon_decode(m, tmp);


}


/**
 * @brief Interface-compatibility wrappers so that hqc.c/kem.c (which use the
 * hqc-1_0713 calling convention) can drive the faster hqc-1_0702 RS/RM decoder.
 *
 * The message buffer is a byte array (uint8_t m[PARAM_SECURITY_BYTES] in kem.c,
 * passed as (uint64_t*)). The ported decoder reads/writes it as PARAM_K bytes,
 * so the pointer cast below is byte-layout safe.
 */
void code_encode(uint64_t *em, const uint64_t *m) {
    PQCLEAN_HQC128_CLEAN_code_encode(em, (const uint8_t *)m);
}

void code_decode(uint64_t *m, const uint64_t *em) {
    PQCLEAN_HQC128_CLEAN_code_decode((uint8_t *)m, em);
}
