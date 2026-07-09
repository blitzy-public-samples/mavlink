"""Integration test: generate common.xml MAVLink 2 C headers and compile them.

Consumes the session-scoped ``generated_common_headers`` fixture (conftest.py),
which enforces that mavgen exited 0. This module then verifies the expected
headers exist and that a small translation unit including the generated header
compiles cleanly under GCC. All artifacts live in pytest temp directories only.
"""
from __future__ import annotations

import shutil
import subprocess

import pytest


def test_common_headers_exist(generated_common_headers):
    """mavgen must have produced common/mavlink.h and common/common.h."""
    common_dir = generated_common_headers / "common"
    assert (common_dir / "mavlink.h").is_file(), "generated common/mavlink.h missing"
    assert (common_dir / "common.h").is_file(), "generated common/common.h missing"


def test_generated_common_compiles(generated_common_headers, tmp_path):
    """A TU including the generated header compiles with `gcc -Wall -c` (exit 0).

    Note: plain `-Wall` (no `-Werror`) is used deliberately; the generated
    headers emit benign -Waddress-of-packed-member warnings that must not fail
    the build. Success is defined solely by a zero exit code.
    """
    gcc = shutil.which("gcc")
    if gcc is None:
        pytest.skip("gcc not available on PATH")

    out_dir = generated_common_headers
    common_dir = out_dir / "common"

    src = tmp_path / "tu.c"
    src.write_text("#include <mavlink.h>\nint main(void) { return 0; }\n")
    obj = tmp_path / "tu.o"

    result = subprocess.run(
        [
            gcc, "-Wall", "-c", str(src), "-o", str(obj),
            f"-I{out_dir}", f"-I{common_dir}",
        ],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )
    assert result.returncode == 0, (
        f"gcc failed to compile generated header (exit {result.returncode}):\n{result.stdout}"
    )
