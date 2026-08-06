/*******************************************************************************
 The block below describes the properties of this module, and is read by
 the Projucer to automatically generate project code that uses it. See the
 JUCE Module Format.md file for details.

 BEGIN_JUCE_MODULE_DECLARATION

  ID:                 licensr_juce
  vendor:             licensr
  version:            1.0.0
  name:               Licensr license activation UI for JUCE
  description:        Drop-in JUCE Component for activating and validating Licensr licenses.
  website:            https://licensr.app
  license:            MIT
  minimumCppStandard:  17

  dependencies:       juce_gui_basics

 END_JUCE_MODULE_DECLARATION

*******************************************************************************/

// This module is deliberately separate from sdks/cpp's core `licensr`
// target (see sdks/cpp/README.md): JUCE itself ships under
// AGPLv3-or-commercial terms, and a JUCE dependency baked into the core
// would force that licensing decision onto every consumer of the SDK,
// including the many plugin hosts that have nothing to do with JUCE. This
// module is MIT-licensed like the core, but only ever gets compiled into
// projects that have already made their own JUCE licensing decision — it
// adds nothing new to that decision.
#pragma once
#define LICENSR_JUCE_H_INCLUDED

#include <juce_gui_basics/juce_gui_basics.h>
#include <licensr/licensr.h>

#include "components/LicenseActivationComponent.h"
