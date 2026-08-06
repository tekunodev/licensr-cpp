// Vendored from orlp/ed25519 (https://github.com/orlp/ed25519), which in
// turn vendors this SHA-512 implementation from LibTomCrypt (Tom St Denis),
// public domain / WTFPL. See THIRD_PARTY_NOTICES.md at the sdks/cpp root.
#include "sha512.h"

namespace licensr::detail::ed25519_ref10 {

namespace {

constexpr std::uint64_t K[80] = {
    0x428a2f98d728ae22ULL, 0x7137449123ef65cdULL, 0xb5c0fbcfec4d3b2fULL, 0xe9b5dba58189dbbcULL,
    0x3956c25bf348b538ULL, 0x59f111f1b605d019ULL, 0x923f82a4af194f9bULL, 0xab1c5ed5da6d8118ULL,
    0xd807aa98a3030242ULL, 0x12835b0145706fbeULL, 0x243185be4ee4b28cULL, 0x550c7dc3d5ffb4e2ULL,
    0x72be5d74f27b896fULL, 0x80deb1fe3b1696b1ULL, 0x9bdc06a725c71235ULL, 0xc19bf174cf692694ULL,
    0xe49b69c19ef14ad2ULL, 0xefbe4786384f25e3ULL, 0x0fc19dc68b8cd5b5ULL, 0x240ca1cc77ac9c65ULL,
    0x2de92c6f592b0275ULL, 0x4a7484aa6ea6e483ULL, 0x5cb0a9dcbd41fbd4ULL, 0x76f988da831153b5ULL,
    0x983e5152ee66dfabULL, 0xa831c66d2db43210ULL, 0xb00327c898fb213fULL, 0xbf597fc7beef0ee4ULL,
    0xc6e00bf33da88fc2ULL, 0xd5a79147930aa725ULL, 0x06ca6351e003826fULL, 0x142929670a0e6e70ULL,
    0x27b70a8546d22ffcULL, 0x2e1b21385c26c926ULL, 0x4d2c6dfc5ac42aedULL, 0x53380d139d95b3dfULL,
    0x650a73548baf63deULL, 0x766a0abb3c77b2a8ULL, 0x81c2c92e47edaee6ULL, 0x92722c851482353bULL,
    0xa2bfe8a14cf10364ULL, 0xa81a664bbc423001ULL, 0xc24b8b70d0f89791ULL, 0xc76c51a30654be30ULL,
    0xd192e819d6ef5218ULL, 0xd69906245565a910ULL, 0xf40e35855771202aULL, 0x106aa07032bbd1b8ULL,
    0x19a4c116b8d2d0c8ULL, 0x1e376c085141ab53ULL, 0x2748774cdf8eeb99ULL, 0x34b0bcb5e19b48a8ULL,
    0x391c0cb3c5c95a63ULL, 0x4ed8aa4ae3418acbULL, 0x5b9cca4f7763e373ULL, 0x682e6ff3d6b2b8a3ULL,
    0x748f82ee5defb2fcULL, 0x78a5636f43172f60ULL, 0x84c87814a1f0ab72ULL, 0x8cc702081a6439ecULL,
    0x90befffa23631e28ULL, 0xa4506cebde82bde9ULL, 0xbef9a3f7b2c67915ULL, 0xc67178f2e372532bULL,
    0xca273eceea26619cULL, 0xd186b8c721c0c207ULL, 0xeada7dd6cde0eb1eULL, 0xf57d4f7fee6ed178ULL,
    0x06f067aa72176fbaULL, 0x0a637dc5a2c898a6ULL, 0x113f9804bef90daeULL, 0x1b710b35131c471bULL,
    0x28db77f523047d84ULL, 0x32caab7b40c72493ULL, 0x3c9ebe0a15c9bebcULL, 0x431d67c49c100d4cULL,
    0x4cc5d4becb3e42b6ULL, 0x597f299cfc657e2aULL, 0x5fcb6fab3ad6faecULL, 0x6c44198c4a475817ULL,
};

inline std::uint64_t ror64(std::uint64_t x, std::uint64_t y) { return (x >> (y & 63)) | (x << (64 - (y & 63))); }

inline void store64h(std::uint64_t x, unsigned char* y) {
  y[0] = (unsigned char)((x >> 56) & 255);
  y[1] = (unsigned char)((x >> 48) & 255);
  y[2] = (unsigned char)((x >> 40) & 255);
  y[3] = (unsigned char)((x >> 32) & 255);
  y[4] = (unsigned char)((x >> 24) & 255);
  y[5] = (unsigned char)((x >> 16) & 255);
  y[6] = (unsigned char)((x >> 8) & 255);
  y[7] = (unsigned char)(x & 255);
}

inline std::uint64_t load64h(const unsigned char* y) {
  return (((std::uint64_t)(y[0] & 255)) << 56) | (((std::uint64_t)(y[1] & 255)) << 48) |
         (((std::uint64_t)(y[2] & 255)) << 40) | (((std::uint64_t)(y[3] & 255)) << 32) |
         (((std::uint64_t)(y[4] & 255)) << 24) | (((std::uint64_t)(y[5] & 255)) << 16) |
         (((std::uint64_t)(y[6] & 255)) << 8) | ((std::uint64_t)(y[7] & 255));
}

inline std::uint64_t ch(std::uint64_t x, std::uint64_t y, std::uint64_t z) { return z ^ (x & (y ^ z)); }
inline std::uint64_t maj(std::uint64_t x, std::uint64_t y, std::uint64_t z) {
  return ((x | y) & z) | (x & y);
}
inline std::uint64_t sigma0(std::uint64_t x) { return ror64(x, 28) ^ ror64(x, 34) ^ ror64(x, 39); }
inline std::uint64_t sigma1(std::uint64_t x) { return ror64(x, 14) ^ ror64(x, 18) ^ ror64(x, 41); }
inline std::uint64_t gamma0(std::uint64_t x) { return ror64(x, 1) ^ ror64(x, 8) ^ (x >> 7); }
inline std::uint64_t gamma1(std::uint64_t x) { return ror64(x, 19) ^ ror64(x, 61) ^ (x >> 6); }

// Compress 1024 bits.
int sha512_compress(sha512_context* md, const unsigned char* buf) {
  std::uint64_t S[8];
  std::uint64_t W[80];
  std::uint64_t t0;
  std::uint64_t t1;
  int i;

  for (i = 0; i < 8; i++) {
    S[i] = md->state[i];
  }

  for (i = 0; i < 16; i++) {
    W[i] = load64h(buf + (8 * i));
  }

  for (i = 16; i < 80; i++) {
    W[i] = gamma1(W[i - 2]) + W[i - 7] + gamma0(W[i - 15]) + W[i - 16];
  }

#define RND(a, b, c, d, e, f, g, h, i)                                    \
  t0 = h + sigma1(e) + ch(e, f, g) + K[i] + W[i];                        \
  t1 = sigma0(a) + maj(a, b, c);                                          \
  d += t0;                                                                \
  h = t0 + t1;

  for (i = 0; i < 80; i += 8) {
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

int sha512_init(sha512_context* md) {
  if (md == nullptr) return 1;

  md->curlen = 0;
  md->length = 0;
  md->state[0] = 0x6a09e667f3bcc908ULL;
  md->state[1] = 0xbb67ae8584caa73bULL;
  md->state[2] = 0x3c6ef372fe94f82bULL;
  md->state[3] = 0xa54ff53a5f1d36f1ULL;
  md->state[4] = 0x510e527fade682d1ULL;
  md->state[5] = 0x9b05688c2b3e6c1fULL;
  md->state[6] = 0x1f83d9abfb41bd6bULL;
  md->state[7] = 0x5be0cd19137e2179ULL;

  return 0;
}

int sha512_update(sha512_context* md, const unsigned char* in, std::size_t inlen) {
  std::size_t n;
  std::size_t i;
  int err;
  if (md == nullptr) return 1;
  if (in == nullptr) return 1;
  if (md->curlen > sizeof(md->buf)) {
    return 1;
  }
  while (inlen > 0) {
    if (md->curlen == 0 && inlen >= 128) {
      if ((err = sha512_compress(md, in)) != 0) {
        return err;
      }
      md->length += 128 * 8;
      in += 128;
      inlen -= 128;
    } else {
      n = inlen < (128 - md->curlen) ? inlen : (128 - md->curlen);

      for (i = 0; i < n; i++) {
        md->buf[i + md->curlen] = in[i];
      }

      md->curlen += n;
      in += n;
      inlen -= n;
      if (md->curlen == 128) {
        if ((err = sha512_compress(md, md->buf)) != 0) {
          return err;
        }
        md->length += 8 * 128;
        md->curlen = 0;
      }
    }
  }
  return 0;
}

int sha512_final(sha512_context* md, unsigned char* out) {
  int i;

  if (md == nullptr) return 1;
  if (out == nullptr) return 1;

  if (md->curlen >= sizeof(md->buf)) {
    return 1;
  }

  md->length += md->curlen * std::uint64_t{8};

  md->buf[md->curlen++] = (unsigned char)0x80;

  // If the length is currently above 112 bytes we append zeros then
  // compress, then fall back to padding zeros and length encoding as
  // usual.
  if (md->curlen > 112) {
    while (md->curlen < 128) {
      md->buf[md->curlen++] = (unsigned char)0;
    }
    sha512_compress(md, md->buf);
    md->curlen = 0;
  }

  // Pad up to 120 bytes of zeroes. From 112 to 120 is the 64 MSB of the
  // length; we assume you won't hash > 2^64 bits of data.
  while (md->curlen < 120) {
    md->buf[md->curlen++] = (unsigned char)0;
  }

  store64h(md->length, md->buf + 120);
  sha512_compress(md, md->buf);

  for (i = 0; i < 8; i++) {
    store64h(md->state[i], out + (8 * i));
  }

  return 0;
}

int sha512(const unsigned char* message, std::size_t message_len, unsigned char* out) {
  sha512_context ctx;
  int ret;
  if ((ret = sha512_init(&ctx))) return ret;
  if ((ret = sha512_update(&ctx, message, message_len))) return ret;
  if ((ret = sha512_final(&ctx, out))) return ret;
  return 0;
}

}  // namespace licensr::detail::ed25519_ref10
