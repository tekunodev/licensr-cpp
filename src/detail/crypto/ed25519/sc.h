// Vendored from orlp/ed25519 (https://github.com/orlp/ed25519), itself a
// portable rewrite of the SUPERCOP "ref10" Ed25519 reference implementation.
// zlib-licensed — see THIRD_PARTY_NOTICES.md at the sdks/cpp root.
//
// Trimmed for verify-only use: sc_muladd (signing-only) is omitted.
#pragma once

namespace licensr::detail::ed25519_ref10 {

// The set of scalars is \Z/l
// where l = 2^252 + 27742317777372353535851937790883648493.
void sc_reduce(unsigned char* s);

}  // namespace licensr::detail::ed25519_ref10
