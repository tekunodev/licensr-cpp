#pragma once

#include <string>

#include "detail/json.h"

namespace licensr_conformance {

// Parses the narrow YAML subset used by contract/conformance.yaml — block
// mappings, block sequences (including sequences of mappings), quoted and
// bare scalars (string/int/bool/null inferred), inline flow sequences and
// the empty flow mapping (`[a, "b"]`, `{}`), and `>` folded block scalars
// (consumed for their indentation but not otherwise interpreted, since no
// conformance case asserts on `description`).
//
// Deliberately not a general YAML parser — mirrors the SDK core's own
// "minimal JSON scoped to known shapes, not a library" doctrine (see
// src/detail/json.h). Anything outside this subset throws
// std::runtime_error naming the offending line, so a future
// conformance.yaml edit that needs more of YAML fails loudly here instead
// of silently misparsing. Reuses licensr::detail::Json as the output tree
// rather than inventing a parallel value type.
licensr::detail::Json parse_yaml(const std::string& text);

}  // namespace licensr_conformance
