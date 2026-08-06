// Symbol visibility macros.
//
// The core is built with hidden visibility by default (see CMakeLists.txt:
// CXX_VISIBILITY_PRESET hidden, VISIBILITY_INLINES_HIDDEN ON). This matters
// for audio plugins specifically: two independently-built plugins that both
// statically link Licensr can end up loaded into the same DAW process as
// separate dylib/so/dll bundles. If Licensr's internal symbols had default
// (exported) visibility, a platform dynamic linker could interpose one
// plugin's copy of e.g. `fe_mul` or the process-wide singleton onto the
// other's, causing state corruption between two unrelated plugin instances.
// Hidden visibility keeps every symbol private to the bundle that linked it.
//
// LICENSR_API marks the handful of entry points a consumer actually calls.
// It is a no-op for the default static-library build (hidden visibility
// already makes them link-time-only within the consumer's own binary) and
// only does real work if a consumer opts into building Licensr as a shared
// library via -DLICENSR_BUILD_SHARED=ON.
#pragma once

#if defined(LICENSR_BUILD_SHARED)
#if defined(_WIN32)
#if defined(LICENSR_BUILDING)
#define LICENSR_API __declspec(dllexport)
#else
#define LICENSR_API __declspec(dllimport)
#endif
#else
#define LICENSR_API __attribute__((visibility("default")))
#endif
#else
#define LICENSR_API
#endif
