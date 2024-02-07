\page PageLibsMysqlSerialization MySQL Serialization Library

<!---
Copyright (c) 2023, 2024, Oracle and/or its affiliates.
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

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License, version 2.0, for more details.
//
You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
-->

MySQL Serialization Library
===========================

For code documentation, please refer to @ref GroupLibsMysqlSerialization.

## High-level description

Serialization framework provides methods for automatic
serialization and deserialization of event fields defined by the API user.

Serialization framework is designed to expose a simple API
that facilitates event definition, serialization and
deserialization. The user instead of implementing serializing and deserializing
functions, specifies definitions of fields included in the packet.
Field definition considers definition of:
- A number of bytes used to represent the field
- A "field missing functor", specifying what decoder should do in case the
  field is not provided in the packet. By default, decoder won't take any
  action upon a missing field.
- A "field encode predicate", specifying whether or not serializer should
  include the field in the packet. Default behavior of the encoder is to
  always include the field in the packet.
- An "unknown field policy" - action that should be taken by decoder in case
  the field is encoded in the packet but its definition is unknown to the 
  decoder (considers decoders of version older than version of the software
  which introduced field into the packet)

The idea of the serialization framework is not to introduce many new
types that can be ingested by encoding and decoding functions, but
to reuse common STL types. Types supported by serialization framework
are:
  - signed/unsigned integers (simple type)
  - floating point numbers (simple type)
  - sets,
  - maps,
  - vectors,
  - arrays (simple type) of fixed size, which cannot evolve over time
  - enumerations (simple type)
  - strings
  - custom types - nested messages, called "serializable fields"

Important design decisions are listed below:
- as message definitions evolve over time, the framework allows inserting new
  fields
- fields get automatic type codes, assigned by the serialization framework and
  used to identify fields
- fields can be effectively removed from the packet by defining a field
  encode predicate which will always return a false value. This ensures
  that removed fields don't occupy space in the output packet, while
  maintaining the auto-generated type codes and field sizes for subsequent
  fields
- for simple types, field sizes are not
  included in the binary representation of a serialized type

## Message format specification

Serialization framework types are encoded using the following
formats:
  - signed/unsigned integers -> fixlen_integer_format / varlen_integer_format
  - floating point numbers -> sp_floating_point_integer_format / dp_floating_point_integer_format
  - sets -> container_format,
  - maps -> map_container_format,
  - vectors -> container_format,
  - arrays (simple type) of fixed size -> fixed_container_format
  - enumerations -> varlen_integer_format
  - strings -> string_format
  - custom types - nested messages, called "serializable fields", encoded
    according to the message_format


Message formatting is the following:

```
{
<message_field> ::= <serialization_version_number> <message_format>
}
```

message_field consists of:
- serialization_version_number - serialization framework version number >=0,
  1 byte
- message_format

```
{
<message_format> ::= <serializable_field_size> <last_non_ignorable_field_id> { <type_field> }
<type_field> ::= <field_id> <field_data>
}
```

message_format consists of:
- serializable_field_size - Size of serializable field payload (1-9 bytes).
  Size information is used to calculate serializable type boundaries within
  a packet and also to skip unknown fields in the packet in case it is
  encoded by encoder of version newer than the version of decoder
- last_non_ignorable_field_id - (1 byte). This is a last encoded, non-ignorable
  field id. In case this field id is unknown to current version of decoder,
  it will generate an error. In case all fields in the packet are ignorable,
  last non-ignorable field id will be equal to 0.
- message_fields: field_0, field_1, field_2...

Type field consists of:
- auto-generated field_id >=0, sequence number which identifies fields
- fields formatted according to the field_data format (data only, type is not
  encoded)

```
{
<field_data> ::= <fixlen_integer_format> | <varlen_integer_format> | <floating_point_integer_format> | <string_format> |  <container_format> | <fixed_container_format> | <map_format> | <message_format>
<fp_number> ::= <sp_floating_point_integer_format> | <dp_floating_point_integer_format>
}
```

field_data can be one of:

- fixlen_integer_format - fixed-length integer, encoded using a fixed number of
  bytes, signed or unsigned
