\page PageLibsMysqlAbiHelpers Library: Abi Helpers

<!---
Copyright (c) 2024, Oracle and/or its affiliates.
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
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA.
-->


<!--
MySQL Library: Abi Helpers
==========================
-->

Code documentation: @ref GroupLibsMysqlAbiHelpers.

This library defines basic types that intended to help defining data types to be
used in interfaces between different shared objects (shared libraries), while
providing ABI compatibility. The types make the ABI compatible in several ways:

 1. All types are *standard layout* in the sense of
    https://en.cppreference.com/w/cpp/named_req/StandardLayoutType . This makes
    them tolerant to differences in compiler choices. In particular, it ensures
    that struct members appear in a deterministic order in  memory.

    However, note that the types are only compatible if the memory alignment and
    word widths are the same in all shared objects linked together. This is not
    guaranteed by the C++ standard. But in practice, it usually holds on all
    compilers on the same platform. This works currently in all build types for
    MySQL. If this changes in the future, this API needs to be replaced by
    something else.

 2. The template class Packet provides a way to define structured data such
    that it can evolve over time without breaking ABI. A Packet can be used in
    a stable ABI instead of a struct. It will be encoded as an array where each
    entry is a (typecode, value) pair. If needed, future versions can add new
    pairs and remove existing pairs without changing ABI compatibility. The only
    requirement is that type codes of existing fields are not changed, and that
    type codes of removed fields are not reused for new fields.

 3. There are helper functions to allocate memory for arrays. These are
    conditionally compiled, so they only exist when the header is included from
    server code. This ensures that an object allocated by the server is deleted
    by the server, as long as this API is used. Allocating and deleting objects
    from different shared object/shared libraries is undefined and leads to
    crashes on some systems.

Example: to define a stable ABI for a service that provides row data for a
table, the row data may be stored as an array of Fields:
@code
  // The table has three columns. The type code identifies the column.
  enum Employee_field_type { col_name, col_birth_date, col_employment_date };
  // A row has three elements, and we store them in an array of 3 Field
  // objects where each value consists of a type code and the actual value.
  using Employee_row = Packet<Employee_field_type>;
  // The table is a vector of rows.
  using Employees = Vector<Employee_row>;
@endcode

An alternative to using this library, is to use the `mysql_serialization`
library (which is similar to protobuf and other frameworks). `mysql_abi_helpers`
is used for communication between shared objects *within* a process, whereas
`mysql_serialization` is used for communication *between* processes, even on
different architectures. Therefore, this library can make assumptions that makes
it faster and simpler:
- `mysql_abi_helpers` stores data in native C++ data types, whereas
  `mysql_serialization` stores data in byte streams.
- `mysql_serialization` has to encode integers by swapping endianness, and in
  the default backend it encodes integers in a space-efficient variable length
  format. `mysql_abi_helpers` does not do that; it just stores any integer as a
  `long long` in memory.
- `mysql_serialization` decodes data into an object representation.
  `mysql_abi_helpers` does not need that; the user can access data directly
  through the data structure, in which case no decoding is needed.
- `mysql_abi_helpers` is a header-only libary, and very small.
