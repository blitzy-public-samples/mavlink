# tests/c/CMakeLists.txt
#
# Build script for the MAVLink 2 C round-trip test (test_roundtrip.c).
#
# Responsibilities:
#   1. Generate the MAVLink 2 `common` C headers into this target's binary dir.
#   2. Add the include paths the generated headers require.
#   3. Build the `test_roundtrip` executable with the exact upstream flags.
#   4. Make the executable depend on header generation (headers-before-compile).
#   5. Register the executable with CTest under the name `roundtrip`.
#
# This file is reached ONLY when the root option MAVLINK_BUILD_TESTS=ON, via:
#   root CMakeLists.txt (if(MAVLINK_BUILD_TESTS) enable_testing(); add_subdirectory(tests))
#     -> tests/CMakeLists.txt (add_subdirectory(c)) -> THIS FILE
#
# Isolation contract: all generated headers land in ${CMAKE_CURRENT_BINARY_DIR}
# and are NEVER written into the source tree or committed. GCC only.

# C is already enabled by the top-level project(mavlink); calling enable_language(C)
# here is harmless and defensive. enable_testing() is intentionally NOT called here
# (the root CMakeLists.txt already calls it once, inside the MAVLINK_BUILD_TESTS guard).
enable_language(C)

# PYTHON_EXECUTABLE is normally provided by the root CMakeLists.txt
# (find_package(Python3 COMPONENTS Interpreter ...) -> set(PYTHON_EXECUTABLE ...)).
# Defensive fallback in case this subdirectory is ever configured in isolation.
if(NOT DEFINED PYTHON_EXECUTABLE OR PYTHON_EXECUTABLE STREQUAL "")
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    set(PYTHON_EXECUTABLE ${Python3_EXECUTABLE})
endif()

# Generate the MAVLink 2 `common` C headers into this target's binary directory.
# --wire-protocol=2.0 is REQUIRED: mavgen defaults to 1.0.
# The PYTHONPATH prefix makes the in-tree pymavlink submodule importable for
# `-m pymavlink.tools.mavgen`. ${CMAKE_SOURCE_DIR} is the repo root (this is a
# subdirectory, so ${CMAKE_CURRENT_SOURCE_DIR} would incorrectly point at tests/c).
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/common/mavlink.h
    COMMAND ${CMAKE_COMMAND} -E env "PYTHONPATH=$ENV{PYTHONPATH}:${CMAKE_SOURCE_DIR}"
            ${PYTHON_EXECUTABLE} -m pymavlink.tools.mavgen
            --lang=C --wire-protocol=2.0
            --output=${CMAKE_CURRENT_BINARY_DIR}
            ${CMAKE_SOURCE_DIR}/message_definitions/v1.0/common.xml
    COMMENT "Generating MAVLink 2 common headers for C round-trip test"
)
add_custom_target(generate_common_headers DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/common/mavlink.h)

# Shared headers (mavlink_types.h, checksum.h, mavlink_helpers.h, protocol.h, ...)
# live at the output root; the dialect headers (version.h, mavlink.h, common.h,
# mavlink_msg_*.h) live under common/. Both directories are needed on the include path.
include_directories(${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_BINARY_DIR}/common)

# Build the round-trip test executable. Flags are exactly the upstream harness flags
# (pymavlink/generator/C/test/posix/Makefile). -g -O0 keeps the binary debuggable.
add_executable(test_roundtrip test_roundtrip.c)
add_dependencies(test_roundtrip generate_common_headers)
target_compile_options(test_roundtrip PRIVATE
    -g -Wall -Werror -O0 -Wno-address-of-packed-member -Wno-unused-function)

# Register the executable with CTest. Run with: ctest --test-dir build -R roundtrip
add_test(NAME roundtrip COMMAND test_roundtrip)