- varlen_integer_format - variable-length integer field, encoded using 1-9 bytes,
  depending on the field value
- floating_point_integer_format - floating point number field:
  - sp_floating_point_integer_format - single precision floating point number
  - dp_floating_point_integer_format - double precision floating point number
- string_format - string field
- container_format - unlimited-size container field
- fixed_container_format - fixed-length container format
- map_format - unlimited size container with key-value pairs
- message_format - nested message
- varlen_integer_format - format used to encode signed/unsigned integers using
  1-9 bytes, depending on the integer value. Format is described in detail
  in the [Variable-length integers](#variable-length-integers).

```
{
<map_format> ::= <number_elements> { <field_data> <field_data> }
}
```

Map format consists of:
- number_elements - number of elements in the container >=0
- key-value pairs formatted according to field_data format

```
{
<container_format> ::= <number_elements> { <field_data> }
}
```

Vector format consists of:
- number_elements - number of elements in the container >=0
- field_data - encoded values, according to field_data format

```
{
<fixed_container_format> ::= { <field_data> }+
}
```

Fixed-size array format consists of:
- encoded values, formatted according to field_data format

```
{
<string_format> ::= <string_length> { <character> }
}
```

String format consists of:
- string_length - the number of 1 byte elements in the string
- string characters, 1 byte unsigned integers

#### Variable-length integers

Variable-length integers are encoded using 1-9 bytes, depending on the
value of a particular field. Bytes are always stored using in LE byte order.

This format allows to implement decoding without looping through the
consecutive bytes.

For each number, rightmost contains encoded information about how many
consecutive bytes represent the number - it is equal to the number of
encoded trailing ones + 1. 
When the number itself uses at most 56 bits, then the trailing ones are
followed by a bit equal to "0"; otherwise it is followed by the full number.
The special case for 57..64 bits allows us to use only 9 bytes to store numbers
where the 63'rd bit is "1". (An encoding without such a special case would
require 10 bytes.)
For readability, in text below, we display bytes in big-endian format.
Within each byte, we display the most significant bit first. Encoding
is explained reading bits from right to left.

- unsigned integers:
  Encoded length of the integer is followed by encoded value. Examples:

  "00000111 11111111 11111011" - BE byte order, bit layout:
    most significant bit first

    65535 is represented using 3 bytes. The rightmost byte contains two
    trailing ones followed by 0 (3 least significant bits - 3 bytes used to
    store the number). The latter bits are used to store the value.

- signed integers:
  Signed integers are encoded such that both positive and negative numbers
  use fewer bits the smaller their magnitude is. The least significant bit
  is the sign and the remaining bits represent the number. If x is positive,
  then we encode (x<<1), cast to unsigned. If x is negative, then we encode
  ((-(x + 1) << 1) | 1), cast to unsigned. That is, we first add 1 to shift
  the range from -2^63..-1 to -(2^63-1)..0; then we negate the result to get
  a nonnegative number in the range 0..2^63-1; then we shift the result left
  by 1 to make place for the sign bit; then we "or" with the sign bit.
  The resulting number is reinterpreted as unsigned and serialized accordingly.

  "00001111 11111111 11110011" - BE byte order, bit layout:
    starting from the most significant bit

    65535 is represented using 3 bytes. The rightmost byte contains two
    trailing ones followed by 0 (3 least significant bits - 3 bytes used to
    store the number). After 0 encoded is one sign bit
    (equal to 0). The latter bits are used to store the value.

  "00001111 11111111 11101011" - BE byte order, bit layout:
    starting from the most significant bit

    -65535 is represented using 3 bytes. The rightmost byte contains two
    trailing ones followed by 0 (3 least significant bits - 3 bytes used to
    store the number). After 0 encoded is one sign bit
    (equal to 1). The latter bits are used to store the value.

  "00001111 11111111 11111011" - BE byte order, bit layout:
    starting from the most significant bit

    -65536 is represented using 3 bytes. The rightmost byte contains two
    trailing ones followed by 0 (3 least significant bits - 3 bytes used
    to store the number). After 0 encoded is one sign bit
    (equal to 1). The latter bits are used to store the value.
