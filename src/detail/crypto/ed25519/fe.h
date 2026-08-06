// Vendored from orlp/ed25519 (https://github.com/orlp/ed25519), itself a
// portable rewrite of the SUPERCOP "ref10" Ed25519 reference implementation.
// zlib-licensed — see THIRD_PARTY_NOTICES.md at the sdks/cpp root. Trimmed to
// what verification needs; see sdks/cpp/README.md's "Vendored crypto" section.
#pragma once

#include <cstdint>

namespace licensr::detail::ed25519_ref10 {

// fe means field element.
// Here the field is \Z/(2^255-19).
// An element t, entries t[0]...t[9], represents the integer
// t[0]+2^26 t[1]+2^51 t[2]+2^77 t[3]+2^102 t[4]+...+2^230 t[9].
// Bounds on each t[i] vary depending on context.
using fe = std::int32_t[10];

void fe_0(fe h);
void fe_1(fe h);

void fe_frombytes(fe h, const unsigned char* s);
void fe_tobytes(unsigned char* s, const fe h);

void fe_copy(fe h, const fe f);
int fe_isnegative(const fe f);
int fe_isnonzero(const fe f);
void fe_cmov(fe f, const fe g, unsigned int b);

void fe_neg(fe h, const fe f);
void fe_add(fe h, const fe f, const fe g);
void fe_invert(fe out, const fe z);
void fe_sq(fe h, const fe f);
void fe_sq2(fe h, const fe f);
void fe_mul(fe h, const fe f, const fe g);
void fe_pow22523(fe out, const fe z);
void fe_sub(fe h, const fe f, const fe g);

}  // namespace licensr::detail::ed25519_ref10
