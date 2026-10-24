cmake_minimum_required(VERSION 3.21)
if(APPLE)
  project(licensr_cpp_sdk LANGUAGES CXX OBJCXX)
else()
  project(licensr_cpp_sdk LANGUAGES CXX)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(LICENSR_BUILD_TESTS "Build the Licensr C++ SDK test suite" ON)
option(LICENSR_BUILD_SHARED "Build licensr as a shared library instead of static" OFF)

# macOS universal builds: set -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" when
# configuring; no special handling needed here, CMake/Xcode/clang do the
# right thing for a plain static/shared library target.

# Selectable MSVC runtime (/MT vs /MD and their debug variants), so
# consumers building with the static CRT elsewhere in their DAW/plugin
# project can make Licensr match rather than fighting a runtime mismatch.
if(MSVC)
  set(LICENSR_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL" CACHE STRING
      "MSVC runtime library for the licensr target (see CMAKE_MSVC_RUNTIME_LIBRARY docs)")
  set(CMAKE_MSVC_RUNTIME_LIBRARY "${LICENSR_MSVC_RUNTIME_LIBRARY}")
endif()

set(LICENSR_HTTP_BACKEND "AUTO" CACHE STRING
    "HTTP transport backend: AUTO, WINHTTP, NSURLSESSION, CURL, or NONE (bring your own via HttpTransport)")
set_property(CACHE LICENSR_HTTP_BACKEND PROPERTY STRINGS AUTO WINHTTP NSURLSESSION CURL NONE)

if(LICENSR_HTTP_BACKEND STREQUAL "AUTO")
  if(WIN32)
    set(LICENSR_HTTP_BACKEND_RESOLVED "WINHTTP")
  elseif(APPLE)
    set(LICENSR_HTTP_BACKEND_RESOLVED "NSURLSESSION")
  else()
    set(LICENSR_HTTP_BACKEND_RESOLVED "CURL")
  endif()
else()
  set(LICENSR_HTTP_BACKEND_RESOLVED "${LICENSR_HTTP_BACKEND}")
endif()

set(LICENSR_SOURCES
    src/licensr.cpp
    src/http_transport.cpp
    src/detail/base64url.cpp
    src/detail/json.cpp
    src/detail/client.cpp
    src/detail/hwid_support.cpp
    src/detail/token_store.cpp
    src/detail/crypto/ed25519/ed25519_verify.cpp
    src/detail/crypto/ed25519/fe.cpp
    src/detail/crypto/ed25519/ge.cpp
    src/detail/crypto/ed25519/sc.cpp
    src/detail/crypto/ed25519/sha512.cpp
    src/detail/crypto/sha256.cpp
)

if(LICENSR_HTTP_BACKEND_RESOLVED STREQUAL "WINHTTP")
  list(APPEND LICENSR_SOURCES src/detail/http/http_transport_winhttp.cpp)
elseif(LICENSR_HTTP_BACKEND_RESOLVED STREQUAL "NSURLSESSION")
  list(APPEND LICENSR_SOURCES src/detail/http/http_transport_nsurlsession.mm)
elseif(LICENSR_HTTP_BACKEND_RESOLVED STREQUAL "CURL")
  list(APPEND LICENSR_SOURCES src/detail/http/http_transport_curl.cpp)
endif()

if(WIN32)
  list(APPEND LICENSR_SOURCES src/hwid_windows.cpp)
elseif(APPLE)
  list(APPEND LICENSR_SOURCES src/hwid_macos.cpp)
else()
  list(APPEND LICENSR_SOURCES src/hwid_linux.cpp)
endif()

if(LICENSR_BUILD_SHARED)
  add_library(licensr SHARED ${LICENSR_SOURCES})
  target_compile_definitions(licensr PUBLIC LICENSR_BUILD_SHARED)
else()
  add_library(licensr STATIC ${LICENSR_SOURCES})
endif()

add_library(licensr::licensr ALIAS licensr)

target_compile_definitions(licensr PRIVATE LICENSR_BUILDING)

# Public headers use std::optional, so consumers need C++17 too.
target_compile_features(licensr PUBLIC cxx_std_17)

target_include_directories(licensr
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Hidden-by-default visibility: see include/licensr/export.h for why this
# matters for plugins that get loaded alongside other Licensr-linked
# plugins in the same host process.
set_target_properties(licensr PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
    POSITION_INDEPENDENT_CODE ON
)

if(MSVC)
  target_compile_options(licensr PRIVATE /W4)
else()
  target_compile_options(licensr PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(LICENSR_HTTP_BACKEND_RESOLVED STREQUAL "WINHTTP")
  target_compile_definitions(licensr PRIVATE LICENSR_HTTP_WINHTTP)
  target_link_libraries(licensr PRIVATE winhttp)
elseif(LICENSR_HTTP_BACKEND_RESOLVED STREQUAL "NSURLSESSION")
  target_compile_definitions(licensr PRIVATE LICENSR_HTTP_NSURLSESSION)
  find_library(FOUNDATION_LIBRARY Foundation REQUIRED)
  target_link_libraries(licensr PRIVATE ${FOUNDATION_LIBRARY})
  set_source_files_properties(src/detail/http/http_transport_nsurlsession.mm PROPERTIES
      COMPILE_FLAGS "-fobjc-arc"
  )
elseif(LICENSR_HTTP_BACKEND_RESOLVED STREQUAL "CURL")
  target_compile_definitions(licensr PRIVATE LICENSR_HTTP_CURL)
  find_package(CURL REQUIRED)
  target_link_libraries(licensr PRIVATE CURL::libcurl)
elseif(NOT LICENSR_HTTP_BACKEND_RESOLVED STREQUAL "NONE")
  message(FATAL_ERROR "Unknown LICENSR_HTTP_BACKEND: ${LICENSR_HTTP_BACKEND_RESOLVED}")
endif()

# HWID: IOKit/CoreFoundation for IOPlatformUUID (macOS), advapi32 for the
# registry-backed MachineGuid lookup (Windows). Linux's /etc/machine-id
# needs no extra library.
if(APPLE)
  find_library(IOKIT_LIBRARY IOKit REQUIRED)
  find_library(COREFOUNDATION_LIBRARY CoreFoundation REQUIRED)
  target_link_libraries(licensr PRIVATE ${IOKIT_LIBRARY} ${COREFOUNDATION_LIBRARY})
elseif(WIN32)
  target_link_libraries(licensr PRIVATE advapi32)
endif()

message(STATUS "Licensr C++ SDK: HTTP backend = ${LICENSR_HTTP_BACKEND_RESOLVED}")

if(LICENSR_BUILD_TESTS)
  enable_testing()
  add_subdirectory(test)
endif()
