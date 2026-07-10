# MAVLink In-Repo Test Suite

## Overview

This directory contains a small, self-contained test suite that validates the MAVLink definition → generation → runtime pipeline across three layers: (1) static validation of the XML message definitions under `message_definitions/v1.0/` (`tests/python/test_xml_definitions.py`); (2) an integration test that generates the `common.xml` C headers and compiles them with `gcc -Wall` (`tests/python/test_generate_common.py`); and (3) C runtime round-trip tests plus a single CRC-rejection check against the generated headers (`tests/c/test_roundtrip.c`, registered with CTest via `tests/c/CMakeLists.txt`). The tests are fully isolated: every generated artifact is written to a temporary directory (Python) or the CMake build tree (C), and pytest's cache and temporary files are redirected outside the repository, so running the suite leaves the source tree unmodified and the working tree git-clean.

## Prerequisites

- Python 3.11 with `pytest` (7.4.4). The generator also requires `future` and `lxml`. For example: `pip install pytest==7.4.4 future lxml`.
  - `pytest` is pinned to `7.4.4` to honor the pinned `pymavlink` submodule's `pytest <= 7.4.4` constraint. Because that release predates the fix (pytest 9.0.3) for CVE-2025-71176 (CWE-379, a predictable-`/tmp` temp-directory issue), the suite runs `pytest` under a private, unpredictable temporary root: the test suite's `conftest.py` sets `PYTEST_DEBUG_TEMPROOT` automatically when it is not already set, and the CI workflow sets it explicitly, so both local and CI runs avoid the predictable shared `/tmp/pytest-of-<user>` path.
- A C toolchain for the C tests: `gcc`, plus `cmake` and `ctest` (CMake ≥ 3.x). On `ubuntu-latest` these are preinstalled.
- Initialized git submodules so the pinned `pymavlink` generator is present: `git submodule update --init --recursive`.

## Running the Python tests

Run the Python suite (XML definition validation and the generation/compile integration test) from the repository root:

```bash
python -m pytest tests/python -v
```

To run a single module or filter to specific cases, pass a path and/or a `-k` expression:

```bash
python -m pytest tests/python/test_xml_definitions.py -k common -v
```

The generation test in `test_generate_common.py` is skipped automatically when `gcc` is not on the `PATH`, so it does not hard-fail on machines without a C compiler.

## Running the C tests

Build and run the C round-trip tests with CMake and CTest from the repository root:

```bash
cmake -S . -B build -DMAVLINK_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

The C tests build only when `-DMAVLINK_BUILD_TESTS=ON` is set (the option defaults to `OFF`), so default builds and installs are unaffected.
