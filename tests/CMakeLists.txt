# MAVLink in-repo test suite aggregator.
#
# CTest is switched on exactly once at the top-level CMakeLists.txt, inside the
# if(MAVLINK_BUILD_TESTS) guard. Performing the test-enable step at the source
# root (rather than in this subdirectory) is what lets `ctest --test-dir build`,
# invoked from the build root, discover the registered C tests. It is therefore
# intentionally NOT repeated in this file.
#
# Only the C layer is a CMake subdirectory: the scripting-language test layers
# run under their own standalone runner and are not built by CMake. This
# aggregator therefore descends solely into c/.
add_subdirectory(c)
