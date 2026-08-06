# Opt-in JUCE integration module — NOT referenced by sdks/cpp/CMakeLists.txt
# and not part of the `licensr` core target, on purpose: JUCE ships under
# AGPLv3-or-commercial terms, and folding a JUCE dependency into the core
# would push that licensing decision onto every consumer, including the
# many plugin/game/desktop hosts that have nothing to do with JUCE.
#
# Add this subdirectory yourself, after your project has already loaded
# JUCE's CMake support and the `licensr` core target:
#
#   add_subdirectory(path/to/JUCE)
#   add_subdirectory(path/to/licensr/sdks/cpp)
#   add_subdirectory(path/to/licensr/sdks/cpp/juce)
#   target_link_libraries(YourPluginTarget PRIVATE licensr_juce)
#
# See sdks/cpp/juce/README.md for a full walkthrough.
cmake_minimum_required(VERSION 3.21)

if(NOT COMMAND juce_add_module)
  message(FATAL_ERROR
      "sdks/cpp/juce requires JUCE's CMake support to already be loaded — "
      "add_subdirectory(path/to/JUCE) before add_subdirectory(sdks/cpp/juce).")
endif()

if(NOT TARGET licensr::licensr)
  message(FATAL_ERROR
      "sdks/cpp/juce requires the licensr core target — "
      "add_subdirectory(path/to/sdks/cpp) before add_subdirectory(sdks/cpp/juce).")
endif()

juce_add_module(${CMAKE_CURRENT_SOURCE_DIR}/licensr_juce)

target_link_libraries(licensr_juce INTERFACE licensr::licensr)
