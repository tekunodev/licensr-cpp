# Third-party notices

## orlp/ed25519 (Ed25519 field/group arithmetic, scalar reduction)

Files under `src/detail/crypto/ed25519/{fe,ge,sc}.{h,cpp}`, `precomp_data.h`,
and the verification logic in `ed25519_verify.cpp` are adapted from
[orlp/ed25519](https://github.com/orlp/ed25519), itself a portable C
rewrite of the SUPERCOP "ref10" Ed25519 reference implementation.

Trimmed to verify-only (see the header comment of each file for exactly
what was removed): the `base[32][8]` precomputed table and
`ge_scalarmult_base`/`ge_p3_tobytes`/`sc_muladd` used only for *signing*
are dropped, since Licensr's SDKs only ever verify signatures — private
keys never leave the license server. Ported from C to a `licensr::detail`
C++ namespace (functions, structs); no algorithmic changes.

zlib License:

```
Copyright (c) 2015 Orson Peters <orsonpeters@gmail.com>

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
```

## LibTomCrypt SHA-512 (via orlp/ed25519)

`src/detail/crypto/ed25519/sha512.{h,cpp}` is adapted from the SHA-512
implementation in [LibTomCrypt](https://github.com/libtom/libtomcrypt) by
Tom St Denis, as vendored by orlp/ed25519. LibTomCrypt is public domain /
WTFPL ("free for all purposes without any express guarantee it works").
Ported from C to the `licensr::detail::ed25519_ref10` C++ namespace; no
algorithmic changes.
