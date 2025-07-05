#include "mercha.h"
#include <omp.h>


static inline void __attribute__((always_inline)) merge_hash(
    const uint8_t * restrict block1,
    const uint8_t * restrict block2,
    uint8_t * restrict output) {

    static int simd_checked = 0;
    static int has_avx2 = 0;
    
    if (!simd_checked) {
        has_avx2 = __builtin_cpu_supports("avx2");
        simd_checked = 1;
    }

    const uint32_t * restrict w1 = (const uint32_t * restrict)block1;
    const uint32_t * restrict w2 = (const uint32_t * restrict)block2;

    if (has_avx2) {
        // SIMD做输入/输出优化
        
        // 并行加载输入数据
        __m256i w1_low = _mm256_loadu_si256((__m256i*)&w1[0]);   // w1[0-7]
        __m256i w2_low = _mm256_loadu_si256((__m256i*)&w2[0]);   // w2[0-7]
        

        uint32_t w1_arr[8], w2_arr[8];
        _mm256_storeu_si256((__m256i*)w1_arr, w1_low);
        _mm256_storeu_si256((__m256i*)w2_arr, w2_low);
        

        register uint32_t s0 = w1_arr[0] ^ w2_arr[7];   register uint32_t s8 = w2_arr[0] ^ w1_arr[7];
        register uint32_t s1 = w1_arr[1] ^ w2_arr[6];   register uint32_t s9 = w2_arr[1] ^ w1_arr[6];
        register uint32_t s2 = w1_arr[2] ^ w2_arr[5];   register uint32_t s10 = w2_arr[2] ^ w1_arr[5];
        register uint32_t s3 = w1_arr[3] ^ w2_arr[4];   register uint32_t s11 = w2_arr[3] ^ w1_arr[4];
        register uint32_t s4 = w1_arr[4] ^ w2_arr[3];   register uint32_t s12 = w2_arr[4] ^ w1_arr[3];
        register uint32_t s5 = w1_arr[5] ^ w2_arr[2];   register uint32_t s13 = w2_arr[5] ^ w1_arr[2];
        register uint32_t s6 = w1_arr[6] ^ w2_arr[1];   register uint32_t s14 = w2_arr[6] ^ w1_arr[1];
        register uint32_t s7 = w1_arr[7] ^ w2_arr[0];   register uint32_t s15 = w2_arr[7] ^ w1_arr[0];
        

        #define HASH_ROUND() \
            s0 += s4; s0 = ROTL32(s0, 7);     s1 += s5; s1 = ROTL32(s1, 7); \
            s2 += s6; s2 = ROTL32(s2, 7);     s3 += s7; s3 = ROTL32(s3, 7); \
            s8 += s12; s8 = ROTL32(s8, 7);    s9 += s13; s9 = ROTL32(s9, 7); \
            s10 += s14; s10 = ROTL32(s10, 7); s11 += s15; s11 = ROTL32(s11, 7); \
            s0 += s8; s0 = ROTL32(s0, 9);     s1 += s9; s1 = ROTL32(s1, 9); \
            s2 += s10; s2 = ROTL32(s2, 9);    s3 += s11; s3 = ROTL32(s3, 9); \
            s4 += s12; s4 = ROTL32(s4, 9);    s5 += s13; s5 = ROTL32(s5, 9); \
            s6 += s14; s6 = ROTL32(s6, 9);    s7 += s15; s7 = ROTL32(s7, 9);
        
        HASH_ROUND() HASH_ROUND() HASH_ROUND() HASH_ROUND() HASH_ROUND()
        HASH_ROUND() HASH_ROUND() HASH_ROUND() HASH_ROUND() HASH_ROUND()
        
        #undef HASH_ROUND
        
        s0 += s15; s1 += s14; s2 += s13; s3 += s12;
        s4 += s11; s5 += s10; s6 += s9;  s7 += s8;
        
        // 并行输出
        uint32_t result[16] = {s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11, s12, s13, s14, s15};
        __m256i out_low = _mm256_loadu_si256((__m256i*)&result[0]);
        __m256i out_high = _mm256_loadu_si256((__m256i*)&result[8]);
        
        _mm256_storeu_si256((__m256i*)&((uint32_t*)output)[0], out_low);
        _mm256_storeu_si256((__m256i*)&((uint32_t*)output)[8], out_high);
        
    } else {

        register uint32_t s0 = w1[0] ^ w2[7];   register uint32_t s8 = w2[0] ^ w1[7];
        register uint32_t s1 = w1[1] ^ w2[6];   register uint32_t s9 = w2[1] ^ w1[6];
        register uint32_t s2 = w1[2] ^ w2[5];   register uint32_t s10 = w2[2] ^ w1[5];
        register uint32_t s3 = w1[3] ^ w2[4];   register uint32_t s11 = w2[3] ^ w1[4];
        register uint32_t s4 = w1[4] ^ w2[3];   register uint32_t s12 = w2[4] ^ w1[3];
        register uint32_t s5 = w1[5] ^ w2[2];   register uint32_t s13 = w2[5] ^ w1[2];
        register uint32_t s6 = w1[6] ^ w2[1];   register uint32_t s14 = w2[6] ^ w1[1];
        register uint32_t s7 = w1[7] ^ w2[0];   register uint32_t s15 = w2[7] ^ w1[0];
        
        s0 += s4; s0 = ROTL32(s0, 7);     s1 += s5; s1 = ROTL32(s1, 7);
        s2 += s6; s2 = ROTL32(s2, 7);     s3 += s7; s3 = ROTL32(s3, 7);
        s8 += s12; s8 = ROTL32(s8, 7);    s9 += s13; s9 = ROTL32(s9, 7);
        s10 += s14; s10 = ROTL32(s10, 7); s11 += s15; s11 = ROTL32(s11, 7);
        s0 += s8; s0 = ROTL32(s0, 9);     s1 += s9; s1 = ROTL32(s1, 9);
        s2 += s10; s2 = ROTL32(s2, 9);    s3 += s11; s3 = ROTL32(s3, 9);
        s4 += s12; s4 = ROTL32(s4, 9);    s5 += s13; s5 = ROTL32(s5, 9);
        s6 += s14; s6 = ROTL32(s6, 9);    s7 += s15; s7 = ROTL32(s7, 9);
        
        s0 += s4; s0 = ROTL32(s0, 7);     s1 += s5; s1 = ROTL32(s1, 7);
        s2 += s6; s2 = ROTL32(s2, 7);     s3 += s7; s3 = ROTL32(s3, 7);
        s8 += s12; s8 = ROTL32(s8, 7);    s9 += s13; s9 = ROTL32(s9, 7);
        s10 += s14; s10 = ROTL32(s10, 7); s11 += s15; s11 = ROTL32(s11, 7);
        s0 += s8; s0 = ROTL32(s0, 9);     s1 += s9; s1 = ROTL32(s1, 9);
        s2 += s10; s2 = ROTL32(s2, 9);    s3 += s11; s3 = ROTL32(s3, 9);
        s4 += s12; s4 = ROTL32(s4, 9);    s5 += s13; s5 = ROTL32(s5, 9);
        s6 += s14; s6 = ROTL32(s6, 9);    s7 += s15; s7 = ROTL32(s7, 9);
        
        s0 += s4; s0 = ROTL32(s0, 7);     s1 += s5; s1 = ROTL32(s1, 7);
        s2 += s6; s2 = ROTL32(s2, 7);     s3 += s7; s3 = ROTL32(s3, 7);
        s8 += s12; s8 = ROTL32(s8, 7);    s9 += s13; s9 = ROTL32(s9, 7);
        s10 += s14; s10 = ROTL32(s10, 7); s11 += s15; s11 = ROTL32(s11, 7);
        s0 += s8; s0 = ROTL32(s0, 9);     s1 += s9; s1 = ROTL32(s1, 9);
        s2 += s10; s2 = ROTL32(s2, 9);    s3 += s11; s3 = ROTL32(s3, 9);
        s4 += s12; s4 = ROTL32(s4, 9);    s5 += s13; s5 = ROTL32(s5, 9);
        s6 += s14; s6 = ROTL32(s6, 9);    s7 += s15; s7 = ROTL32(s7, 9);
        
        s0 += s4; s0 = ROTL32(s0, 7);     s1 += s5; s1 = ROTL32(s1, 7);
        s2 += s6; s2 = ROTL32(s2, 7);     s3 += s7; s3 = ROTL32(s3, 7);
        s8 += s12; s8 = ROTL32(s8, 7);    s9 += s13; s9 = ROTL32(s9, 7);
        s10 += s14; s10 = ROTL32(s10, 7); s11 += s15; s11 = ROTL32(s11, 7);
        s0 += s8; s0 = ROTL32(s0, 9);     s1 += s9; s1 = ROTL32(s1, 9);
        s2 += s10; s2 = ROTL32(s2, 9);    s3 += s11; s3 = ROTL32(s3, 9);
        s4 += s12; s4 = ROTL32(s4, 9);    s5 += s13; s5 = ROTL32(s5, 9);
        s6 += s14; s6 = ROTL32(s6, 9);    s7 += s15; s7 = ROTL32(s7, 9);
        
        s0 += s4; s0 = ROTL32(s0, 7);     s1 += s5; s1 = ROTL32(s1, 7);
        s2 += s6; s2 = ROTL32(s2, 7);     s3 += s7; s3 = ROTL32(s3, 7);
        s8 += s12; s8 = ROTL32(s8, 7);    s9 += s13; s9 = ROTL32(s9, 7);
        s10 += s14; s10 = ROTL32(s10, 7); s11 += s15; s11 = ROTL32(s11, 7);
        s0 += s8; s0 = ROTL32(s0, 9);     s1 += s9; s1 = ROTL32(s1, 9);
        s2 += s10; s2 = ROTL32(s2, 9);    s3 += s11; s3 = ROTL32(s3, 9);
        s4 += s12; s4 = ROTL32(s4, 9);    s5 += s13; s5 = ROTL32(s5, 9);
        s6 += s14; s6 = ROTL32(s6, 9);    s7 += s15; s7 = ROTL32(s7, 9);
        
        s0 += s4; s0 = ROTL32(s0, 7);     s1 += s5; s1 = ROTL32(s1, 7);
        s2 += s6; s2 = ROTL32(s2, 7);     s3 += s7; s3 = ROTL32(s3, 7);
        s8 += s12; s8 = ROTL32(s8, 7);    s9 += s13; s9 = ROTL32(s9, 7);
        s10 += s14; s10 = ROTL32(s10, 7); s11 += s15; s11 = ROTL32(s11, 7);
        s0 += s8; s0 = ROTL32(s0, 9);     s1 += s9; s1 = ROTL32(s1, 9);
        s2 += s10; s2 = ROTL32(s2, 9);    s3 += s11; s3 = ROTL32(s3, 9);
        s4 += s12; s4 = ROTL32(s4, 9);    s5 += s13; s5 = ROTL32(s5, 9);
        s6 += s14; s6 = ROTL32(s6, 9);    s7 += s15; s7 = ROTL32(s7, 9);
        
        s0 += s4; s0 = ROTL32(s0, 7);     s1 += s5; s1 = ROTL32(s1, 7);
        s2 += s6; s2 = ROTL32(s2, 7);     s3 += s7; s3 = ROTL32(s3, 7);
        s8 += s12; s8 = ROTL32(s8, 7);    s9 += s13; s9 = ROTL32(s9, 7);
        s10 += s14; s10 = ROTL32(s10, 7); s11 += s15; s11 = ROTL32(s11, 7);
        s0 += s8; s0 = ROTL32(s0, 9);     s1 += s9; s1 = ROTL32(s1, 9);
        s2 += s10; s2 = ROTL32(s2, 9);    s3 += s11; s3 = ROTL32(s3, 9);
        s4 += s12; s4 = ROTL32(s4, 9);    s5 += s13; s5 = ROTL32(s5, 9);
        s6 += s14; s6 = ROTL32(s6, 9);    s7 += s15; s7 = ROTL32(s7, 9);
        
        s0 += s4; s0 = ROTL32(s0, 7);     s1 += s5; s1 = ROTL32(s1, 7);
        s2 += s6; s2 = ROTL32(s2, 7);     s3 += s7; s3 = ROTL32(s3, 7);
        s8 += s12; s8 = ROTL32(s8, 7);    s9 += s13; s9 = ROTL32(s9, 7);
        s10 += s14; s10 = ROTL32(s10, 7); s11 += s15; s11 = ROTL32(s11, 7);
        s0 += s8; s0 = ROTL32(s0, 9);     s1 += s9; s1 = ROTL32(s1, 9);
        s2 += s10; s2 = ROTL32(s2, 9);    s3 += s11; s3 = ROTL32(s3, 9);
        s4 += s12; s4 = ROTL32(s4, 9);    s5 += s13; s5 = ROTL32(s5, 9);
        s6 += s14; s6 = ROTL32(s6, 9);    s7 += s15; s7 = ROTL32(s7, 9);
        
        s0 += s4; s0 = ROTL32(s0, 7);     s1 += s5; s1 = ROTL32(s1, 7);
        s2 += s6; s2 = ROTL32(s2, 7);     s3 += s7; s3 = ROTL32(s3, 7);
        s8 += s12; s8 = ROTL32(s8, 7);    s9 += s13; s9 = ROTL32(s9, 7);
        s10 += s14; s10 = ROTL32(s10, 7); s11 += s15; s11 = ROTL32(s11, 7);
        s0 += s8; s0 = ROTL32(s0, 9);     s1 += s9; s1 = ROTL32(s1, 9);
        s2 += s10; s2 = ROTL32(s2, 9);    s3 += s11; s3 = ROTL32(s3, 9);
        s4 += s12; s4 = ROTL32(s4, 9);    s5 += s13; s5 = ROTL32(s5, 9);
        s6 += s14; s6 = ROTL32(s6, 9);    s7 += s15; s7 = ROTL32(s7, 9);
        
        s0 += s15; s1 += s14; s2 += s13; s3 += s12;
        s4 += s11; s5 += s10; s6 += s9;  s7 += s8;
        
        uint32_t * restrict out32 = (uint32_t * restrict)output;
        out32[0] = s0;  out32[1] = s1;  out32[2] = s2;  out32[3] = s3;
        out32[4] = s4;  out32[5] = s5;  out32[6] = s6;  out32[7] = s7;
        out32[8] = s8;  out32[9] = s9;  out32[10] = s10; out32[11] = s11;
        out32[12] = s12; out32[13] = s13; out32[14] = s14; out32[15] = s15;
    }
}

