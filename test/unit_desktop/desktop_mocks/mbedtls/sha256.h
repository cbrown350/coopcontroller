#ifndef MBEDTLS_SHA256_H_MOCK
#define MBEDTLS_SHA256_H_MOCK

// Mock mbedtls SHA256 implementation for desktop unit tests
// Provides the same API as mbedtls/sha256.h for compatibility

#include <cstdint>
#include <cstring>

// SHA256 produces a 32-byte (256-bit) hash
#define MBEDTLS_SHA256_OUTPUT_SIZE 32

// Context structure - simplified mock
typedef struct {
    uint32_t hash[8];
    uint64_t total_len;
    uint8_t buffer[64];
    int buffer_len;
} mbedtls_sha256_context;

// Initialize SHA256 context
inline void mbedtls_sha256_init(mbedtls_sha256_context* ctx) {
    if (ctx) {
        memset(ctx, 0, sizeof(mbedtls_sha256_context));
    }
}

// Free SHA256 context
inline void mbedtls_sha256_free(mbedtls_sha256_context* ctx) {
    if (ctx) {
        memset(ctx, 0, sizeof(mbedtls_sha256_context));
    }
}

// SHA256 constants
static const uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

// SHA256 helper functions
#define ROTR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
#define CH(x, y, z) (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define EP0(x) (ROTR(x, 2) ^ ROTR(x, 13) ^ ROTR(x, 22))
#define EP1(x) (ROTR(x, 6) ^ ROTR(x, 11) ^ ROTR(x, 25))
#define SIG0(x) (ROTR(x, 7) ^ ROTR(x, 18) ^ ((x) >> 3))
#define SIG1(x) (ROTR(x, 17) ^ ROTR(x, 19) ^ ((x) >> 10))

// Initial hash values
static const uint32_t init_hash[8] = {
    0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
    0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
};

// Process a 512-bit block
static void sha256_process_block(mbedtls_sha256_context* ctx) {
    uint32_t w[64];
    
    // Prepare message schedule
    for (int i = 0; i < 16; i++) {
        w[i] = (ctx->buffer[i * 4] << 24) | (ctx->buffer[i * 4 + 1] << 16) |
               (ctx->buffer[i * 4 + 2] << 8) | ctx->buffer[i * 4 + 3];
    }
    for (int i = 16; i < 64; i++) {
        w[i] = SIG1(w[i-2]) + w[i-7] + SIG0(w[i-15]) + w[i-16];
    }
    
    // Initialize working variables
    uint32_t a = ctx->hash[0];
    uint32_t b = ctx->hash[1];
    uint32_t c = ctx->hash[2];
    uint32_t d = ctx->hash[3];
    uint32_t e = ctx->hash[4];
    uint32_t f = ctx->hash[5];
    uint32_t g = ctx->hash[6];
    uint32_t h = ctx->hash[7];
    
    // 64 rounds
    for (int i = 0; i < 64; i++) {
        uint32_t t1 = h + EP1(e) + CH(e, f, g) + K[i] + w[i];
        uint32_t t2 = EP0(a) + MAJ(a, b, c);
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }
    
    // Compute intermediate hash value
    ctx->hash[0] += a;
    ctx->hash[1] += b;
    ctx->hash[2] += c;
    ctx->hash[3] += d;
    ctx->hash[4] += e;
    ctx->hash[5] += f;
    ctx->hash[6] += g;
    ctx->hash[7] += h;
}

// Start SHA256
inline int mbedtls_sha256_starts_ret(mbedtls_sha256_context* ctx, int is224) {
    (void)is224; // We only implement SHA256 (not SHA224) for simplicity
    
    if (!ctx) return -1;
    
    // Initialize with IV
    for (int i = 0; i < 8; i++) {
        ctx->hash[i] = init_hash[i];
    }
    ctx->total_len = 0;
    ctx->buffer_len = 0;
    return 0;
}

// Update with data
inline int mbedtls_sha256_update_ret(mbedtls_sha256_context* ctx, const uint8_t* input, size_t ilen) {
    if (!ctx || !input) return -1;
    
    size_t remaining = ilen;
    size_t offset = 0;
    
    while (remaining > 0) {
        // Copy to buffer
        size_t to_copy = (remaining < (size_t)(64 - ctx->buffer_len)) ? remaining : (size_t)(64 - ctx->buffer_len);
        memcpy(ctx->buffer + ctx->buffer_len, input + offset, to_copy);
        ctx->buffer_len += to_copy;
        offset += to_copy;
        remaining -= to_copy;
        
        // Process full block
        if (ctx->buffer_len == 64) {
            sha256_process_block(ctx);
            ctx->total_len += 512;
            ctx->buffer_len = 0;
        }
    }
    
    return 0;
}

// Finish and output hash
inline int mbedtls_sha256_finish_ret(mbedtls_sha256_context* ctx, uint8_t* output) {
    if (!ctx || !output) return -1;
    
    // Padding
    uint64_t bit_len = ctx->total_len + (ctx->buffer_len * 8);
    
    // Append '1' bit
    ctx->buffer[ctx->buffer_len++] = 0x80;
    
    // Pad with zeros until we have 56 bytes (to fit length)
    while (ctx->buffer_len < 56) {
        ctx->buffer[ctx->buffer_len++] = 0x00;
    }
    
    // Append length in bits as big-endian 64-bit
    for (int i = 7; i >= 0; i--) {
        ctx->buffer[55 + (7 - i)] = (bit_len >> (i * 8)) & 0xFF;
    }
    
    // Process final block(s)
    sha256_process_block(ctx);
    if (ctx->buffer_len > 0) {
        sha256_process_block(ctx);
    }
    
    // Output hash
    for (int i = 0; i < 8; i++) {
        output[i * 4] = (ctx->hash[i] >> 24) & 0xFF;
        output[i * 4 + 1] = (ctx->hash[i] >> 16) & 0xFF;
        output[i * 4 + 2] = (ctx->hash[i] >> 8) & 0xFF;
        output[i * 4 + 3] = ctx->hash[i] & 0xFF;
    }
    
    return 0;
}

#endif // MBEDTLS_SHA256_H_MOCK
