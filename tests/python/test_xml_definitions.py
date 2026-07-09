"""Unit validation of all MAVLink dialect XML definitions.

Parametrized over every dialect in ``message_definitions/v1.0/``. For each
dialect the suite asserts: (1) well-formed XML, (2) every ``<message id>`` is an
integer within the MAVLink 2 24-bit range [0, 16777215], (3) message IDs are
unique within that file (per-file only; the ``<include>`` graph is NOT
traversed), and (4) every ``<field type>`` is a recognized MAVLink base type
(allowing an optional ``[N]`` array suffix and the special
``uint8_t_mavlink_version`` type). Definition files are read strictly read-only.
"""
from __future__ import annotations

import re
import xml.etree.ElementTree as ET
from collections import Counter
from pathlib import Path

import pytest

# tests/python/test_xml_definitions.py -> parents[2] = <repo root>
REPO_ROOT = Path(__file__).resolve().parents[2]
DEFINITIONS_DIR = REPO_ROOT / "message_definitions" / "v1.0"

# The 19 dialects shipped in message_definitions/v1.0/.
DIALECTS = [
    "ASLUAV", "AVSSUAS", "all", "ardupilotmega", "common", "csAirLink",
    "cubepilot", "development", "icarous", "loweheiser", "matrixpilot",
    "minimal", "paparazzi", "python_array_test", "standard", "storm32",
    "test", "uAvionix", "ualberta",
]

MAVLINK_V2_MAX_MSG_ID = 16_777_215  # 2**24 - 1

VALID_FIELD_TYPES = {
    "char", "int8_t", "uint8_t", "int16_t", "uint16_t", "int32_t", "uint32_t",
    "int64_t", "uint64_t", "float", "double", "uint8_t_mavlink_version",
}

_ARRAY_SUFFIX = re.compile(r"\[\d+\]$")


def _dialect_path(dialect: str) -> Path:
    return DEFINITIONS_DIR / f"{dialect}.xml"


@pytest.mark.parametrize("dialect", DIALECTS)
def test_dialect_is_well_formed(dialect: str) -> None:
    path = _dialect_path(dialect)
    assert path.is_file(), f"dialect file missing: {path}"
    ET.parse(path)  # raises ParseError if not well-formed


@pytest.mark.parametrize("dialect", DIALECTS)
def test_message_ids_valid_and_unique(dialect: str) -> None:
    root = ET.parse(_dialect_path(dialect)).getroot()
    ids = []
    for message in root.iter("message"):
        raw = message.get("id")
        assert raw is not None, f"{dialect}: <message> missing id attribute"
        mid = int(raw)  # ValueError if non-integer
        assert 0 <= mid <= MAVLINK_V2_MAX_MSG_ID, (
            f"{dialect}: message id {mid} outside MAVLink 2 range "
            f"[0, {MAVLINK_V2_MAX_MSG_ID}]"
        )
        ids.append(mid)
    # Per-file uniqueness only (empty message sets are valid for aggregators).
    dupes = [i for i, c in Counter(ids).items() if c > 1]
    assert not dupes, f"{dialect}: duplicate message ids within file: {sorted(dupes)}"


@pytest.mark.parametrize("dialect", DIALECTS)
def test_field_types_are_valid(dialect: str) -> None:
    root = ET.parse(_dialect_path(dialect)).getroot()
    for message in root.iter("message"):
        for field in message.iter("field"):
            ftype = field.get("type")
            assert ftype is not None, (
                f"{dialect}: <field> missing type in message "
                f"'{message.get('name')}'"
            )
            base = _ARRAY_SUFFIX.sub("", ftype)
            assert base in VALID_FIELD_TYPES, (
                f"{dialect}: message '{message.get('name')}' field "
                f"'{field.get('name')}' has unrecognized type '{ftype}'"
            )
