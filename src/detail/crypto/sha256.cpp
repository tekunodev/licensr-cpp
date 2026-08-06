// Vendored from LibTomCrypt (Tom St Denis), public domain / WTFPL. See
// sha256.h for why this lives outside crypto/ed25519/.
#include "sha256.h"

namespace licensr::detail {

namespace {

constexpr std::uint32_t K[64] = {
    0x428a2f98UL, 0x71374491UL, 0xb5c0fbcfUL, 0xe9b5dba5UL, 0x3956c25bUL, 0x59f111f1UL, 0x923f82a4UL, 0xab1c5ed5UL,
    0xd807aa98UL, 0x12835b01UL, 0x243185beUL, 0x550c7dc3UL, 0x72be5d74UL, 0x80deb1feUL, 0x9bdc06a7UL, 0xc19bf174UL,
    0xe49b69c1UL, 0xefbe4786UL, 0x0fc19dc6UL, 0x240ca1ccUL, 0x2de92c6fUL, 0x4a7484aaUL, 0x5cb0a9dcUL, 0x76f988daUL,
    0x983e5152UL, 0xa831c66dUL, 0xb00327c8UL, 0xbf597fc7UL, 0xc6e00bf3UL, 0xd5a79147UL, 0x06ca6351UL, 0x14292967UL,
    0x27b70a85UL, 0x2e1b2138UL, 0x4d2c6dfcUL, 0x53380d13UL, 0x650a7354UL, 0x766a0abbUL, 0x81c2c92eUL, 0x92722c85UL,
    0xa2bfe8a1UL, 0xa81a664bUL, 0xc24b8b70UL, 0xc76c51a3UL, 0xd192e819UL, 0xd6990624UL, 0xf40e3585UL, 0x106aa070UL,
    0x19a4c116UL, 0x1e376c08UL, 0x2748774cUL, 0x34b0bcb5UL, 0x391c0cb3UL, 0x4ed8aa4aUL, 0x5b9cca4fUL, 0x682e6ff3UL,
    0x748f82eeUL, 0x78a5636fUL, 0x84c87814UL, 0x8cc70208UL, 0x90befffaUL, 0xa4506cebUL, 0xbef9a3f7UL, 0xc67178f2UL,
};

inline std::uint32_t ror32(std::uint32_t x, std::uint32_t y) { return (x >> (y & 31)) | (x << (32 - (y & 31))); }

inline void store32h(std::uint32_t x, unsigned char* y) {
  y[0] = (unsigned char)((x >> 24) & 255);
  y[1] = (unsigned char)((x >> 16) & 255);
  y[2] = (unsigned char)((x >> 8) & 255);
  y[3] = (unsigned char)(x & 255);
}

inline std::uint32_t load32h(const unsigned char* y) {
  return (((std::uint32_t)(y[0] & 255)) << 24) | (((std::uint32_t)(y[1] & 255)) << 16) |
         (((std::uint32_t)(y[2] & 255)) << 8) | ((std::uint32_t)(y[3] & 255));
}

inline std::uint32_t ch(std::uint32_t x, std::uint32_t y, std::uint32_t z) { return z ^ (x & (y ^ z)); }
inline std::uint32_t maj(std::uint32_t x, std::uint32_t y, std::uint32_t z) { return ((x | y) & z) | (x & y); }
inline std::uint32_t sigma0(std::uint32_t x) { return ror32(x, 2) ^ ror32(x, 13) ^ ror32(x, 22); }
inline std::uint32_t sigma1(std::uint32_t x) { return ror32(x, 6) ^ ror32(x, 11) ^ ror32(x, 25); }
inline std::uint32_t gamma0(std::uint32_t x) { return ror32(x, 7) ^ ror32(x, 18) ^ (x >> 3); }
inline std::uint32_t gamma1(std::uint32_t x) { return ror32(x, 17) ^ ror32(x, 19) ^ (x >> 10); }

// Compress 512 bits.
int sha256_compress(sha256_context* md, const unsigned char* buf) {
  std::uint32_t S[8];
  std::uint32_t W[64];
  std::uint32_t t0;
  std::uint32_t t1;
  int i;

  for (i = 0; i < 8; i++) {
    S[i] = md->state[i];
  }

  for (i = 0; i < 16; i++) {
    W[i] = load32h(buf + (4 * i));
  }

  for (i = 16; i < 64; i++) {
    W[i] = gamma1(W[i - 2]) + W[i - 7] + gamma0(W[i - 15]) + W[i - 16];
  }

#define RND(a, b, c, d, e, f, g, h, i)                    \
  t0 = h + sigma1(e) + ch(e, f, g) + K[i] + W[i];         \
  t1 = sigma0(a) + maj(a, b, c);                          \
  d += t0;                                                \
  h = t0 + t1;

  for (i = 0; i < 64; i += 8) {
    RND(S[0], S[1], S[2], S[3], S[4], S[5], S[6], S[7], i + 0);
    RND(S[7], S[0], S[1], S[2], S[3], S[4], S[5], S[6], i + 1);
    RND(S[6], S[7], S[0], S[1], S[2], S[3], S[4], S[5], i + 2);
    RND(S[5], S[6], S[7], S[0], S[1], S[2], S[3], S[4], i + 3);
    RND(S[4], S[5], S[6], S[7], S[0], S[1], S[2], S[3], i + 4);
    RND(S[3], S[4], S[5], S[6], S[7], S[0], S[1], S[2], i + 5);
    RND(S[2], S[3], S[4], S[5], S[6], S[7], S[0], S[1], i + 6);
    RND(S[1], S[2], S[3], S[4], S[5], S[6], S[7], S[0], i + 7);
  }

#undef RND

  for (i = 0; i < 8; i++) {
    md->state[i] = md->state[i] + S[i];
  }

  return 0;
}

}  // namespace

int sha256_init(sha256_context* md) {
  if (md == nullptr) return 1;

  md->curlen = 0;
  md->length = 0;
  md->state[0] = 0x6a09e667UL;
  md->state[1] = 0xbb67ae85UL;
  md->state[2] = 0x3c6ef372UL;
  md->state[3] = 0xa54ff53aUL;
  md->state[4] = 0x510e527fUL;
  md->state[5] = 0x9b05688cUL;
  md->state[6] = 0x1f83d9abUL;
  md->state[7] = 0x5be0cd19UL;

  return 0;
}

int sha256_update(sha256_context* md, const unsigned char* in, std::size_t inlen) {
  std::size_t n;
  std::size_t i;
  int err;
  if (md == nullptr) return 1;
  if (in == nullptr) return 1;
  if (md->curlen > sizeof(md->buf)) {
    return 1;
  }
  while (inlen > 0) {
    if (md->curlen == 0 && inlen >= 64) {
      if ((err = sha256_compress(md, in)) != 0) {
        return err;
      }
      md->length += 64 * 8;
      in += 64;
      inlen -= 64;
    } else {
      n = inlen < (64 - md->curlen) ? inlen : (64 - md->curlen);

      for (i = 0; i < n; i++) {
        md->buf[i + md->curlen] = in[i];
      }

      md->curlen += n;
      in += n;
      inlen -= n;
      if (md->curlen == 64) {
        if ((err = sha256_compress(md, md->buf)) != 0) {
          return err;
        }
        md->length += 8 * 64;
        md->curlen = 0;
      }
    }
  }
  return 0;
}

int sha256_final(sha256_context* md, unsigned char* out) {
  int i;

  if (md == nullptr) return 1;
  if (out == nullptr) return 1;

  if (md->curlen >= sizeof(md->buf)) {
    return 1;
  }

  md->length += md->curlen * std::uint64_t{8};

  md->buf[md->curlen++] = (unsigned char)0x80;

  // If the length is currently above 56 bytes we append zeros then
  // compress, then fall back to padding zeros and length encoding as usual.
  if (md->curlen > 56) {
    while (md->curlen < 64) {
      md->buf[md->curlen++] = (unsigned char)0;
    }
    sha256_compress(md, md->buf);
    md->curlen = 0;
  }

  // Pad up to 56 bytes of zeroes.
  while (md->curlen < 56) {
    md->buf[md->curlen++] = (unsigned char)0;
  }

  store32h((std::uint32_t)(md->length >> 32), md->buf + 56);
  store32h((std::uint32_t)md->length, md->buf + 60);
  sha256_compress(md, md->buf);

  for (i = 0; i < 8; i++) {
    store32h(md->state[i], out + (4 * i));
  }

  return 0;
}

int sha256(const unsigned char* message, std::size_t message_len, unsigned char* out) {
  sha256_context ctx;
  int ret;
  if ((ret = sha256_init(&ctx))) return ret;
  if ((ret = sha256_update(&ctx, message, message_len))) return ret;
  if ((ret = sha256_final(&ctx, out))) return ret;
  return 0;
}

}  // namespace licensr::detail
