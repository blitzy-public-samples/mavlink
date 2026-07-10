# In-repo C runtime tests (CTest).
#
# Reached ONLY via the root CMakeLists.txt guarded `add_subdirectory(tests)`
# (i.e. when configured with -DMAVLINK_BUILD_TESTS=ON) -> tests/CMakeLists.txt
# -> add_subdirectory(c). C is already enabled by the root guard, and the
# parent-scope variables mavgen_EXECUTABLE, PYTHON_EXECUTABLE and
# MAVLINK_SOURCE_DIR are inherited from the root CMakeLists.txt.
#
# All artifacts (generated headers + the test executable) live in the CMake
# build tree; the source tree is never written to.

# Generate the common.xml C headers (wire protocol 2.0) into the build tree,
# mirroring the root generateMavlink macro convention. common.xml transitively
# includes standard.xml and minimal.xml, so HEARTBEAT (minimal) and
# GLOBAL_POSITION_INT (standard) are produced alongside the common messages.
set(MAVLINK_TEST_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/include/v2.0")
set(MAVLINK_TEST_COMMON_HEADER "${MAVLINK_TEST_GEN_DIR}/common/mavlink.h")
set(MAVLINK_TEST_COMMON_XML "${MAVLINK_SOURCE_DIR}/message_definitions/v1.0/common.xml")
# common.xml remains the SOLE generation input, but mavgen also reads the dialects
# it transitively includes (standard.xml -> minimal.xml). List them as explicit
# dependencies so the generated headers are correctly rebuilt whenever any of the
# three definition files change (accurate rebuild dependency closure).
set(MAVLINK_TEST_STANDARD_XML "${MAVLINK_SOURCE_DIR}/message_definitions/v1.0/standard.xml")
set(MAVLINK_TEST_MINIMAL_XML "${MAVLINK_SOURCE_DIR}/message_definitions/v1.0/minimal.xml")

add_custom_command(
    OUTPUT "${MAVLINK_TEST_COMMON_HEADER}"
    COMMAND ${mavgen_EXECUTABLE} --lang=C
                                 --wire-protocol=2.0
                                 --output=${MAVLINK_TEST_GEN_DIR}
                                 ${MAVLINK_TEST_COMMON_XML}
    DEPENDS "${MAVLINK_TEST_COMMON_XML}"
            "${MAVLINK_TEST_STANDARD_XML}"
            "${MAVLINK_TEST_MINIMAL_XML}"
    COMMENT "Generating common.xml C headers for the C round-trip tests"
)
add_custom_target(test_roundtrip_headers DEPENDS "${MAVLINK_TEST_COMMON_HEADER}")

# Build the round-trip test executable and register it with CTest.
add_executable(test_roundtrip test_roundtrip.c)
add_dependencies(test_roundtrip test_roundtrip_headers)
target_include_directories(test_roundtrip PRIVATE "${MAVLINK_TEST_GEN_DIR}")

add_test(NAME c_roundtrip COMMAND test_roundtrip)
