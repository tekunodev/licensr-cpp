/*
  When you add this cpp file to your project, you mustn't include it in a
  file where you've already included any other headers — put it inside a
  file on its own, possibly with your config flags preceding it. This
  mirrors every other JUCE module's *.cpp unity-build convention.
*/
#ifdef LICENSR_JUCE_H_INCLUDED
#error "Incorrect use of JUCE cpp file"
#endif

#include "licensr_juce.h"

#include "components/LicenseActivationComponent.cpp"
