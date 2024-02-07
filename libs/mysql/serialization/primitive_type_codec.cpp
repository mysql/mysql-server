// Copyright (c) 2023, 2024, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is designed to work with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation.  The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have either included with
// the program or referenced in the documentation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA.

#include "mysql/serialization/primitive_type_codec.h"
#include <stdint.h>
#include <iostream>
#include "mysql/serialization/variable_length_integers.h"

#include "my_byteorder.h"

/// @file

namespace mysql::serialization {

template <>
template <>
size_t Primitive_type_codec<uint8_t>::write_bytes<1>(unsigned char *stream,
                                                     const uint8_t &data) {
  *stream = data;
  return 1;
}

template <>
template <>
size_t Primitive_type_codec<uint16_t>::write_bytes<2>(unsigned char *stream,
                                                      const uint16_t &data) {
  int2store(stream, data);
  return 2;
}

template <>
template <>
size_t Primitive_type_codec<uint32_t>::write_bytes<3>(unsigned char *stream,
                                                      const uint32_t &data) {
  int3store(stream, data);
  return 3;
}

template <>
template <>
size_t Primitive_type_codec<uint32_t>::write_bytes<4>(unsigned char *stream,
                                                      const uint32_t &data) {
  int4store(stream, data);
  return 4;
}

template <>
template <>
size_t Primitive_type_codec<uint64_t>::write_bytes<5>(unsigned char *stream,
                                                      const uint64_t &data) {
  int5store(stream, data);
  return 5;
}

template <>
template <>
size_t Primitive_type_codec<uint64_t>::write_bytes<6>(unsigned char *stream,
                                                      const uint64_t &data) {
  int6store(stream, data);
  return 6;
}

template <>
template <>
size_t Primitive_type_codec<uint64_t>::write_bytes<7>(unsigned char *stream,
                                                      const uint64_t &data) {
  int7store(stream, data);
  return 7;
}

template <>
template <>
size_t Primitive_type_codec<uint64_t>::write_bytes<8>(unsigned char *stream,
                                                      const uint64_t &data) {
  int8store(stream, data);
  return 8;
}

template <>
template <>
size_t Primitive_type_codec<float>::write_bytes<4>(unsigned char *stream,
                                                   const float &data) {
  float4store(stream, data);
  return 4;
}

template <>
template <>
size_t Primitive_type_codec<double>::write_bytes<8>(unsigned char *stream,
                                                    const double &data) {
  float8store(stream, data);
  return 8;
}

template <>
template <>
size_t Primitive_type_codec<float>::write_bytes<0>(unsigned char *stream,
                                                   const float &data) {
  return write_bytes<4>(stream, data);
}

template <>
template <>
size_t Primitive_type_codec<double>::write_bytes<0>(unsigned char *stream,
                                                    const double &data) {
  return write_bytes<8>(stream, data);
}

template <>
template <>
size_t Primitive_type_codec<uint8_t>::read_bytes<1>(const unsigned char *stream,
                                                    std::size_t stream_bytes,
                                                    uint8_t &data) {
  if (stream_bytes < 1) {
    return 0;
  }
  data = *stream;
  return 1;
}

template <>
template <>
size_t Primitive_type_codec<uint16_t>::read_bytes<2>(
    const unsigned char *stream, std::size_t stream_bytes, uint16_t &data) {
  if (stream_bytes < 2) {
    return 0;
  }
  data = uint2korr(stream);
  return 2;
}

template <>
template <>
size_t Primitive_type_codec<uint32_t>::read_bytes<3>(
    const unsigned char *stream, std::size_t stream_bytes, uint32_t &data) {
  if (stream_bytes < 3) {
    return 0;
  }
  data = uint3korr(stream);
  return 3;
}

template <>
template <>
size_t Primitive_type_codec<uint32_t>::read_bytes<4>(
    const unsigned char *stream, std::size_t stream_bytes, uint32_t &data) {
  if (stream_bytes < 4) {
    return 0;
  }
  data = uint4korr(stream);
  return 4;
}

template <>
template <>
size_t Primitive_type_codec<uint64_t>::read_bytes<5>(
    const unsigned char *stream, std::size_t stream_bytes, uint64_t &data) {
  if (stream_bytes < 5) {
    return 0;
  }
  data = uint5korr(stream);
  return 5;
}

template <>
template <>
size_t Primitive_type_codec<uint64_t>::read_bytes<6>(
    const unsigned char *stream, std::size_t stream_bytes, uint64_t &data) {
  if (stream_bytes < 6) {
    return 0;
  }
  data = uint6korr(stream);
  return 6;
}

template <>
template <>
size_t Primitive_type_codec<uint64_t>::read_bytes<7>(
    const unsigned char *stream, std::size_t stream_bytes, uint64_t &data) {
  if (stream_bytes < 7) {
    return 0;
  }
  data = uint7korr(stream);
  return 7;
}

template <>
template <>
size_t Primitive_type_codec<uint64_t>::read_bytes<8>(
    const unsigned char *stream, std::size_t stream_bytes, uint64_t &data) {
  if (stream_bytes < 8) {
    return 0;
  }
  data = uint8korr(stream);
  return 8;
}

template <>
template <>
size_t Primitive_type_codec<float>::read_bytes<4>(const unsigned char *stream,
                                                  std::size_t stream_bytes,
                                                  float &data) {
  if (stream_bytes < 4) {
    return 0;
  }
  data = float4get(stream);
  return 4;
}

template <>
template <>
size_t Primitive_type_codec<double>::read_bytes<8>(const unsigned char *stream,
                                                   std::size_t stream_bytes,
                                                   double &data) {
  if (stream_bytes < 8) {
    return 0;
  }
  data = float8get(stream);
  return 8;
}

template <>
template <>
size_t Primitive_type_codec<float>::read_bytes<0>(const unsigned char *stream,
                                                  std::size_t stream_bytes,
                                                  float &data) {
  return read_bytes<4>(stream, stream_bytes, data);
}

template <>
template <>
size_t Primitive_type_codec<double>::read_bytes<0>(const unsigned char *stream,
                                                   std::size_t stream_bytes,
                                                   double &data) {
  return read_bytes<8>(stream, stream_bytes, data);
}

template <>
template <>
size_t Primitive_type_codec<int8_t>::write_bytes<0>(unsigned char *stream,
                                                    const int8_t &data) {
  return detail::write_varlen_bytes(stream, data);
}

template <>
template <>
size_t Primitive_type_codec<uint8_t>::write_bytes<0>(unsigned char *stream,
                                                     const uint8_t &data) {
  return detail::write_varlen_bytes(stream, data);
}

template <>
template <>
size_t Primitive_type_codec<int16_t>::write_bytes<0>(unsigned char *stream,
                                                     const int16_t &data) {
  return detail::write_varlen_bytes(stream, data);
}

template <>
template <>
size_t Primitive_type_codec<uint16_t>::write_bytes<0>(unsigned char *stream,
                                                      const uint16_t &data) {
  return detail::write_varlen_bytes(stream, data);
}

template <>
template <>
size_t Primitive_type_codec<int32_t>::write_bytes<0>(unsigned char *stream,
                                                     const int32_t &data) {
  return detail::write_varlen_bytes(stream, data);
}

template <>
template <>
size_t Primitive_type_codec<uint32_t>::write_bytes<0>(unsigned char *stream,
                                                      const uint32_t &data) {
  return detail::write_varlen_bytes(stream, data);
}

template <>
template <>
size_t Primitive_type_codec<int64_t>::write_bytes<0>(unsigned char *stream,
                                                     const int64_t &data) {
  return detail::write_varlen_bytes(stream, data);
}

template <>
template <>
size_t Primitive_type_codec<uint64_t>::write_bytes<0>(unsigned char *stream,
                                                      const uint64_t &data) {
  return detail::write_varlen_bytes(stream, data);
}

template <>
template <>
size_t Primitive_type_codec<int8_t>::read_bytes<0>(const unsigned char *stream,
                                                   std::size_t stream_bytes,
                                                   int8_t &data) {
  return detail::read_varlen_bytes(stream, stream_bytes, data);
}

template <>
template <>
size_t Primitive_type_codec<uint8_t>::read_bytes<0>(const unsigned char *stream,
                                                    std::size_t stream_bytes,
                                                    uint8_t &data) {
  return detail::read_varlen_bytes(stream, stream_bytes, data);
}

template <>
template <>
size_t Primitive_type_codec<int16_t>::read_bytes<0>(const unsigned char *stream,
                                                    std::size_t stream_bytes,
                                                    int16_t &data) {
  return detail::read_varlen_bytes(stream, stream_bytes, data);
}

template <>
template <>
size_t Primitive_type_codec<uint16_t>::read_bytes<0>(
    const unsigned char *stream, std::size_t stream_bytes, uint16_t &data) {
  return detail::read_varlen_bytes(stream, stream_bytes, data);
}

template <>
template <>
size_t Primitive_type_codec<int32_t>::read_bytes<0>(const unsigned char *stream,
                                                    std::size_t stream_bytes,
                                                    int32_t &data) {
  return detail::read_varlen_bytes(stream, stream_bytes, data);
}

template <>
template <>
size_t Primitive_type_codec<uint32_t>::read_bytes<0>(
    const unsigned char *stream, std::size_t stream_bytes, uint32_t &data) {
  return detail::read_varlen_bytes(stream, stream_bytes, data);
}

template <>
template <>
size_t Primitive_type_codec<int64_t>::read_bytes<0>(const unsigned char *stream,
                                                    std::size_t stream_bytes,
                                                    int64_t &data) {
  return detail::read_varlen_bytes(stream, stream_bytes, data);
}

template <>
template <>
size_t Primitive_type_codec<uint64_t>::read_bytes<0>(
    const unsigned char *stream, std::size_t stream_bytes, uint64_t &data) {
  return detail::read_varlen_bytes(stream, stream_bytes, data);
}

}  // namespace mysql::serialization
