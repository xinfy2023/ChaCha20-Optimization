#ifndef _MERCHA_H
#define _MERCHA_H

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <immintrin.h>
#include <omp.h>        // 为OpenMP函数
#include <unistd.h>     

#define ROTL32(x, n) ((x) << (n)) | ((x) >> (32 - (n)))

void chacha20_encrypt(const uint8_t key[32], const uint8_t nonce[12], uint32_t initial_counter, uint8_t *buffer, size_t length);

void merkel_tree(const uint8_t *input, uint8_t *output, size_t length);

void mercha(const uint8_t key[32], const uint8_t nonce[12], uint8_t *input, uint8_t *output, size_t length);

#endif