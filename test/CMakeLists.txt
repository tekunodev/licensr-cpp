add_executable(licensr_tests
    test_main.cpp
    test_base64url.cpp
    test_client.cpp
    test_conformance_live.cpp
    test_ed25519.cpp
    test_http_transport.cpp
    test_hwid.cpp
    test_json.cpp
    test_sha256.cpp
    test_singleton.cpp
    test_token_store.cpp
    conformance/yaml_lite.cpp
)

target_link_libraries(licensr_tests PRIVATE licensr::licensr)

# Tests reach into src/detail (e.g. the vendored crypto headers) rather
# than only the public include/ surface.
target_include_directories(licensr_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)

# Baked in at configure time so test_conformance_live.cpp can find
# contract/conformance.yaml regardless of the test binary's working
# directory at run time.
get_filename_component(LICENSR_CONTRACT_DIR "${CMAKE_SOURCE_DIR}/../../contract" ABSOLUTE)
target_compile_definitions(licensr_tests PRIVATE
    LICENSR_CONFORMANCE_YAML_PATH="${LICENSR_CONTRACT_DIR}/conformance.yaml"
)

add_test(NAME licensr_tests COMMAND licensr_tests)
