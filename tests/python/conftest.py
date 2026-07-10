"""Shared pytest fixtures and isolation setup for the MAVLink in-repo suite.

Contains exactly one session-scoped fixture that generates the ``common.xml``
C headers into a temporary directory using the pinned ``pymavlink`` submodule
generator, plus import-time isolation/hardening so that running the suite
leaves the source tree unmodified: interpreter byte-code caches are
suppressed, pytest's cache directory is redirected out of the repository, and
pytest's temporary base is placed under a private, unpredictable root (which
also mitigates CVE-2025-71176 for the pinned ``pytest 7.4.4``). No artifacts
are written to the source tree.
"""

import os
import subprocess
import sys
import tempfile
from pathlib import Path

import pytest

# ---------------------------------------------------------------------------
# Test-suite isolation and security hardening (applied at import time).
#
# pytest imports conftest.py during start-up, before its tmpdir/cache plugins
# are configured, so these statements take effect for EVERY invocation of the
# suite -- including the bare ``python -m pytest tests/python -v`` documented
# in the README, not only the CI workflow.
#
# 1. ``sys.dont_write_bytecode`` stops CPython from writing ``__pycache__``
#    byte-code for the test modules imported after this point, keeping the
#    source tree free of interpreter cache files (AAP isolation mandate:
#    "the source tree is never written to" -- 0.1.2 / 0.7.2 / 0.10).
# 2. ``PYTEST_DEBUG_TEMPROOT`` points pytest's temporary base at a private,
#    unpredictable ``mktemp -d`` root instead of the predictable, shared
#    ``/tmp/pytest-of-<user>`` path. This is the mitigation for
#    CVE-2025-71176 / GHSA-6w46-j5rx-g56g (CWE-379) affecting the pinned
#    ``pytest==7.4.4`` (whose fix, 9.0.3, the pymavlink ``pytest <= 7.4.4``
#    developer pin forbids adopting). ``setdefault`` means any value the
#    caller already exported -- e.g. the CI workflow's
#    ``PYTEST_DEBUG_TEMPROOT="$(mktemp -d)"`` -- is always respected; the
#    suite only supplies a private default when none was set.
# ---------------------------------------------------------------------------
sys.dont_write_bytecode = True
os.environ.setdefault(
    "PYTEST_DEBUG_TEMPROOT", tempfile.mkdtemp(prefix="mavlink_pytest_temproot_")
)

# tests/python/conftest.py -> parents[2] is the repository root.
REPO_ROOT = Path(__file__).resolve().parents[2]
COMMON_XML = REPO_ROOT / "message_definitions" / "v1.0" / "common.xml"


def pytest_configure(config):
    """Redirect pytest's cache directory outside the source tree.

    Keeps the ``.pytest_cache`` directory under a private temporary directory
    rather than the repository root, so running the suite leaves the working
    tree clean (the same AAP isolation mandate as above). pytest is pinned to
    7.4.4, whose ``Cache`` stores its location on the private ``_cachedir``
    attribute; the assignment is guarded so any future internal change simply
    degrades to pytest's default cache location without breaking the run.
    """
    cache = getattr(config, "cache", None)
    if cache is not None:
        try:
            cache._cachedir = Path(
                tempfile.mkdtemp(prefix="mavlink_pytest_cache_")
            )
        except Exception:  # pragma: no cover - defensive, version-guard only
            pass


def pytest_unconfigure(config):
    """Remove this suite's own byte-code cache so a run leaves the tree pristine.

    ``conftest.py`` is imported (and its ``.pyc`` compiled) before the
    module-level ``sys.dont_write_bytecode`` above can take effect, so a single
    ``__pycache__/conftest.*.pyc`` is written for this file on every run. Remove
    just that cache at session teardown so a completed run leaves the source
    tree byte-for-byte unchanged (AAP "the source tree is never written to").
    Strictly scoped to this file's own byte-code and best-effort.
    """
    try:
        pycache = Path(__file__).resolve().parent / "__pycache__"
        for pyc in pycache.glob("conftest.*.pyc"):
            pyc.unlink()
        # Remove the directory only if empty, leaving any unrelated content.
        if pycache.is_dir() and not any(pycache.iterdir()):
            pycache.rmdir()
    except Exception:  # pragma: no cover - best-effort cleanup
        pass


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
    # The mavgen subprocess is a separate interpreter, so the parent's
    # ``sys.dont_write_bytecode`` does not apply to it; set the equivalent
    # environment variable so the generator does not write ``__pycache__``
    # byte-code into the pinned pymavlink submodule tree. Generation stays
    # fully isolated from the source tree.
    env["PYTHONDONTWRITEBYTECODE"] = "1"

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
