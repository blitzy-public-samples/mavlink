# In-repo test suite aggregator.
#
# Reached only when the root option MAVLINK_BUILD_TESTS=ON, via the root
# CMakeLists.txt guard (if(MAVLINK_BUILD_TESTS) enable_testing(); add_subdirectory(tests)).
# The C round-trip test lives under c/; its CMakeLists.txt generates the
# MAVLink 2 common headers into the binary directory and registers the test
# with CTest. The Python layers run directly under pytest and need no CMake.
add_subdirectory(c)
