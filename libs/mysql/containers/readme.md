\page PageLibsMysqlContainers Library: Containers

<!---
Copyright (c) 2024, Oracle and/or its affiliates.
//
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2.0,
as published by the Free Software Foundation.
//
This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms, as
designated in a particular file or component or in included license
documentation. The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.
//
This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See
the GNU General Public License, version 2.0, for more details.
//
You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
-->


<!--
MySQL Containers Library
========================
-->

Code documentation: @ref GroupLibsMysqlContainers.

## Summary

This library contains the following types of containers:

### Buffers

Provides two types of containers. Both containers are used to store sequences of
bytes, and both have the following two properties:
- There is a cursor pointing to a position within the container, where by
  convention the user stores data before the cursor, the "read part", and the
  space after the buffer, the "write part", may be uninitialized.
- The user can request the container to grow. How much it grows is controlled by
  a "grow policy", which the user can control and fine-tune.

The two types of containers are:
- buffer: a single, contiguous block of bytes. The structure is similar to a
  vector, but with the addition of the cursor position and the controllable grow
  policy.
- buffer sequence: a sequence of contiguous buffers. The structure is similar to
  a deque, but with the addition of the cursor position and the controllable
  grow policy. Also, provides iterators over the sequence of contiguous
  sub-segments, rather than a flat non-contiguous sequence.

Both data structures are intended for the use case where a third-party API
requires the user to provide buffers for the API to write to, where the exact
size is not known beforehand. Both data structures conveniently provide the
cursor position to track the write position, and the grow policy to control
memory usage when a bound on the memory is known. The buffer, just like a
vector, guarantees that the stored bytes are contiguous in memory, for the price
of having to copy existing data to a new location when it grows. The buffer
sequence, just like a deque, never needs to copy existing data during a grow
operation, but stores bytes non-contiguously.

Both the buffer and the buffer sequence exist as two classes:
- `Managed_buffer` and `Managed_buffer_sequence` manage their own memory, and
  can grow, and have a cursor position.
- `Buffer_view` and `Buffer_sequence_view` are non-owning, non-growable, without
  a cursor position. They are used to represent views over the read part and the
  write part of their managed counterparts.
- (`Rw_buffer` and `Rw_buffer_sequence` are non-owning, non-growable, but have a
  cursor position. They are not normally used directly, but rather exist in
  order to separate the "managing" part from the "cursor" part in the
  implementation.)
