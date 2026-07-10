"""Integration test for the common.xml generation pipeline.

Consumes the session-scoped ``generated_common_headers`` fixture (see
``conftest.py``), asserts the expected headers exist, then compiles a tiny
probe that includes ``common/mavlink.h`` with ``gcc -Wall``.
"""

import shutil
import subprocess

import pytest

EXPECTED_HEADERS = ("common/mavlink.h", "common/common.h")


def test_expected_headers_generated(generated_common_headers):
    for rel in EXPECTED_HEADERS:
        header = generated_common_headers / rel
        assert header.is_file(), "expected generated header missing: %s" % rel


def test_generated_headers_compile(generated_common_headers, tmp_path):
    gcc = shutil.which("gcc")
    if gcc is None:
        pytest.skip("gcc not available; skipping compile check")

    probe = tmp_path / "probe.c"
    probe.write_text(
        '#include "common/mavlink.h"\n'
        "int main(void) { return 0; }\n"
    )

    result = subprocess.run(
        [
            gcc,
            "-Wall",
            "-I" + str(generated_common_headers),
            "-c",
            str(probe),
            "-o",
            str(tmp_path / "probe.o"),
        ],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )
    # Warnings (e.g. -Waddress-of-packed-member) are expected and acceptable;
    # only a non-zero exit status constitutes a failure.
    assert result.returncode == 0, (
        "gcc -Wall failed to compile a probe including common/mavlink.h:\n%s"
        % result.stdout
    )
