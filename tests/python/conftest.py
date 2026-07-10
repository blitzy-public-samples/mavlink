"""Shared pytest fixtures for the MAVLink in-repo Python test suite.

Contains exactly one session-scoped fixture that generates the ``common.xml``
C headers into a temporary directory using the pinned ``pymavlink`` submodule
generator. No artifacts are written to the source tree.
"""

import os
import subprocess
import sys
from pathlib import Path

import pytest

# tests/python/conftest.py -> parents[2] is the repository root.
REPO_ROOT = Path(__file__).resolve().parents[2]
COMMON_XML = REPO_ROOT / "message_definitions" / "v1.0" / "common.xml"


@pytest.fixture(scope="session")
def generated_common_headers(tmp_path_factory):
    """Generate ``common.xml`` C headers into a temp dir and return its Path.

    Uses the pinned in-repo ``pymavlink`` submodule as the generator by setting
    ``PYTHONPATH`` to the repository root. The ``common.xml`` include chain
    (common -> standard -> minimal) resolves transitively.
    """
    output_dir = tmp_path_factory.mktemp("generated_common")

    env = dict(os.environ)
    existing = env.get("PYTHONPATH", "")
    env["PYTHONPATH"] = (
        str(REPO_ROOT) + (os.pathsep + existing if existing else "")
    )

    cmd = [
        sys.executable,
        "-m",
        "pymavlink.tools.mavgen",
        "--lang=C",
        "--wire-protocol=2.0",
        "--output=" + str(output_dir),
        str(COMMON_XML),
    ]
    result = subprocess.run(
        cmd,
        cwd=str(REPO_ROOT),
        env=env,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )
    assert result.returncode == 0, (
        "mavgen failed to generate common.xml headers "
        "(exit %d):\n%s" % (result.returncode, result.stdout)
    )
    return output_dir
