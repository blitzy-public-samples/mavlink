# MAVLink in-repo tests

This suite validates the critical path **XML → generated header → compile →
pack/unpack round-trip** for MAVLink 2, plus one corrupted-CRC frame-rejection
test. It is organized in two layers:

- **Python** (`tests/python/`, run by `pytest`): validates every dialect
  definition under `message_definitions/v1.0/` and verifies that `common.xml`
  generates C headers that compile cleanly.
- **C** (`tests/c/`, built and run by CMake/CTest): packs, serializes, parses,
  and decodes ten representative messages against the generated headers and
  asserts field equality, plus the corrupted-CRC rejection test. This layer is
  gated behind the root CMake option `MAVLINK_BUILD_TESTS` (default `OFF`).

## Prerequisites

Initialize the `pymavlink` submodule so the generator is importable:

```bash
git submodule update --init --recursive
```

Use Python 3.11+ (3.12 also works). Install the Python test dependencies:

```bash
python -m pip install pytest lxml
# or pinned:
python -m pip install pytest==9.1.1 lxml==6.1.1
```

`lxml` is optional for the XML-validation layer, which uses the standard-library
`xml.etree.ElementTree`, but it is pulled in transitively by `mavgen`.

Ensure GCC and CMake/CTest are available on `PATH` (provided by `ubuntu-latest`
in CI).

## Running the Python layers

```bash
python -m pytest tests/python -v
```

## Running the C layer (CMake + CTest)

```bash
cmake -S . -B build -DMAVLINK_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

Passing `-DMAVLINK_BUILD_TESTS=ON` opts into building the C test. The option
defaults to `OFF`, so ordinary builds are unaffected.

## Running a single test

Python (only the `common` dialect case):

```bash
python -m pytest tests/python/test_xml_definitions.py -k common -v
```

C (only the round-trip test):

```bash
ctest --test-dir build -R roundtrip --output-on-failure
```

## Debugging

Python:

```bash
python -m pytest tests/python -vv -s
```

C — the executable is built with `-O0 -g`, so it can be run under gdb:

```bash
gdb ./build/tests/c/test_roundtrip
```

## Notes

- GCC only — no Clang and no multi-compiler matrix.
- No coverage tooling is used (by design).
- Test isolation: the Python tests write only into temporary directories and the
  C test writes only into the CMake binary directory — nothing is ever written
  into the source tree.
