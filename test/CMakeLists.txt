add_executable(licensr_tests
    test_main.cpp
    test_base64url.cpp
    test_client.cpp
    test_ed25519.cpp
    test_http_transport.cpp
    test_json.cpp
    test_singleton.cpp
)

target_link_libraries(licensr_tests PRIVATE licensr::licensr)

# Tests reach into src/detail (e.g. the vendored crypto headers) rather
# than only the public include/ surface.
target_include_directories(licensr_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)

add_test(NAME licensr_tests COMMAND licensr_tests)
