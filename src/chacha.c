#include <omp.h>
#include <string.h>
#include <stdint.h>
#include <immintrin.h>  


#ifdef __GNUC__
    #define PREFETCH_READ(addr) __builtin_prefetch((addr), 0, 3)
    #define PREFETCH_WRITE(addr) __builtin_prefetch((addr), 1, 3)
#else
    #define PREFETCH_READ(addr) 
    #define PREFETCH_WRITE(addr)
#endif


#define QUARTERROUND_OPTIMIZED(a, b, c, d) \
    do { \
        a += b; d ^= a; d = (d << 16) | (d >> 16); \
        c += d; b ^= c; b = (b << 12) | (b >> 20); \
        a += b; d ^= a; d = (d << 8) | (d >> 24); \
        c += d; b ^= c; b = (b << 7) | (b >> 25); \
    } while(0)


// SIMD优化的XOR函数
static inline void simd_xor_block(uint8_t * restrict buffer, 
                                 const uint8_t * restrict keystream, 
                                 size_t block_size) {
    static int has_avx2 = 0;
    static int checked = 0;
    if (!checked) {
        has_avx2 = __builtin_cpu_supports("avx2"); // SIMD: CPU特性检测
        checked = 1;
    }
    
    if (has_avx2 && block_size >= 32) {
        // SIMD: 使用AVX2进行32字节块的并行XOR
        const size_t avx_blocks = block_size / 32;
        
        for (size_t i = 0; i < avx_blocks; i++) {
            __m256i data = _mm256_loadu_si256((__m256i*)(buffer + i * 32)); // SIMD: AVX2加载指令
            __m256i key = _mm256_loadu_si256((__m256i*)(keystream + i * 32)); 
            __m256i result = _mm256_xor_si256(data, key); // SIMD: AVX2并行XOR指令
            _mm256_storeu_si256((__m256i*)(buffer + i * 32), result); 
        }
        
        for (size_t i = avx_blocks * 32; i < block_size; i++) {
            buffer[i] ^= keystream[i];
        }
    } else {
        const uint64_t * restrict buf64 = (const uint64_t * restrict)buffer;
        const uint64_t * restrict key64 = (const uint64_t * restrict)keystream;
        uint64_t * restrict out64 = (uint64_t * restrict)buffer;
        
        const size_t full_qwords = block_size / 8;
        for (size_t i = 0; i < full_qwords; i++) {
            out64[i] = buf64[i] ^ key64[i];
        }
        
        for (size_t i = full_qwords * 8; i < block_size; i++) {
            buffer[i] ^= keystream[i];
        }
    }
}