static inline void optimized_xor_block(uint8_t * restrict buffer, 
                                      const uint8_t * restrict keystream, 
                                      size_t block_size) {
    static int has_avx2 = 0;
    static int checked = 0;
    if (!checked) {
        has_avx2 = __builtin_cpu_supports("avx2");
        checked = 1;
    }
    
    if (has_avx2 && block_size == 64) {

        __m256i data1 = _mm256_loadu_si256((__m256i*)buffer);
        __m256i data2 = _mm256_loadu_si256((__m256i*)(buffer + 32));
        __m256i key1 = _mm256_loadu_si256((__m256i*)keystream);
        __m256i key2 = _mm256_loadu_si256((__m256i*)(keystream + 32));
        
        data1 = _mm256_xor_si256(data1, key1);
        data2 = _mm256_xor_si256(data2, key2);
        
        _mm256_storeu_si256((__m256i*)buffer, data1);
        _mm256_storeu_si256((__m256i*)(buffer + 32), data2);
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

void merkel_tree(const uint8_t *input, uint8_t *output, size_t length) {
    

    if (length == 64) {
        memcpy(output, input, 64);
        return;
    }
    

    const size_t alignment = 64;
    
    // 使用posix_memalign或aligned_alloc进行对齐分配
    uint8_t *buf1, *buf2;
    #ifdef _POSIX_C_SOURCE
        if (posix_memalign((void**)&buf1, alignment, length) != 0) {
            buf1 = malloc(length);
        }
        if (posix_memalign((void**)&buf2, alignment, length) != 0) {
            buf2 = malloc(length);
        }
    #else
        buf1 = malloc(length + alignment - 1);
        buf2 = malloc(length + alignment - 1);
        // 手动对齐
        buf1 = (uint8_t*)(((uintptr_t)buf1 + alignment - 1) & ~(alignment - 1));
        buf2 = (uint8_t*)(((uintptr_t)buf2 + alignment - 1) & ~(alignment - 1));
    #endif
    
    // 缓存局部性优化：使用restrict指针
    uint8_t * restrict prev_buf = buf1;
    uint8_t * restrict cur_buf = buf2;
    

    const uint8_t * restrict first_input = input;
    int first_iteration = 1;
    
    length /= 2;
    
    while (length >= 64) {
        const size_t num_blocks = length / 64;
        
        // 动态并行策略优化
        const int num_threads = omp_get_max_threads();
        const size_t blocks_per_thread = num_blocks / num_threads;
        const int use_parallel = (num_blocks >= 4) && (blocks_per_thread >= 1);
        
        if (use_parallel) {
            // 高级并行策略：分块处理以提高缓存效率
            #pragma omp parallel num_threads(num_threads)
            {
                const int thread_id = omp_get_thread_num();
                const size_t start_block = thread_id * blocks_per_thread;
                const size_t end_block = (thread_id == num_threads - 1) ? 
                                       num_blocks : start_block + blocks_per_thread;
                
                // 线程本地的预取优化
                for (size_t i = start_block; i < end_block; ++i) {
                    // 预取下一个块的数据
                    if (i + 1 < end_block) {
                        const uint8_t *next_block1 = first_iteration ? 
                            &first_input[(2*(i+1))*64] : &prev_buf[(2*(i+1))*64];
                        const uint8_t *next_block2 = first_iteration ? 
                            &first_input[(2*(i+1)+1)*64] : &prev_buf[(2*(i+1)+1)*64];
                        
                        #ifdef __GNUC__
                        __builtin_prefetch(next_block1, 0, 3);
                        __builtin_prefetch(next_block2, 0, 3);
                        __builtin_prefetch(&cur_buf[(i+1)*64], 1, 3);
                        #endif
                    }
                    
                    if (first_iteration) {
                        merge_hash(&first_input[(2*i)*64], &first_input[(2*i+1)*64], &cur_buf[i*64]);
                    } else {
                        merge_hash(&prev_buf[(2*i)*64], &prev_buf[(2*i+1)*64], &cur_buf[i*64]);
                    }
                }
            }
        } else {
            for (size_t i = 0; i < num_blocks; ++i) {
                if (i + 1 < num_blocks) {
                    const uint8_t *next_block1 = first_iteration ? 
                        &first_input[(2*(i+1))*64] : &prev_buf[(2*(i+1))*64];
                    const uint8_t *next_block2 = first_iteration ? 
                        &first_input[(2*(i+1)+1)*64] : &prev_buf[(2*(i+1)+1)*64];
                    
                    #ifdef __GNUC__
                    __builtin_prefetch(next_block1, 0, 3);
                    __builtin_prefetch(next_block2, 0, 3);
                    __builtin_prefetch(&cur_buf[(i+1)*64], 1, 3);
                    #endif
                }
                
                if (first_iteration) {
                    merge_hash(&first_input[(2*i)*64], &first_input[(2*i+1)*64], &cur_buf[i*64]);
                } else {
                    merge_hash(&prev_buf[(2*i)*64], &prev_buf[(2*i+1)*64], &cur_buf[i*64]);
                }
            }
        }
        
   
        if (first_iteration) {
            first_iteration = 0;
            prev_buf = cur_buf;
            cur_buf = (cur_buf == buf1) ? buf2 : buf1;
        } else {
            uint8_t * restrict tmp = cur_buf;
            cur_buf = prev_buf;
            prev_buf = tmp;
        }
        
        length /= 2;
    }
    
 
    #ifdef __GNUC__
    __builtin_prefetch(output, 1, 3);
    #endif
    
   
    if (((uintptr_t)prev_buf & 63) == 0 && ((uintptr_t)output & 63) == 0) {
      
        *((uint64_t*)output + 0) = *((uint64_t*)prev_buf + 0);
        *((uint64_t*)output + 1) = *((uint64_t*)prev_buf + 1);
        *((uint64_t*)output + 2) = *((uint64_t*)prev_buf + 2);
        *((uint64_t*)output + 3) = *((uint64_t*)prev_buf + 3);
        *((uint64_t*)output + 4) = *((uint64_t*)prev_buf + 4);
        *((uint64_t*)output + 5) = *((uint64_t*)prev_buf + 5);
        *((uint64_t*)output + 6) = *((uint64_t*)prev_buf + 6);
        *((uint64_t*)output + 7) = *((uint64_t*)prev_buf + 7);
    } else {
    
        memcpy(output, prev_buf, 64);
    }
    
   
    #ifdef _POSIX_C_SOURCE
        free(buf1);
        free(buf2);
    #else
      
        free(buf1);
        free(buf2);
    #endif
}