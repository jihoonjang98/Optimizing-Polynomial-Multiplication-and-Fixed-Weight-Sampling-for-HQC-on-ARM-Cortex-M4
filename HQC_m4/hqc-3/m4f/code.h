#ifndef CODE_H
#define CODE_H


/**
 * @file code.h
 * @brief Header file of code.c
 */

#include <stdint.h>

void PQCLEAN_HQC192_CLEAN_code_encode(uint64_t *em, const uint8_t *message);

void PQCLEAN_HQC192_CLEAN_code_decode(uint8_t *m, const uint64_t *em);

/* Interface-compatibility wrappers matching the hqc-3_0713 calling convention
 * used by this scheme's hqc.c/kem.c. */
void code_encode(uint64_t *em, const uint64_t *m);
void code_decode(uint64_t *m, const uint64_t *em);


#endif