static inline void __attribute__((always_inline)) chacha20_block(uint32_t state[16], uint8_t *output) {
    

    #ifdef __GNUC__
    __builtin_prefetch(output, 1, 3);    
    __builtin_prefetch(output + 32, 1, 3); 
    #endif
    

    register uint32_t x0 = state[0], x1 = state[1], x2 = state[2], x3 = state[3];
    register uint32_t x4 = state[4], x5 = state[5], x6 = state[6], x7 = state[7];
    register uint32_t x8 = state[8], x9 = state[9], x10 = state[10], x11 = state[11];
    register uint32_t x12 = state[12], x13 = state[13], x14 = state[14], x15 = state[15];
    
    #define QR_INLINE(a, b, c, d) \
        a += b; d ^= a; d = (d << 16) | (d >> 16); \
        c += d; b ^= c; b = (b << 12) | (b >> 20); \
        a += b; d ^= a; d = (d << 8) | (d >> 24); \
        c += d; b ^= c; b = (b << 7) | (b >> 25);
    
    #define DOUBLE_ROUND() \
        QR_INLINE(x0, x4, x8, x12);  QR_INLINE(x1, x5, x9, x13); \
        QR_INLINE(x2, x6, x10, x14); QR_INLINE(x3, x7, x11, x15); \
        QR_INLINE(x0, x5, x10, x15); QR_INLINE(x1, x6, x11, x12); \
        QR_INLINE(x2, x7, x8, x13);  QR_INLINE(x3, x4, x9, x14);
    
    DOUBLE_ROUND()  
    DOUBLE_ROUND() 
    DOUBLE_ROUND() 
    DOUBLE_ROUND() 
    DOUBLE_ROUND()  
    DOUBLE_ROUND() 
    DOUBLE_ROUND() 
    DOUBLE_ROUND() 
    DOUBLE_ROUND() 
    DOUBLE_ROUND() 
    
    #undef DOUBLE_ROUND
    #undef QR_INLINE
    
    register uint32_t s0 = state[0], s1 = state[1], s2 = state[2], s3 = state[3];
    register uint32_t s4 = state[4], s5 = state[5], s6 = state[6], s7 = state[7];
    register uint32_t s8 = state[8], s9 = state[9], s10 = state[10], s11 = state[11];
    register uint32_t s12 = state[12], s13 = state[13], s14 = state[14], s15 = state[15];
    
    x0 += s0;   x4 += s4;   x8 += s8;   x12 += s12;
    x1 += s1;   x5 += s5;   x9 += s9;   x13 += s13;
    x2 += s2;   x6 += s6;   x10 += s10; x14 += s14;
    x3 += s3;   x7 += s7;   x11 += s11; x15 += s15;
    
    uint32_t * restrict out32 = (uint32_t * restrict)output;
    
    out32[0] = x0;   out32[1] = x1;   out32[2] = x2;   out32[3] = x3;
    out32[4] = x4;   out32[5] = x5;   out32[6] = x6;   out32[7] = x7;
    out32[8] = x8;   out32[9] = x9;   out32[10] = x10; out32[11] = x11;
    out32[12] = x12; out32[13] = x13; out32[14] = x14; out32[15] = x15;
}

