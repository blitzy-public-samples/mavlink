"""Static validation of every MAVLink dialect XML definition.

Parametrized over all dialect XMLs discovered at runtime via the
``message_definitions/v1.0/*.xml`` glob (do not hardcode filenames). Each
dialect is validated for: well-formedness, unique message IDs, whitelisted
field types, and message IDs within the MAVLink 2 24-bit range.
"""

import re
import xml.etree.ElementTree as ET
from pathlib import Path

import pytest

# tests/python/test_xml_definitions.py -> parents[2] is the repository root.
REPO_ROOT = Path(__file__).resolve().parents[2]
DEFINITIONS_DIR = REPO_ROOT / "message_definitions" / "v1.0"

# Whitelist of MAVLink base field types (optional [N] array suffix handled
# by the regex below).
VALID_BASE_TYPES = {
    "char",
    "int8_t",
    "uint8_t",
    "uint8_t_mavlink_version",
    "int16_t",
    "uint16_t",
    "int32_t",
    "uint32_t",
    "int64_t",
    "uint64_t",
    "float",
    "double",
}

# base type with optional [N] array suffix, e.g. "char[50]", "uint16_t[8]".
FIELD_TYPE_RE = re.compile(r"^(?P<base>[A-Za-z0-9_]+)(?:\[(?P<n>\d+)\])?$")

MAVLINK2_MAX_MSG_ID = 16777215  # 2**24 - 1

DIALECT_XMLS = sorted(DEFINITIONS_DIR.glob("*.xml"))


def _ids(path):
    return path.name


def test_dialects_discovered():
    """Guard against a broken glob silently collecting nothing."""
    assert DIALECT_XMLS, "no dialect XMLs found under %s" % DEFINITIONS_DIR


@pytest.mark.parametrize("xml_path", DIALECT_XMLS, ids=_ids)
def test_well_formed(xml_path):
    # (a) Well-formedness: ET.parse raises ParseError on malformed XML.
    ET.parse(xml_path)


@pytest.mark.parametrize("xml_path", DIALECT_XMLS, ids=_ids)
def test_message_ids_unique(xml_path):
    # (b) Message IDs are unique within the dialect file.
    root = ET.parse(xml_path).getroot()
    ids = [int(msg.get("id")) for msg in root.iter("message")]
    assert len(ids) == len(set(ids)), (
        "duplicate message id(s) in %s: %s"
        % (xml_path.name, sorted(i for i in set(ids) if ids.count(i) > 1))
    )


@pytest.mark.parametrize("xml_path", DIALECT_XMLS, ids=_ids)
def test_field_types_whitelisted(xml_path):
    # (c) Every <field type> is a whitelisted base type + optional [N].
    root = ET.parse(xml_path).getroot()
    for field in root.iter("field"):
        ftype = field.get("type", "")
        match = FIELD_TYPE_RE.match(ftype)
        assert match is not None, (
            "malformed field type %r in %s" % (ftype, xml_path.name)
        )
        assert match.group("base") in VALID_BASE_TYPES, (
            "non-whitelisted field type %r in %s" % (ftype, xml_path.name)
        )


@pytest.mark.parametrize("xml_path", DIALECT_XMLS, ids=_ids)
def test_message_ids_in_24bit_range(xml_path):
    # (d) Each <message id> is an integer within 0..16,777,215.
    root = ET.parse(xml_path).getroot()
    for msg in root.iter("message"):
        mid = int(msg.get("id"))
        assert 0 <= mid <= MAVLINK2_MAX_MSG_ID, (
            "message id %d out of 24-bit range in %s" % (mid, xml_path.name)
        )
