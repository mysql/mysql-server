\page PageLibsMysqlGtids Library: Gtids

<!---
Copyright (c) 2025, 2026, Oracle and/or its affiliates.
//
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2.0,
as published by the Free Software Foundation.
//
This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.
//
This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License, version 2.0, for more details.
//
You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
-->


<!--
MySQL Library: Gtids
====================
-->

Code documentation: @ref GroupLibsMysqlGtids.

## Overview

This library provides data structures for handling GTIDs:

- Tag: The tag component of a GTID: an identifier of length 0...32 alphanumeric
  ascii characters.

- Tsid: The transaction source identifier, consisting of the pair (uuid, tag).
  (The UUID component is defined in the `uuids` library).

- Sequence_number: the number component of a GTID.

- Gtid: The pair (Tsid, Sequence_number).

- Gtid_interval: an interval of sequence numbers, specified by the endpoints,
  represented as a specialization of `mysql::sets::Interval`.

- Gtid_interval_set: a set of Gtid_intervals, represented as a specialization of
  `mysql::sets::Map_interval_container`.

- Gtid_set: A set of GTIDs, represented as a specialization of
  `mysql::sets::Nested_set`. This supports all operations provided by nested
  sets in the `sets` library. See the unittest gtids_example-t.cc for example
  usage.

## String Formats

The types in this class can be encoded into strings using four formats: Text,
Binary v0, Binary v1, and Binary v2.

The text format should be used when these objects are presented to users. The
binary formats should be preferred otherwise.

If the decoder is given a buffer in any of the Binary formats, it detects which
of the formats is used by inspecting the buffer. The encoder will automatically
choose Binary v0 for sets that do not have tags and Binary v1 for sets that have
tags; it will never use Binary v2. We introduced the decoder for Binary v2
before we enabled the encoder for Binary v2, so that we can later switch to
Binary v2 without breaking cross-version compatibility.

### Text Format

The text format represents each object type as follows:
- Tag: raw text.
- Tsid: the UUID in text format (36 bytes of hex digits and dashes), followed by
  colon, followed by the Tag. If the tag is empty, the colon and Tag are
  omitted.
- Sequence_number: the text form of the integer.
- Gtid: the Tsid, followed by a colon, followed by the Sequence_number.
- Gtid_interval: the start (inclusively), followed by a dash, followed by the
  end (inclusively). If the two inclusive endpoints are are equal, the dash and
  end are omitted.
- Gtid_interval_set: a colon-separated list of Gtid_intervals.
- Gtid_set: a comma-separated list, where each entry consists of the following:
  - A UUID
  - If there are any intervals having the Tsid consisting of UUID without a tag,
    a colon followed by the Gtid_interval_set for that Tsid.
  - For each tag where there are intervals having the Tsid consisting of UUID
    with that tag, a colon, the tag, a colon, and the Gtid_interval_set.
- The formatter produces a string with all UUIDs in alphabetic order, all tags
  within a UUID in alphabetic order, and all intervals within the same Tsid
  disjoint, non-adjacent, and in numeric order. It will produce a newline after
  each comma, and no other whitespace.
- The parser does not require that UUIDs, tags, or intervals are in order, nor
  that intervals are disjoint. It accepts whitespace around each token, and at
  the beginning and end. It accepts redundant commas.

### Binary v0

The Binary v0 format can not represent tags, and is only used for backward
compatibility. It represents each object type as follows:
- Tag cannot be represented.
- Tsid: the UUID in binary using 16 bytes.
- Sequence_number: a 64-bit integer in little-endian format.
- Gtid: the Tsid followed by the Sequence_number.
- Gtid_interval: start (inclusively), followed by the end (exclusively); both
  in 64-bit little-endian format.
- Gtid_interval_set: the number of intervals as a 64-bit integer in
  little-endian format, followed by the Gtid_intervals in order.
- Gtid_set: the following fields:
  - The number of TSIDs as a 56-bit (7-byte) number in little-endian format.
  - A type code, which is always 0.
  - For each TSID, the UUID followed by the Gtid_interval_set.

### Binary v1

The Binary v1 format can represent all object types. It represents each object
type as follows:
- Tag: one byte for the length, followed by the tag characters.
- Tsid: the UUID in binary using 16 bytes, followed by the Tag.
- Sequence_number: a 64-bit integer in little-endian format.
- Gtid: the Tsid followed by the Sequence_number.
- Gtid_interval: the start (inclusively), followed by the end (exclusively);
  both in 64-bit little-endian format.
- Gtid_interval_set: the number of intervals in 64-bit little-endian format,
  followed by the Gtid_intervals in order.
- Gtid_set: the following fields:
  - A type code, which is always 1
  - The number of TSIDs as a 48-bit (6-byte) number in little-endian format.
  - Another, redundant type code, which is always 1. This exists only so that
    this format can be distinguished from Binary v0 format.
  - For each TSID, the Gtid followed by the Gtid_interval_set.

### Binary v2

The Binary v2 format can represent all object types, and is more space-effective
than the other formats. It represents each object type as follows:
- Tag: one byte for the length, followed by the tag characters.
- Tsid: the UUID in binary using 16 bytes, followed by the Tag.
- Sequence_number: an unsigned varlen integer (see the `mysql_serialization`
  library).
- Gtid: the Tsid followed by the Sequence_number.
- Gtid_intervals are not specifically serialized in this format.
- Gtid_interval_set: a sequence where each entry has the following fields:
  - The number of boundary points, i.e., twice the number of interals, in
    unsigned varlen integer format. If the start of the first interval is equal
    to 1, the number is decremented by 1. (This is an optimization to not have
    to store the first boundary point in the common case that it is 1.)
  - A sequence of deltas represented as unsigned varlen integers. For deltas
    other than the first one, the Nth delta is equal to the value of the Nth
    boundary point, minus the value of the (N-1)th boundary point, minus 1. If
    the first boundary is 1, its delta is omitted; otherwise its delta is the
    first boundary point minus 2. The deltas have smaller magnitudes than the
    boundary values, and therefore their varint representations can be shorter.
- Gtid_set: the following fields:
  - A type code, which is always 2
  - The number of TSIDs as a 48-bit (6-byte) number in little-endian format.
  - Another, redundant type code, which is always 2. This exists only so that
    this format can be distinguished from Binary Format Without Tags.
  - The number of tags in unsigned varint format.
  - The sequence of tags in alphabetic order.
  - For each Tsid in alphabetic order, a sequence of the following:
    - An integer "code" in unsigned varint format. The least significant bit of
      the code is 0 if the UUID is equal to the UUID of the previous TSID. The
      remaining bits is the ordinal position of the tag, among the alphabetic
      list of tags, shifted left one bit.
    - The UUID, unless it is equal to the UUID of the previous Tsid.
    - The Gtid_interval_set.
    The use of the code and the omission of repeated UUIDs ensures that Tags and
    UUIDs are only stored once.