void chacha20_encrypt(const uint8_t key[32], const uint8_t nonce[12], 
    uint32_t initial_counter, uint8_t *buffer, size_t length) {

if (length == 0) return;

#ifdef __GNUC__
__builtin_prefetch(buffer, 1, 3);
__builtin_prefetch(buffer + 64, 1, 3);
#endif

uint32_t state[16];

state[0] = 0x61707865; state[1] = 0x3320646e; 
state[2] = 0x79622d32; state[3] = 0x6b206574;

state[4] = key[0] | (key[1] << 8) | (key[2] << 16) | (key[3] << 24);
state[5] = key[4] | (key[5] << 8) | (key[6] << 16) | (key[7] << 24);
state[6] = key[8] | (key[9] << 8) | (key[10] << 16) | (key[11] << 24);
state[7] = key[12] | (key[13] << 8) | (key[14] << 16) | (key[15] << 24);
state[8] = key[16] | (key[17] << 8) | (key[18] << 16) | (key[19] << 24);
state[9] = key[20] | (key[21] << 8) | (key[22] << 16) | (key[23] << 24);
state[10] = key[24] | (key[25] << 8) | (key[26] << 16) | (key[27] << 24);
state[11] = key[28] | (key[29] << 8) | (key[30] << 16) | (key[31] << 24);

state[12] = initial_counter;

state[13] = nonce[0] | (nonce[1] << 8) | (nonce[2] << 16) | (nonce[3] << 24);
state[14] = nonce[4] | (nonce[5] << 8) | (nonce[6] << 16) | (nonce[7] << 24);
state[15] = nonce[8] | (nonce[9] << 8) | (nonce[10] << 16) | (nonce[11] << 24);

const size_t num_blocks = (length + 63) / 64;

int num_threads = 1;
#ifdef _OPENMP
num_threads = omp_get_max_threads(); // MULTITHREADING: 获取最大线程数
#endif

if (num_blocks >= 16 && num_threads > 1) {
#pragma omp parallel num_threads(num_threads)
{
uint32_t local_state[16] __attribute__((aligned(64)));
uint8_t keystream_batch[256] __attribute__((aligned(64)));

local_state[0] = state[0];   local_state[1] = state[1];   
local_state[2] = state[2];   local_state[3] = state[3];
local_state[4] = state[4];   local_state[5] = state[5];   
local_state[6] = state[6];   local_state[7] = state[7];
local_state[8] = state[8];   local_state[9] = state[9];   
local_state[10] = state[10]; local_state[11] = state[11];
local_state[12] = state[12]; local_state[13] = state[13]; 
local_state[14] = state[14]; local_state[15] = state[15];

#pragma omp for schedule(dynamic, 4) // MULTITHREADING: OpenMP并行循环，动态调度
for (size_t block_batch = 0; block_batch < (num_blocks + 3) / 4; block_batch++) {
const size_t start_block = block_batch * 4;
const size_t end_block = (start_block + 4 <= num_blocks) ? start_block + 4 : num_blocks;
const size_t batch_size = end_block - start_block;

for (size_t i = 0; i < batch_size; i++) {
   local_state[12] = initial_counter + (uint32_t)(start_block + i);
   chacha20_block(local_state, &keystream_batch[i * 64]);
}

if (block_batch + 1 < (num_blocks + 3) / 4) {
   const size_t next_offset = (start_block + 4) * 64;
   if (next_offset < length) {
       #ifdef __GNUC__
       __builtin_prefetch(buffer + next_offset, 1, 3);
       __builtin_prefetch(buffer + next_offset + 64, 1, 3);
       #endif
   }
}

for (size_t i = 0; i < batch_size; i++) {
   const size_t block_offset = (start_block + i) * 64;
   const size_t block_size = (block_offset + 64 <= length) ? 64 : (length - block_offset);
   
   // SIMD: 使用SIMD优化的XOR操作替换原来的循环
   simd_xor_block(buffer + block_offset, &keystream_batch[i * 64], block_size);
}
}
} // MULTITHREADING: OpenMP并行区域结束
}
else if (num_blocks >= 8) {
#pragma omp parallel for if(num_threads > 1) schedule(static, 2) // MULTITHREADING: OpenMP并行for循环
for (size_t block = 0; block < num_blocks; block++) {
uint32_t local_state[16]; 
uint8_t keystream[64]; 

memcpy(local_state, state, 64);
local_state[12] = initial_counter + (uint32_t)block;

chacha20_block(local_state, keystream);

const size_t block_offset = block * 64;
const size_t block_size = (block_offset + 64 <= length) ? 64 : (length - block_offset);

// SIMD: 使用SIMD优化的XOR操作
simd_xor_block(buffer + block_offset, keystream, block_size);
}
}
else if (num_blocks >= 2) {
uint8_t keystream[128]; 

size_t block = 0;

for (; block + 1 < num_blocks; block += 2) {
if (block + 3 < num_blocks) {
#ifdef __GNUC__
__builtin_prefetch(buffer + (block + 2) * 64, 1, 3);
#endif
}

state[12] = initial_counter + (uint32_t)block;
chacha20_block(state, keystream);

state[12] = initial_counter + (uint32_t)(block + 1);
chacha20_block(state, keystream + 64);

for (int i = 0; i < 2; i++) {
const size_t block_offset = (block + i) * 64;
const size_t block_size = (block_offset + 64 <= length) ? 64 : (length - block_offset);

// SIMD: 使用SIMD优化的XOR操作
simd_xor_block(buffer + block_offset, keystream + i * 64, block_size);
}
}

if (block < num_blocks) {
uint8_t keystream_single[64];
state[12] = initial_counter + (uint32_t)block;
chacha20_block(state, keystream_single);

const size_t block_offset = block * 64;
const size_t block_size = (block_offset + 64 <= length) ? 64 : (length - block_offset);

// SIMD: 使用SIMD优化的XOR操作
simd_xor_block(buffer + block_offset, keystream_single, block_size);
}
}
else {
uint8_t keystream[64];

for (size_t block = 0; block < num_blocks; block++) {
state[12] = initial_counter + (uint32_t)block;
chacha20_block(state, keystream);

const size_t block_offset = block * 64;
const size_t block_size = (block_offset + 64 <= length) ? 64 : (length - block_offset);

// SIMD: 使用SIMD优化的XOR操作
simd_xor_block(buffer + block_offset, keystream, block_size);
}
}
}