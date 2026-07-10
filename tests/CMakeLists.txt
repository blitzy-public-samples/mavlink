# In-repo test suite entry point.
# Reached from the root CMakeLists.txt via the guarded `add_subdirectory(tests)`
# (only when configured with -DMAVLINK_BUILD_TESTS=ON). C is already enabled by
# that guard, and mavgen_EXECUTABLE / PYTHON_EXECUTABLE / MAVLINK_SOURCE_DIR are
# in scope from the parent. This thin forwarder simply descends into the C tests.
add_subdirectory(c)
