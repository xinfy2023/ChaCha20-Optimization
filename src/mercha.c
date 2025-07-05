#include "mercha.h"

void mercha(const uint8_t key[32], const uint8_t nonce[12], uint8_t *input, uint8_t *output, size_t length) {
    chacha20_encrypt(key, nonce, 0, input, length);
    merkel_tree(input, output, length);
}