"""Shared pytest fixtures for the MAVLink in-repo Python test suite.

Provides a single, session-scoped fixture that generates the MAVLink 2
``common`` C headers exactly once (via the pinned ``pymavlink`` submodule's
``mavgen`` entry point) into a pytest-managed temporary directory. Nothing is
ever written into the source tree.
"""
from __future__ import annotations

import os
import subprocess
import sys
from pathlib import Path

import pytest

# tests/python/conftest.py -> parents[0]=tests/python, [1]=tests, [2]=<repo root>
REPO_ROOT = Path(__file__).resolve().parents[2]
COMMON_XML = REPO_ROOT / "message_definitions" / "v1.0" / "common.xml"


@pytest.fixture(scope="session")
def generated_common_headers(tmp_path_factory):
    """Generate MAVLink 2 C headers for common.xml once per test session.

    Yields the output directory containing the generated ``common/`` header
    subdirectory (plus the shared top-level headers such as protocol.h).
    """
    out_dir = tmp_path_factory.mktemp("generated_common")

    env = dict(os.environ)
    # `python -m pymavlink.tools.mavgen` requires the repo root on the import
    # path so the `pymavlink` submodule package resolves.
    existing = env.get("PYTHONPATH", "")
    env["PYTHONPATH"] = (
        str(REPO_ROOT) + (os.pathsep + existing if existing else "")
    )

    result = subprocess.run(
        [
            sys.executable,
            "-m",
            "pymavlink.tools.mavgen",
            "--lang=C",
            "--wire-protocol=2.0",
            f"--output={out_dir}",
            str(COMMON_XML),
        ],
        cwd=str(REPO_ROOT),
        env=env,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )
    # mavgen calls exit(1) on failure; returncode 0 means generation succeeded.
    assert result.returncode == 0, (
        "mavgen failed to generate common.xml MAVLink 2 headers "
        f"(exit {result.returncode}):\n{result.stdout}"
    )
    yield out_dir
