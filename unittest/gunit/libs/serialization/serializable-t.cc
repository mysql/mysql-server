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

#include <gtest/gtest.h>
#include <array>
#include <sstream>
#include <string>

#include "mysql/serialization/archive_binary.h"
#include "mysql/serialization/archive_text.h"
#include "mysql/serialization/field_definition_helpers.h"
#include "mysql/serialization/primitive_type_codec.h"
#include "mysql/serialization/read_archive_binary.h"
#include "mysql/serialization/serializable.h"
#include "mysql/serialization/serializer.h"
#include "mysql/serialization/serializer_default.h"
#include "mysql/serialization/unknown_field_policy.h"
#include "mysql/serialization/write_archive_binary.h"
#include "mysql/utils/enumeration_utils.h"

namespace mysql::serialization {

static constexpr bool debug_print = false;

// enum class serialized in test below
enum class My_enum : uint32_t { state1, state2, state3 };
enum class My_enum_v2 : uint32_t { state1, state2, state3, state4 };
}  // namespace mysql::serialization

namespace mysql::utils {
template <>
constexpr inline typename serialization::My_enum
enum_max<serialization::My_enum>() {
  return serialization::My_enum::state3;
}

template <>
constexpr inline typename serialization::My_enum_v2
enum_max<serialization::My_enum_v2>() {
  return serialization::My_enum_v2::state4;
}
}  // namespace mysql::utils

namespace mysql::serialization {

static constexpr std::size_t serializable_overhead_small = 3;
static constexpr std::size_t serializable_overhead_small_no_id = 2;
static constexpr std::size_t serializable_overhead_max = 9 + 9 + 9;
static constexpr std::size_t id_max_size = 9;

// Simple message format, which will be aggregated in Format_a
struct Format_internal : public Serializable<Format_internal> {
  uint32_t field_a_1 = 10;
  uint32_t field_a_2 = 11;

  auto define_fields() const {
    return std::make_tuple(define_field(field_a_1), define_field(field_a_2));
  }

  auto define_fields() {
    return std::make_tuple(define_field(field_a_1), define_field(field_a_2));
  }
};

// Message format that encapsulates Format_internal
struct Format_a : public Serializable<Format_a> {
  uint64_t field_a = 5;
  uint64_t field_b = 4;
  uint32_t field_c = 6;
  Format_internal compound_field_1;
  uint32_t field_d = 7;
  decltype(auto) define_fields() {
    return std::make_tuple(
        define_field(field_a), define_field(field_b), define_field(field_c),
        define_compound_field(compound_field_1), define_field(field_d));
  }

  decltype(auto) define_fields() const {
    return std::make_tuple(
        define_field(field_a), define_field(field_b), define_field(field_c),
        define_compound_field(compound_field_1), define_field(field_d));
  }
};

// Serializer class implementation used to exercise Serializer interface
// Does not write or read metadata for internal messages, is not backward
// or forward compatible. Uses text format
struct Serializer_printer
    : public Serializer<Serializer_printer, Archive_text> {
  template <class Field_type, Field_size field_size_defined>
  void encode(Level_type level, Field_id_type field_id,
              const Field_definition<Field_type, field_size_defined>
                  &field_definition) {
    uint64_t field_id_converted = static_cast<uint64_t>(field_id);
    for (size_t lid = 0; lid < level; ++lid) {
      m_archive.put_level_separator();
    }
    m_archive << create_varlen_field_wrapper(field_id_converted);
    m_archive.put_entry_separator();
    m_archive << Field_wrapper<const Field_type, field_size_defined>(
        field_definition.get_ref());
    m_archive.put_field_separator();
  }
  template <class Field_type, Field_size field_size_defined>
  void decode(
      Level_type, Field_id_type field_id, Field_id_type,
      Field_definition<Field_type, field_size_defined> &field_definition) {
    uint64_t field_type_converted = 0;
    m_archive >> create_varlen_field_wrapper(field_type_converted) >>
        Field_wrapper<Field_type, field_size_defined>(
            field_definition.get_ref());
    Field_id_type field_type_read =
        static_cast<Field_id_type>(field_type_converted);
    if (field_id != field_type_read) {
      std::cout << "field_id mismatch, got: " << uint32(field_type_read)
                << ", expected: " << uint32(field_id) << std::endl;
    }
  }

  template <class Serializable_type>
  void encode_serializable_metadata(Level_type, Field_id_type,
                                    Serializable_type &&, bool) {}

  template <class Serializable_type>
  std::size_t decode_serializable_metadata(Level_type, Field_id_type,
                                           Serializable_type &&, bool) {
    return 0;  // don't use serializable_end_pos information
  }
};

// Basic test which exercises Serializable interface
//
// R1. When providing a new serializer implementation, serialization
//     framework shall correctly decode message encoded with serializer
//     class provided
//
// 1. Write a structure with a nested serializable structure, all fields are
//    provided. Use a custom serializer and the Archive_text as an archive
// 2. Read a message with a nested message
// 3. Check that data read is equal to data written
TEST(Serialization, Basic) {
  Format_a var_a;
  var_a.field_a = 10;
  var_a.field_b = 11;
  var_a.field_c = 12;
  var_a.compound_field_1.field_a_1 = 13;
  var_a.compound_field_1.field_a_2 = 14;
  var_a.field_d = 16;

  Serializer_printer serializer;

  serializer << var_a;

  Format_a var_b;
  serializer >> var_b;

  ASSERT_EQ(var_a.field_a, var_b.field_a);
  ASSERT_EQ(var_a.field_b, var_b.field_b);
  ASSERT_EQ(var_a.field_c, var_b.field_c);
  ASSERT_EQ(var_a.compound_field_1.field_a_1, var_b.compound_field_1.field_a_1);
  ASSERT_EQ(var_a.compound_field_1.field_a_2, var_b.compound_field_1.field_a_2);
  ASSERT_EQ(var_a.field_d, var_b.field_d);
}

// Basic test verifying Archive_text implementation
//
// R2. When using text format, serialization framework shall correctly
//     decode and encode provided data
//
// 1. Write a structure encapsulating other serializable structure.
//    Provide all fields. Use Serializer_default and Archive_text
// 2. Read a message with a nested message
// 3. Check that data read is equal to data written
TEST(Serialization, DefaultSerializer) {
  Format_a var_a;
  var_a.field_a = ~0ULL;
  var_a.field_b = 11;
  var_a.field_c = 12;
  var_a.compound_field_1.field_a_1 = 13;
  var_a.compound_field_1.field_a_2 = 14;
  var_a.field_d = 16;

  Serializer_default<Archive_text> serializer;

  serializer << var_a;
  Format_a var_b;
  serializer >> var_b;

  ASSERT_EQ(var_a.field_a, var_b.field_a);
  ASSERT_EQ(var_a.field_b, var_b.field_b);
  ASSERT_EQ(var_a.field_c, var_b.field_c);
  ASSERT_EQ(var_a.compound_field_1.field_a_1, var_b.compound_field_1.field_a_1);
  ASSERT_EQ(var_a.compound_field_1.field_a_2, var_b.compound_field_1.field_a_2);
  ASSERT_EQ(var_a.field_d, var_b.field_d);
}

struct Small_structure : public Serializable<Small_structure> {
  uint32_t field_a = 0;

  decltype(auto) define_fields() {
    return std::make_tuple(define_field(field_a));
  }

  decltype(auto) define_fields() const {
    return std::make_tuple(define_field(field_a));
  }
};

// Basic test verifying Archive_binary implementation, reading and writing
// of a basic message
//
// R3. When using binary format, serialization framework shall correctly
//     decode and encode a message
//
// 1. Write a structure with basic fields. Use Archive_binary and
//    Serializer_default. Provide all fields.
// 2. Read a structure
// 3. Check that data read is equal to data written
// 4. Check that calculated size of the encoded data is equal to the result
//    of get_size function
TEST(Serialization, BasicMessage) {
  // create object of serializer_default class
  Serializer_default<Archive_binary> serializer;

  // define what you would like to write
  Small_structure small;
  small.field_a = 55;

  // write data into archive
  serializer << small;
  ASSERT_EQ(serializer.get_archive().get_raw_data().size(),
            Serializer_default<Archive_binary>::get_size(small));

  // read data from the archive into another object
  Small_structure small_read;
  serializer >> small_read;

  // Observe that objects states are equal
  ASSERT_EQ(small_read.field_a, small.field_a);
}

// Basic test verifying Archive_binary implementation, reading and writing
// of a structure aggregating other structures
//
// R4. When using binary format, serialization framework shall correctly
//     decode and encode messages having nested messages
//
// 1. Write a structure with a nested serializable structure, all fields are
//    provided. Use Serializer_default and Archive_binary
// 2. Decode data
// 3. Check that data read is equal to data written
// 4. Check that calculated size of the encoded data is equal to the result
//    of get_size function
TEST(Serialization, NestedMessage) {
  Format_a var_a;
  var_a.field_a = 10;
  var_a.field_b = 11;
  var_a.field_c = 12;
  var_a.compound_field_1.field_a_1 = 13;
  var_a.compound_field_1.field_a_2 = 14;
  var_a.field_d = 16;

  Serializer_default<Archive_binary> serializer;

  serializer << var_a;

  Format_a var_b;
  serializer >> var_b;
  ASSERT_EQ(serializer.get_archive().get_raw_data().size(),
            Serializer_default<Archive_binary>::get_size(var_a));

  ASSERT_EQ(var_a.field_a, var_b.field_a);
  ASSERT_EQ(var_a.field_b, var_b.field_b);
  ASSERT_EQ(var_a.field_c, var_b.field_c);
  ASSERT_EQ(var_a.compound_field_1.field_a_1, var_b.compound_field_1.field_a_1);
  ASSERT_EQ(var_a.compound_field_1.field_a_2, var_b.compound_field_1.field_a_2);
  ASSERT_EQ(var_a.field_d, var_b.field_d);
}

// structure with "optional fields, having defined encode predicates for
// specific fields and corresponding missing behaviors
struct Format_optional_field : public Serializable<Format_optional_field> {
  uint64_t field_a = 5;
  uint64_t field_b = 4;
  uint32_t field_c = 6;
  decltype(auto) define_fields() {
    return std::make_tuple(
        define_field(field_a),
        define_field(field_b, Field_missing_functor([this]() -> auto {
                       this->field_b = 10;
                     })),
        define_field(field_c, Field_missing_functor([this]() -> auto {
                       this->field_c = 100;
                     })));
  }

  decltype(auto) define_fields() const {
    return std::make_tuple(
        define_field(field_a),
        define_field(field_b, Field_encode_predicate([this]() -> auto {
                       return this->field_a == 2;
                     })),
        define_field(field_c));
  }
};

// Basic test verifying binary format implementation, reading and writing
// of a structure with basic, optional fields
//
// R5. When using binary format, serialization framework shall correctly
//     decode and encode messages having optional fields
//
// 1. Write a structure. Use Serializer_default and Archive_binary
// 2. Decode data
// 3. Check that provided fields are equal to encoded fields. Check that
//    fields missing in the encoded message are equal to values set in the
//    Field_missing_functor
// 4. Check that calculated size of the encoded data is equal to the result
//    of get_size function
TEST(Serialization, OptionalFields) {
  Format_optional_field var_a;
  var_a.field_a = 0;
  var_a.field_b = 2;
  var_a.field_c = 5;
  Serializer_default<Archive_binary> serializer;
  Format_optional_field var_b;

  serializer << var_a;
  serializer >> var_b;

  ASSERT_EQ(serializer.is_good(), true);
  ASSERT_EQ(serializer.get_archive().get_raw_data().size(),
            Serializer_default<Archive_binary>::get_size(var_a));

  ASSERT_EQ(var_a.field_a, var_b.field_a);
  ASSERT_EQ(10, var_b.field_b);
  ASSERT_EQ(var_a.field_c, var_b.field_c);
}

// structure aggregating other serializable structure (Format_optional_field),
// having both optional
// fields and defined "field missing" behavior
struct Format_optional_field_2 : public Serializable<Format_optional_field_2> {
  Format_optional_field field_1;
  uint64_t field_a = 5;
  uint64_t field_b = 4;
  uint32_t field_c = 6;
  decltype(auto) define_fields() {
    return std::make_tuple(
        define_compound_field(field_1), define_field(field_a),
        define_field(field_b, Field_missing_functor([this]() -> auto {
                       this->field_b = 10;
                     })),
        define_field(field_c));
  }

  decltype(auto) define_fields() const {
    return std::make_tuple(
        define_compound_field(field_1), define_field(field_a),
        define_field(field_b, Field_encode_predicate([this]() -> auto {
                       return this->field_a == 0;
                     })),
        define_field(field_c));
  }
};

// Test verifying binary format implementation, reading and writing
// of a structure with optional fields and nested serializable structure
//
// R6. When using binary format, serialization framework shall correctly
//     decode and encode nested messages having optional fields
//
// 1. Write a structure. Use Serializer_default and Archive_binary
// 2. Decode data
// 3. Check that provided fields are equal to encoded fields. Check that
//    fields missing in the encoded message are equal to values set in the
//    Field_missing_functor
// 4. Check that calculated size of the encoded data is equal to the result
//    of get_size function
TEST(Serialization, OptionalFieldsNested) {
  Format_optional_field_2 var_a;
  var_a.field_a = 1;
  var_a.field_b = 2;
  var_a.field_c = 5;
  Serializer_default<Archive_binary> serializer;
  Format_optional_field_2 var_b;

  serializer << var_a;
  serializer >> var_b;

  ASSERT_EQ(serializer.is_good(), true);
  ASSERT_EQ(serializer.get_archive().get_raw_data().size(),
            Serializer_default<Archive_binary>::get_size(var_a));

  ASSERT_EQ(var_a.field_a, var_b.field_a);
  ASSERT_EQ(10, var_b.field_b);
  ASSERT_EQ(var_a.field_c, var_b.field_c);
  ASSERT_EQ(var_a.field_1.field_a, var_b.field_1.field_a);
  ASSERT_EQ(10, var_b.field_1.field_b);
  ASSERT_EQ(var_a.field_1.field_c, var_b.field_1.field_c);
}

// structure having fixed-length integers
struct Format_defined_size : public Serializable<Format_defined_size> {
  uint64_t field_a = 5;
  uint64_t field_b = 2;
  decltype(auto) define_fields() {
    return std::make_tuple(define_field_with_size<5>(field_a),
                           define_field_with_size<6>(field_b));
  }

  decltype(auto) define_fields() const {
    return std::make_tuple(define_field_with_size<5>(field_a),
                           define_field_with_size<6>(field_b));
  }
};

// structure aggregating another serializable structure and having fixed-length
// integers
struct Format_defined_size2 : public Serializable<Format_defined_size2> {
  uint32_t field_a = 2;
  Format_defined_size field_b;
  Format_defined_size field_c;
  decltype(auto) define_fields() {
    return std::make_tuple(define_field_with_size<3>(field_a),
                           define_compound_field(field_b),
                           define_compound_field(field_c));
  }

  decltype(auto) define_fields() const {
    return std::make_tuple(define_field_with_size<3>(field_a),
                           define_compound_field(field_b),
                           define_compound_field(field_c));
  }
};

// Test verifying binary format implementation, reading and writing
// of a structure with fixed integers
//
// R7. When using binary format, serialization framework shall correctly
//     decode and encode messages having fixed-length integers
//
// 1. Write a structure. Use Serializer_default and Archive_binary
//    Provide all fields
// 2. Decode data
// 3. Check that provided fields are equal to encoded fields.
// 4. Check that calculated size of the encoded data is equal to the result
//    of get_size function and equal to expected size of the message
TEST(Serialization, FixedSize) {
  Format_defined_size var_a;
  var_a.field_a = 55;
  Serializer_default<Archive_binary> serializer;
  Format_defined_size var_b;

  serializer << var_a;
  serializer >> var_b;
  ASSERT_EQ(serializer.is_good(), true);

  ASSERT_EQ(serializer.get_archive().get_raw_data().size(),
            serializable_overhead_small + 1 + 5 + 1 + 6);
  ASSERT_EQ(serializer.get_archive().get_raw_data().size(),
            Serializer_default<Archive_binary>::get_size(var_a));

  ASSERT_EQ(var_a.field_a, var_b.field_a);
  ASSERT_EQ(var_a.field_b, var_b.field_b);
}

// Test verifying binary format implementation, reading and writing
// of a structure aggregating another serializable structure having
// fixed size integers fields
//
// R8. When using binary format, serialization framework shall correctly
//     decode and encode nested messages having fixed-length integers
//
// 1. Write a structure. Use Serializer_default and Archive_binary
//    Provide all fields.
// 2. Decode data
// 3. Check that provided fields are equal to encoded fields.
// 4. Check that calculated size of the encoded data is equal to the result
//    of get_size function and equal to expected size of the message
// 5. Check that calculated maximum size of the encoded message is as expected
TEST(Serialization, FixedSizeNestedMessage) {
  Format_defined_size2 var_a;

  using Serializer_type = Serializer_default<Archive_binary>;

  Serializer_type serializer;
  Format_defined_size2 var_b;

  serializer << var_a;
  serializer >> var_b;
  ASSERT_EQ(serializer.is_good(), true);

  ASSERT_EQ(serializer.get_archive().get_raw_data().size(),
            serializable_overhead_small + 1 + 3 +
                (serializable_overhead_small + 1 + 5 + 1 + 6) * 2);

  ASSERT_EQ(
      Serializer_type::get_max_size<Format_defined_size2>(),
      serializable_overhead_max + id_max_size + 3 +
          (serializable_overhead_max + id_max_size + 5 + id_max_size + 6) * 2);

  ASSERT_EQ(Serializer_type::get_size(var_a),
            serializer.get_archive().get_raw_data().size());

  ASSERT_EQ(var_a.field_a, var_b.field_a);
  ASSERT_EQ(var_a.field_b.field_a, var_a.field_b.field_a);
  ASSERT_EQ(var_a.field_b.field_b, var_a.field_b.field_b);
  ASSERT_EQ(var_a.field_c.field_a, var_a.field_c.field_a);
  ASSERT_EQ(var_a.field_c.field_b, var_a.field_c.field_b);
}

// Structure having floating-point fields
struct Format_float : public Serializable<Format_float> {
  double field_a = 2;
  float field_b = 4;
  decltype(auto) define_fields() const {
    return std::make_tuple(define_field(field_a), define_field(field_b));
  }
  decltype(auto) define_fields() {
    return std::make_tuple(define_field(field_a), define_field(field_b));
  }
};

// Test verifying binary format implementation, reading and writing
// of a structure having a floating point fields defined
//
// R9. When using binary format, serialization framework shall correctly
//     decode and encode nested messages having floating point fields
//
// 1. Write a structure. Use Serializer_default and Archive_binary
//    Provide all fields.
// 2. Decode data
// 3. Check that provided fields are equal to encoded fields.
// 4. Check that calculated size of the encoded data is equal to the result
//    of get_size function and equal to expected size of the message
TEST(Serialization, FloatFields) {
  Format_float var_a;
  Format_float var_b;

  var_a.field_a = 6.5;
  var_a.field_b = 45.6654f;

  using Serializer_type = Serializer_default<Archive_binary>;

  Serializer_type serializer;

  serializer << var_a;
  serializer >> var_b;
  ASSERT_EQ(serializer.is_good(), true);

  size_t expected_size = 8 + 4 + 2 + serializable_overhead_small;

  ASSERT_EQ(Serializer_type::get_size(var_a), expected_size);
  ASSERT_EQ(serializer.get_archive().get_raw_data().size(), expected_size);

  ASSERT_EQ(var_a.field_a, var_b.field_a);
  ASSERT_EQ(var_a.field_b, var_b.field_b);
}

// structure having a string field
struct Format_string : public Serializable<Format_string> {
  std::string field_string = "expr_ex";
  decltype(auto) define_fields() const {
    return std::make_tuple(define_field_with_size<32>(field_string));
  }
  decltype(auto) define_fields() {
    return std::make_tuple(define_field_with_size<32>(field_string));
  }
};

// Test verifying binary format implementation, reading and writing
// of a structure having a string field defined
//
// R10. When using binary format, serialization framework shall correctly
//      decode and encode nested messages having floating point fields
//
// 1. Write a structure. Use Serializer_default and Archive_binary
//    Provide all fields.
// 2. Decode data
// 3. Check that provided fields are equal to encoded fields.
// 4. Check that calculated size of the encoded data is equal to the result
//    of get_size function and equal to expected size of the message
TEST(Serialization, StringField) {
  Format_string var_a;
  Format_string var_b;

  var_a.field_string = "expr_expr_e";

  using Serializer_type = Serializer_default<Archive_binary>;

  Serializer_type serializer;

  serializer << var_a;
  serializer >> var_b;
  ASSERT_EQ(serializer.is_good(), true);

  size_t expected_size =
      serializable_overhead_small + 1 +
      Byte_count_helper<0>::count_write_bytes(var_a.field_string.length()) +
      var_a.field_string.length();

  ASSERT_EQ(Serializer_type::get_size(var_a), expected_size);
  ASSERT_EQ(serializer.get_archive().get_raw_data().size(), expected_size);
  ASSERT_EQ(serializer.get_archive().get_raw_data().size(),
            Serializer_default<Archive_binary>::get_size(var_a));
  ASSERT_EQ(var_a.field_string, var_b.field_string);
}

// Test verifying Write_archive_binary/Read_archive_binary implementation,
// needed for integration with mysql_binlog_event API
//
// R11. When using different implementation of binary archives, the
//      serialization framework shall be able to correctly decode encoded
//      data.
//
// 1. Write a structure. Use Write_archive_binary
// 2. Decode data using Read_archive_binary
// 3. Check that provided fields are equal to encoded fields.
// 4. Check that calculated size of the encoded data is equal to the result
//    of get_size function and equal to expected size of the message
// 5. Check that calculated size of the encoded data is equal to the size
//    calculated with Serializer_default<Archive_binary>
// 6. Try to write more bytes into the stream than maximum size of the stream
// 7. Observe that encoder returned the
// Serialization_error_type::archive_write_error
TEST(Serialization, StringFieldReadWriteArchive) {
  Format_string var_a;
  Format_string var_b;

  var_a.field_string = "expr_expr_e";

  using Write_serializer_type = Serializer_default<Write_archive_binary>;
  using Read_serializer_type = Serializer_default<Read_archive_binary>;

  auto max_size =
      Serializer_default<Archive_binary>::get_max_size<Format_string>();
  std::vector<unsigned char> data(max_size);

  Write_serializer_type encoder;
  encoder.get_archive().set_stream(data.data(), max_size);

  ASSERT_EQ(data.data(), encoder.get_archive().get_raw_data());

  Read_serializer_type decoder;
  decoder.get_archive().set_stream(data.data(), max_size);

  encoder << var_a;
  decoder >> var_b;
  ASSERT_EQ(encoder.is_good(), true);
  ASSERT_EQ(decoder.is_good(), true);

  size_t expected_size =
      var_a.field_string.length() +
      Byte_count_helper<0>::count_write_bytes(var_a.field_string.length()) + 1 +
      serializable_overhead_small;

  ASSERT_EQ(Write_serializer_type::get_size(var_a), expected_size);
  ASSERT_EQ(encoder.get_archive().get_size_written(), expected_size);
  ASSERT_EQ(encoder.get_archive().get_size_written(),
            Serializer_default<Archive_binary>::get_size(var_a));
  ASSERT_EQ(var_a.field_string, var_b.field_string);

  encoder.get_archive().set_stream(data.data(), 5);
  encoder << var_a;
  ASSERT_EQ(encoder.is_good(), false);
  ASSERT_EQ(encoder.get_error().get_type(),
            Serialization_error_type::archive_write_error);
}

// structure containing enumeration fields
struct Format_enum : public Serializable<Format_enum> {
  My_enum field_enum = My_enum::state2;
  decltype(auto) define_fields() const {
    return std::make_tuple(define_field(field_enum));
  }
  decltype(auto) define_fields() {
    return std::make_tuple(define_field(field_enum));
  }
};

// structure containing enumeration fields, new enumeration constant w.r.t.
// Format_enum
struct Format_enum_v2 : public Serializable<Format_enum_v2> {
  My_enum_v2 field_enum = My_enum_v2::state3;
  decltype(auto) define_fields() const {
    return std::make_tuple(define_field(field_enum));
  }
  decltype(auto) define_fields() {
    return std::make_tuple(define_field(field_enum));
  }
};

// Test verifying binary format implementation, reading and writing
// of a structure having a string field defined
//
// R12. When using binary format, serialization framework shall correctly
//      decode and encode enumeration fields
//
// 1. Write a structure. Use Serializer_default and Archive_binary
//    Provide all fields.
// 2. Decode data
// 3. Check that provided fields are equal to encoded fields.
// 4. Check that calculated size of the encoded data is equal to the result
//    of get_size function and equal to expected size of the message
TEST(Serialization, EnumField) {
  Format_enum_v2 var_a;
  Format_enum var_b;

  var_a.field_enum = My_enum_v2::state3;

  using Serializer_type = Serializer_default<Archive_binary>;
  Serializer_type serializer;

  serializer << var_a;
  serializer >> var_b;
  ASSERT_EQ(serializer.is_good(), true);
  ASSERT_EQ(serializer.get_archive().get_raw_data().size(),
            Serializer_default<Archive_binary>::get_size(var_a));

  size_t expected_size = serializable_overhead_small + 1 + 1;

  ASSERT_EQ(Serializer_type::get_size(var_a), expected_size);
  ASSERT_EQ(serializer.get_archive().get_raw_data().size(), expected_size);
  ASSERT_EQ(mysql::utils::to_underlying(var_a.field_enum),
            mysql::utils::to_underlying(var_b.field_enum));
}

// Test verifying binary format implementation, reading and writing
// of a structure having a string field defined
//
// R12. When using binary format, serialization framework shall correctly
//      decode and encode enumeration fields
//
// 1. Write a structure with format *new*. Use Serializer_default and
//    Archive_binary. Provide all fields.
// 2. Decode data to structure *old*.
// 3. Check that decoding returned
//    the Serialization_error_type::data_integrity_error
TEST(Serialization, EnumFieldError) {
  Format_enum_v2 var_a;
  Format_enum var_b;

  var_a.field_enum = My_enum_v2::state4;

  using Serializer_type = Serializer_default<Archive_binary>;
  Serializer_type serializer;

  serializer << var_a;
  serializer >> var_b;
  ASSERT_EQ(serializer.is_good(), false);
  ASSERT_EQ(serializer.get_error().get_type(),
            Serialization_error_type::data_integrity_error);
}

// Structure containing a vector field
struct Format_vector : public Serializable<Format_vector> {
  std::vector<uint64_t> field = {0, 1, 2};
  decltype(auto) define_fields() const {
    return std::make_tuple(define_field(field));
  }
  decltype(auto) define_fields() {
    return std::make_tuple(define_field_with_size<sizeof(uint64_t)>(field));
  }
};

// Test verifying binary format implementation, reading and writing
// of a structure having a vector field of simple types defined
//
// R13. When using binary format, serialization framework shall correctly
//      decode and encode supported containers
//
// 1. Write a structure. Use Serializer_default and Archive_binary
//    Provide all fields.
// 2. Decode data
// 3. Check that provided fields are equal to encoded fields.
// 4. Check that calculated size of the encoded data is equal to the result
//    of get_size function.
TEST(Serialization, VectorField) {
  Format_vector var_a;
  Format_vector var_b;

  using Serializer_type = Serializer_default<Archive_binary>;

  Serializer_type serializer;

  var_a.field.push_back(5);

  serializer << var_a;
  serializer >> var_b;
  ASSERT_EQ(serializer.is_good(), true);
  ASSERT_EQ(serializer.get_archive().get_raw_data().size(),
            Serializer_default<Archive_binary>::get_size(var_a));

  ASSERT_EQ(var_a.field.size(), var_b.field.size());
  for (std::size_t id = 0; id < var_a.field.size(); ++id) {
    ASSERT_EQ(var_a.field[id], var_b.field[id]);
  }
}

// Simple serializable struct aggregated in the vector contained in the
// Format_vector_compound
struct Serializable_pair : public Serializable<Serializable_pair> {
  uint32_t first;
  uint64_t second;
  Serializable_pair() {}
  Serializable_pair(uint32_t f, uint32_t s) : first(f), second(s) {}
  decltype(auto) define_fields() const {
    return std::make_tuple(define_field(first), define_field(second));
  }
  decltype(auto) define_fields() {
    return std::make_tuple(define_field(first), define_field(second));
  }
};

// structure having a vector field of other serializable formats
struct Format_vector_compound : public Serializable<Format_vector_compound> {
  std::vector<Serializable_pair> field;
  decltype(auto) define_fields() const {
    return std::make_tuple(define_field(field));
  }
  decltype(auto) define_fields() {
    return std::make_tuple(define_field(field));
  }
};

// Test verifying binary format implementation, reading and writing
// of a structure having a vector field of other serializable objects
//
// R14. Nested messages should be able to be kept in any of containers
//      supported by serialization framework
//
// 1. Write a structure. Use Serializer_default and Archive_binary
//    Provide all fields.
// 2. Decode data
// 3. Check that provided fields are equal to encoded fields.
// 4. Check that calculated size of the encoded data is equal to the result
//    of get_size function and equal to expected size of the message
TEST(Serialization, VectorFieldWithSerializable) {
  Format_vector_compound var_a;
  Format_vector_compound var_b;

  using Serializer_type = Serializer_default<Archive_binary>;

  Serializer_type serializer;

  var_a.field.push_back(Serializable_pair(1, 3));
  var_a.field.push_back(Serializable_pair(2, 4));

  serializer << var_a;
  serializer >> var_b;
  ASSERT_EQ(serializer.is_good(), true);

  ASSERT_EQ(serializer.get_archive().get_raw_data().size(),
            Serializer_type::get_size(var_a));

  ASSERT_EQ(var_a.field.size(), var_b.field.size());
  for (std::size_t id = 0; id < var_a.field.size(); ++id) {
    ASSERT_EQ(var_a.field[id].first, var_b.field[id].first);
    ASSERT_EQ(var_a.field[id].second, var_b.field[id].second);
  }
}

// Serializable structure having a map field
struct Format_map : public Serializable<Format_map> {
  std::map<uint32_t, uint32_t> field;
  decltype(auto) define_fields() const {
    return std::make_tuple(define_field(field));
  }
  decltype(auto) define_fields() {
    return std::make_tuple(define_field(field));
  }
};

// Test verifying binary format implementation, reading and writing
// of a structure having a map field with simple key and value
//
// R13. When using binary format, serialization framework shall correctly
//      decode and encode supported containers
//
// 1. Write a structure. Use Serializer_default and Archive_binary
//    Provide all fields.
// 2. Decode data
// 3. Check that provided fields are equal to encoded fields.
// 4. Check that calculated size of the encoded data is equal to the result
//    of get_size function and equal to expected size of the message
TEST(Serialization, MapField) {
  Format_map var_a;
  Format_map var_b;

  using Serializer_type = Serializer_default<Archive_binary>;

  Serializer_type serializer;

  var_a.field.insert(std::make_pair(5, 6));
  var_a.field.insert(std::make_pair(7, 8));
  var_a.field.insert(std::make_pair(9, 1));

  serializer << var_a;
  serializer >> var_b;
  ASSERT_EQ(serializer.is_good(), true);
  ASSERT_EQ(serializer.get_archive().get_raw_data().size(),
            Serializer_type::get_size(var_a));

  ASSERT_EQ(var_a.field.size(), var_b.field.size());
  for (const auto &ff : var_a.field) {
    ASSERT_EQ(ff.second, var_b.field.at(ff.first));
  }
}

// Serializable structure with a map field, keeping nested structure as
// a value
struct Format_map_compound : public Serializable<Format_map_compound> {
  std::map<uint32_t, Serializable_pair> field;
  decltype(auto) define_fields() const {
    return std::make_tuple(define_field(field));
  }
  decltype(auto) define_fields() {
    return std::make_tuple(define_field(field));
  }
};

// Test verifying binary format implementation, reading and writing
// of a structure having a map field of other serializable objects
//
// R14. Nested messages should be able to be kept in any of containers
//      supported by serialization framework
//
// 1. Write a structure. Use Serializer_default and Archive_binary
//    Provide all fields.
// 2. Decode data
// 3. Check that provided fields are equal to encoded fields.
// 4. Check that calculated size of the encoded data is equal to the result
//    of get_size function and equal to expected size of the message
TEST(Serialization, MapFieldSerializable) {
  Format_map_compound var_a;
  Format_map_compound var_b;

  using Serializer_type = Serializer_default<Archive_binary>;

  Serializer_type serializer;

  var_a.field.insert(std::make_pair(5, Serializable_pair(4, 5)));
  var_a.field.insert(std::make_pair(7, Serializable_pair(8, 9)));
  var_a.field.insert(std::make_pair(9, Serializable_pair(1, 2)));

  serializer << var_a;
  serializer >> var_b;
  ASSERT_EQ(serializer.is_good(), true);
  ASSERT_EQ(serializer.get_archive().get_raw_data().size(),
            Serializer_type::get_size(var_a));

  ASSERT_EQ(var_a.field.size(), var_b.field.size());
  for (const auto &ff : var_a.field) {
    ASSERT_EQ(ff.second.first, var_b.field.at(ff.first).first);
    ASSERT_EQ(ff.second.second, var_b.field.at(ff.first).second);
  }
}

// Test verifying Read_archive_binary implementation,
// needed for integration with mysql_binlog_event API and its consistency
// with implementation of Archive_binary
//
// R15. When implementing different API of the same encoding, it should be
//      possible to decode the data using different implementation of an
//      archive.
//
// 1. Write a structure. Use Archive_binary
// 2. Decode data using Read_archive_binary
// 3. Check that provided fields are equal to encoded fields.
// 4. Check that calculated size of the encoded data is equal to the result
//    of get_size function
// 5. Check that calculated size of the encoded data is equal to the size
//    calculated with Serializer_default<Archive_binary>
TEST(Serialization, ReadArchive) {
  Format_map_compound var_a;
  Format_map_compound var_b;

  using Serializer_type = Serializer_default<Archive_binary>;

  using Serializer_readonly_type = Serializer_default<Read_archive_binary>;

  Serializer_type serializer;

  Serializer_readonly_type read_only_serializer;

  var_a.field.insert(std::make_pair(5, Serializable_pair(4, 5)));
  var_a.field.insert(std::make_pair(7, Serializable_pair(8, 9)));
  var_a.field.insert(std::make_pair(9, Serializable_pair(1, 2)));

  serializer << var_a;
  ASSERT_EQ(serializer.get_archive().get_raw_data().size(),
            Serializer_type::get_size(var_a));

  // capture the stream here
  read_only_serializer.get_archive().set_stream(
      serializer.get_archive().get_raw_data().data(),
      serializer.get_archive().get_raw_data().size());

  read_only_serializer >> var_b;

  ASSERT_EQ(serializer.is_good(), true);
  ASSERT_EQ(read_only_serializer.is_good(), true);

  ASSERT_EQ(var_a.field.size(), var_b.field.size());
  for (const auto &ff : var_a.field) {
    ASSERT_EQ(ff.second.first, var_b.field.at(ff.first).first);
    ASSERT_EQ(ff.second.second, var_b.field.at(ff.first).second);
  }
}

static constexpr std::size_t format_char_array_field_size = 16;

// Serializable structure having an array of bytes
struct Format_char_array : public Serializable<Format_char_array> {
  std::array<unsigned char, format_char_array_field_size> field;
  decltype(auto) define_fields() const {
    return std::make_tuple(
        define_field_with_size<format_char_array_field_size>(field));
  }
  decltype(auto) define_fields() {
    return std::make_tuple(
        define_field_with_size<format_char_array_field_size>(field));
  }
};

// Serializable structure having a C array of bytes
struct Format_char_carray : public Serializable<Format_char_carray> {
  unsigned char field[format_char_array_field_size];
  decltype(auto) define_fields() const {
    return std::make_tuple(
        define_field_with_size<format_char_array_field_size>(field));
  }
  decltype(auto) define_fields() {
    return std::make_tuple(
        define_field_with_size<format_char_array_field_size>(field));
  }
};

// Test verifying binary format implementation, reading and writing
// of a structure having an array / C array of bytes
//
// R13. When using binary format, serialization framework shall correctly
//      decode and encode supported containers
//
// 1. Write a structure. Use Serializer_default and Archive_binary
//    Provide all fields.
// 2. Decode data
// 3. Check that provided fields are equal to encoded fields.
// 4. Check that calculated size of the encoded data is equal to the result
//    of get_size function.
TEST(Serialization, CharArrayField) {
  Format_char_array var_a;
  Format_char_array var_b;

  Format_char_carray var_ac;
  Format_char_carray var_bc;

  for (std::size_t id = 0; id < format_char_array_field_size; ++id) {
    var_a.field[id] = (unsigned char)(id);
    var_ac.field[id] = (unsigned char)(id);
  }

  using Serializer_type = Serializer_default<Archive_binary>;

  Serializer_type serializer;

  serializer << var_a << var_ac;
  serializer >> var_b >> var_bc;

  size_t expected_size =
      2 * (serializable_overhead_small + 1 + format_char_array_field_size);

  ASSERT_EQ(serializer.get_archive().get_raw_data().size(), expected_size);

  ASSERT_EQ(
      serializer.get_archive().get_raw_data().size(),
      Serializer_type::get_size(var_a) + Serializer_type::get_size(var_ac));
  ASSERT_EQ(serializer.is_good(), true);

  for (std::size_t id = 0; id < format_char_array_field_size; ++id) {
    ASSERT_EQ(var_a.field[id], var_b.field[id]);
    ASSERT_EQ(var_ac.field[id], var_bc.field[id]);
  }
}

// Serializable structure which emulates a structure defined in the first
// version of the software
struct Format_internal_v1 : public Serializable<Format_internal_v1> {
  uint64_t field_1a = 0;
  uint64_t field_1b = 0;
  decltype(auto) define_fields() const {
    return std::make_tuple(
        define_field(field_1a, Field_encode_predicate([this]() -> auto {
                       return field_1a == 5;
                     })),
        define_field(field_1b, Field_encode_predicate([this]() -> auto {
                       return field_1b == 5;
                     })));
  }
  decltype(auto) define_fields() {
    return std::make_tuple(
        define_field(field_1a,
                     Field_missing_functor([this]() -> auto { field_1a = 6; })),
        define_field(field_1b, Field_missing_functor([this]() -> auto {
                       field_1b = 6;
                     })));
  }
};

// Serializable structure which emulates a structure defined in the second
// version of the software (Format_internal_v1 with fields added in version 2)
struct Format_internal_v2 : public Serializable<Format_internal_v2> {
  uint64_t field_1a = 0;
  uint64_t field_1b = 0;
  uint32_t field_1c = 0;           // added 1 field to Format_internal_v1
  std::vector<uint32_t> field_1d;  // added 2 field to Format_internal_v1
  decltype(auto) define_fields() const {
    return std::make_tuple(
        define_field(field_1a, Field_encode_predicate([this]() -> auto {
                       return field_1a == 5;
                     })),
        define_field(field_1b, Field_encode_predicate([this]() -> auto {
                       return field_1b == 5;
                     })),
        define_field(field_1c, Field_encode_predicate([this]() -> auto {
                       return field_1c == 5;
                     })),
        define_field(field_1d, Field_encode_predicate([this]() -> auto {
                       return field_1d.size() > 0;
                     })));
  }
  decltype(auto) define_fields() {
    return std::make_tuple(
        define_field(field_1a,
                     Field_missing_functor([this]() -> auto { field_1a = 6; })),
        define_field(field_1b,
                     Field_missing_functor([this]() -> auto { field_1b = 6; })),
        define_field(field_1c,
                     Field_missing_functor([this]() -> auto { field_1c = 7; })),
        define_field(field_1d, Field_missing_functor([this]() -> auto {
                       field_1d.push_back(9);
                     })));
  }
};

// Serializable structure which emulates a structure defined in the first
// version of the software, which aggregates another serializable structure
struct Format_v1 : public Serializable<Format_v1> {
  std::vector<uint32_t> field_aa;
  Format_internal_v1 field_bb;
  uint32_t field_cc = 0;
  decltype(auto) define_fields() const {
    return std::make_tuple(
        define_field_with_size<sizeof(uint32_t)>(
            field_aa, Field_encode_predicate([this]() -> auto {
              return field_aa.size() > 0;
            })),
        define_compound_field(field_bb),
        define_field(field_cc, Field_encode_predicate([this]() -> auto {
                       return field_cc == 9;
                     })));
  }
  decltype(auto) define_fields() {
    return std::make_tuple(
        define_field_with_size<sizeof(uint32_t)>(
            field_aa,
            Field_missing_functor([this]() -> auto { field_aa.push_back(7); })),
        define_compound_field(field_bb),
        define_field(field_cc, Field_missing_functor([this]() -> auto {
                       field_cc = 19;
                     })));
  }
};

// Serializable structure which emulates a structure defined in the second
// version of the software (Format_v1 with fields added in version 2). In
// addition, this structure aggregates another structure also extended in the
// same version (Format_internal_v1 with fields added in version 2)
struct Format_v2 : public Serializable<Format_v2> {
  std::vector<uint32_t> field_aa;
  Format_internal_v2 field_bb;  // contains updated internal type
  uint32_t field_cc = 0;
  decltype(auto) define_fields() const {
    return std::make_tuple(
        define_field_with_size<sizeof(uint32_t)>(
            field_aa, Field_encode_predicate([this]() -> auto {
              return field_aa.size() > 0;
            })),
        define_compound_field(field_bb),
        define_field(field_cc, Field_encode_predicate([this]() -> auto {
                       return field_cc == 9;
                     })));
  }
  decltype(auto) define_fields() {
    return std::make_tuple(
        define_field_with_size<sizeof(uint32_t)>(
            field_aa,
            Field_missing_functor([this]() -> auto { field_aa.push_back(7); })),
        define_compound_field(field_bb),
        define_field(field_cc, Field_missing_functor([this]() -> auto {
                       field_cc = 19;
                     })));
  }
};

// Test verifying binary format implementation, backward compatibility of
// serializable structures with simple fields
//
// R16. When using binary format, serialization framework shall allow for
//      the extension of the message, keeping at the same time backward
//      compatibility with message format defined in earlier versions
//      of the software (old->new)
//
// 1. Write a structure, old format. Provide all fields.
// 2. Decode data to structure *new*.
// 3. Check expected size of the message.
// 4. Check that fields provided are correctly encoded.
// 5. Check that missing functors defined for *new* fields were run
TEST(Serialization, BackwardCompatibility) {
  Format_internal_v1 var_a;  // not filled

  Format_internal_v1 var_b;  // filled
  var_b.field_1a = 5;
  var_b.field_1b = 5;

  using Serializer_type = Serializer_default<Archive_binary>;

  Serializer_type serializer_a;

  Serializer_type serializer_b;

  serializer_a << var_a;
  ASSERT_EQ(serializer_a.is_good(), true);

  serializer_b << var_b;
  ASSERT_EQ(serializer_b.is_good(), true);

  Format_internal_v2 var_a_rec;
  Format_internal_v2 var_b_rec;

  serializer_a >> var_a_rec;
  // This is how to receive error message:
  // std::cout << serializer_a.get_error().what() << std::endl;
  ASSERT_EQ(serializer_a.is_good(), true);

  // check that saved data contains only serializable overhead
  // (serializable_overhead_small)
  ASSERT_EQ(serializer_a.get_archive().get_raw_data().size(),
            serializable_overhead_small);

  // check that field missing functors were run for all of the fields
  ASSERT_EQ(var_a_rec.field_1a, 6);
  ASSERT_EQ(var_a_rec.field_1b, 6);
  ASSERT_EQ(var_a_rec.field_1c, 7);
  ASSERT_EQ(var_a_rec.field_1d.size(), 1);
  ASSERT_EQ(var_a_rec.field_1d.at(0), 9);

  serializer_b >> var_b_rec;
  ASSERT_EQ(serializer_b.is_good(), true);

  // check that old format fields were read
  ASSERT_EQ(var_b_rec.field_1a, 5);
  ASSERT_EQ(var_b_rec.field_1b, 5);

  // for new fields, check that field missing functors were run
  ASSERT_EQ(var_b_rec.field_1c, 7);
  ASSERT_EQ(var_b_rec.field_1d.size(), 1);
  ASSERT_EQ(var_b_rec.field_1d.at(0), 9);
}

// Test verifying binary format implementation, backward compatibility of
// serializable structures with simple fields
//
// R17. When using binary format, serialization framework shall allow for
//      the extension of the message, keeping at the same time forward
//      compatibility with message format defined in future versions
//      of the software (new->old)
//
// 1. Write a structure, new format. Provide all fields.
// 2. Decode data to structure *old*.
// 3. Check expected size of the message.
// 4. Check that message was successfully decoded.
// 5. Check that all fields of *old* structure have expected values.
TEST(Serialization, ForwardCompatibility) {
  Format_internal_v2 var_a;  // not filled, partially
  var_a.field_1d.push_back(99);

  Format_internal_v2 var_b;  // filled
  var_b.field_1a = 5;
  var_b.field_1b = 5;
  var_b.field_1c = 5;
  var_b.field_1d.push_back(1);
  var_b.field_1d.push_back(2);

  using Serializer_type = Serializer_default<Archive_binary>;

  Serializer_type serializer_a;

  Serializer_type serializer_b;

  serializer_a << var_a;
  ASSERT_EQ(serializer_a.is_good(), true);

  serializer_b << var_b;
  ASSERT_EQ(serializer_b.is_good(), true);

  Format_internal_v1 var_a_rec;
  Format_internal_v1 var_b_rec;

  serializer_a >> var_a_rec;
  ASSERT_EQ(serializer_a.is_good(), true);

  // check saved data size
  ASSERT_EQ(serializer_a.get_archive().get_raw_data().size(),
            serializable_overhead_small + 1 + 1 + 1);

  // check that field missing functors were run for all of the fields
  ASSERT_EQ(var_a_rec.field_1a, 6);
  ASSERT_EQ(var_a_rec.field_1b, 6);

  serializer_b >> var_b_rec;
  ASSERT_EQ(serializer_b.is_good(), true);

  // check that old format fields were read
  ASSERT_EQ(var_b_rec.field_1a, 5);
  ASSERT_EQ(var_b_rec.field_1b, 5);
}

// Test verifying binary format implementation, backward compatibility of
// serializable structures aggregating other serializable structures
//
// R16. When using binary format, serialization framework shall allow for
//      the extension of the message, keeping at the same time backward
//      compatibility with message format defined in earlier versions
//      of the software (old->new)
//
// 1. Write a structure, old format. Provide a subset of fields.
// 2. Decode data to structure *new*.
// 3. Check that fields provided are correctly encoded
// 4. Check that missing functors defined for *new* fields and fields not
//    provided (including fields unknown to *old*), were run
TEST(Serialization, BackwardCompatibilityNestedMessages) {
  Format_v1 var_a;  // not filled

  Format_v1 var_b;  // filled
  var_b.field_aa.push_back(10);
  var_b.field_aa.push_back(11);
  var_b.field_bb.field_1a = 5;
  var_b.field_bb.field_1b = 5;
  var_b.field_cc = 9;

  using Serializer_type = Serializer_default<Archive_binary>;

  Serializer_type serializer_a;

  Serializer_type serializer_b;

  serializer_a << var_a;
  ASSERT_EQ(serializer_a.is_good(), true);

  serializer_b << var_b;
  ASSERT_EQ(serializer_b.is_good(), true);

  Format_v2 var_a_rec;
  Format_v2 var_b_rec;

  serializer_a >> var_a_rec;

  ASSERT_EQ(serializer_a.is_good(), true);

  // check that field missing functors were run for missing fields
  ASSERT_EQ(var_a_rec.field_bb.field_1a, 6);
  ASSERT_EQ(var_a_rec.field_bb.field_1b, 6);
  ASSERT_EQ(var_a_rec.field_bb.field_1c, 7);
  ASSERT_EQ(var_a_rec.field_bb.field_1d.size(), 1);
  ASSERT_EQ(var_a_rec.field_bb.field_1d.at(0), 9);
  ASSERT_EQ(var_a_rec.field_aa.size(), 1);
  ASSERT_EQ(var_a_rec.field_aa.at(0), 7);
  ASSERT_EQ(var_a_rec.field_cc, 19);

  serializer_b >> var_b_rec;
  ASSERT_EQ(serializer_b.is_good(), true);

  // check that old format fields were read correctly
  ASSERT_EQ(var_b_rec.field_bb.field_1a, 5);
  ASSERT_EQ(var_b_rec.field_bb.field_1b, 5);
  ASSERT_EQ(var_b_rec.field_aa.size(), 2);
  ASSERT_EQ(var_b_rec.field_aa.at(0), 10);
  ASSERT_EQ(var_b_rec.field_aa.at(1), 11);
  ASSERT_EQ(var_b_rec.field_cc, 9);

  // for new fields, check that field missing functors were run
  ASSERT_EQ(var_b_rec.field_bb.field_1c, 7);
  ASSERT_EQ(var_b_rec.field_bb.field_1d.size(), 1);
  ASSERT_EQ(var_b_rec.field_bb.field_1d.at(0), 9);
}

// Serializable structure which emulates a structure defined in the second
// version of the software (new serializable defined inside of
// Format_new_nested).
struct Nested : public Serializable<Nested> {
  uint32_t field_na = 0;
  uint32_t field_nb = 0;
  decltype(auto) define_fields() {
    return std::make_tuple(
        define_field(field_na,
                     Field_missing_functor([this]() -> auto { field_na = 1; })),
        define_field(field_nb,
                     Field_missing_functor([this]() -> auto { field_nb = 2; }))

    );
  }
  decltype(auto) define_fields() const {
    return std::make_tuple(define_field(field_na), define_field(field_nb));
  }
};

// Serializable structure which emulates a structure defined in the second
// version of the software (Format_no_nested_old with new nested serializable
// defined in version 2).
struct Format_new_nested : public Serializable<Format_new_nested> {
  uint32_t a;
  Nested b;

  decltype(auto) define_fields() const {
    return std::make_tuple(define_field(a), define_compound_field(b));
  }
  decltype(auto) define_fields() {
    return std::make_tuple(define_field(a), define_compound_field(b));
  }
};

// Serializable structure which emulates a structure defined in the first
// version of the software (single field).
struct Format_no_nested_old : public Serializable<Format_no_nested_old> {
  uint32_t a = 0;

  decltype(auto) define_fields() const {
    return std::make_tuple(define_field(a));
  }
  decltype(auto) define_fields() { return std::make_tuple(define_field(a)); }
};

// Test verifying binary format implementation, backward compatibility of
// serializable structures aggregating other serializable structures. This
// time we test on whether missing functors for a missing nested message
// have been run
//
// R16. When using binary format, serialization framework shall allow for
//      the extension of the message, keeping at the same time backward
//      compatibility with message format defined in earlier versions
//      of the software (old->new)
//
// 1. Write a structure, old format. Provide all fields
// 2. Decode data to structure *new*.
// 3. Check that fields provided are correctly encoded
// 4. Check that missing functors defined for *new* fields and fields not
//    provided (including fields unknown to *old*), were run
TEST(Serialization, BackwardCompatibilityNewNested) {
  Format_no_nested_old var_a;  // filled
  Format_new_nested var_a_rec;

  using Serializer_type = Serializer_default<Archive_binary>;

  Serializer_type serializer;

  serializer << var_a;
  ASSERT_EQ(serializer.is_good(), true);

  serializer >> var_a_rec;

  ASSERT_EQ(serializer.is_good(), true);

  // check that field missing functors were run for missing fields
  ASSERT_EQ(var_a_rec.a, var_a.a);
  ASSERT_EQ(var_a_rec.b.field_na, 1);
  ASSERT_EQ(var_a_rec.b.field_nb, 2);
}

// Test verifying binary format implementation, backward compatibility of
// serializable structures aggregating other serializable structures
//
// R17. When using binary format, serialization framework shall allow for
//      the extension of the message, keeping at the same time forward
//      compatibility with message format defined in future versions
//      of the software (new->old)
//
// 1. Write a structure, new format. Provide a subset fields.
// 2. Decode data to structure *old*.
// 3. Check that message was successfully decoded.
// 4. Check that all fields of *old* structure have expected values.
TEST(Serialization, ForwardCompatibilityNestedMessages) {
  Format_v2 var_a;  // not filled, partially
  var_a.field_bb.field_1d.push_back(99);

  Format_v2 var_b;  // filled
  var_b.field_bb.field_1a = 5;
  var_b.field_bb.field_1b = 5;
  var_b.field_bb.field_1c = 5;
  var_b.field_bb.field_1d.push_back(1);
  var_b.field_bb.field_1d.push_back(2);

  var_b.field_aa.push_back(10);
  var_b.field_aa.push_back(11);
  var_b.field_cc = 9;

  using Serializer_type = Serializer_default<Archive_binary>;

  Serializer_type serializer_a;

  Serializer_type serializer_b;

  serializer_a << var_a;
  ASSERT_EQ(serializer_a.is_good(), true);

  serializer_b << var_b;
  ASSERT_EQ(serializer_b.is_good(), true);

  Format_v1 var_a_rec;
  Format_v1 var_b_rec;

  serializer_a >> var_a_rec;
  ASSERT_EQ(serializer_a.is_good(), true);

  // check that field missing functors were run for missing fields
  ASSERT_EQ(var_a_rec.field_bb.field_1a, 6);
  ASSERT_EQ(var_a_rec.field_bb.field_1b, 6);
  ASSERT_EQ(var_a_rec.field_aa.size(), 1);
  ASSERT_EQ(var_a_rec.field_aa.at(0), 7);
  ASSERT_EQ(var_a_rec.field_cc, 19);

  serializer_b >> var_b_rec;
  ASSERT_EQ(serializer_b.is_good(), true);

  // check that old format fields were read correctly
  ASSERT_EQ(var_b_rec.field_bb.field_1a, 5);
  ASSERT_EQ(var_b_rec.field_bb.field_1b, 5);
  ASSERT_EQ(var_b_rec.field_aa.size(), 2);
  ASSERT_EQ(var_b_rec.field_aa.at(0), 10);
  ASSERT_EQ(var_b_rec.field_aa.at(1), 11);
  ASSERT_EQ(var_b_rec.field_cc, 9);
}

// Serializable structure having a variable-length integer defined
struct Format_vlen_1 : public Serializable<Format_vlen_1> {
  uint64_t field_a = 1;
  decltype(auto) define_fields() {
    return std::make_tuple(define_field(field_a));
  }

  decltype(auto) define_fields() const {
    return std::make_tuple(define_field(field_a));
  }
};

TEST(Serialization, VarlenField) {
  Format_vlen_1 var;

  var.field_a = 65000;

  using Serializer_type = Serializer_default<Archive_binary>;

  Serializer_type serializer;

  serializer << var;
  ASSERT_EQ(serializer.is_good(), true);

  Format_vlen_1 new_var;

  serializer >> new_var;
  ASSERT_EQ(serializer.is_good(), true);

  ASSERT_EQ(serializer.get_archive().get_raw_data().size(),
            serializable_overhead_small + 1 + 3);
  ASSERT_EQ(new_var.field_a, var.field_a);
}

struct Repeated_serializable_internal
    : public Serializable<Repeated_serializable_internal> {
  uint32_t a = 1;
  decltype(auto) define_fields() { return std::make_tuple(define_field(a)); }

  decltype(auto) define_fields() const {
    return std::make_tuple(define_field(a));
  }
};

struct Repeated_serializable : public Serializable<Repeated_serializable> {
  std::map<uint32_t, Repeated_serializable_internal> field;
  decltype(auto) define_fields() {
    return std::make_tuple(define_field(field));
  }

  decltype(auto) define_fields() const {
    return std::make_tuple(define_field(field));
  }
};

// Test verifying binary format implementation, reading and writing
// of a structure having a map field of other serializable objects
//
// R14. Nested messages should be able to be kept in any of containers
//      supported by serialization framework
//
// 1. Write a structure. Use Serializer_default and Archive_binary
//    Provide all fields.
// 2. Decode data
// 3. Check that provided fields are equal to encoded fields.
// 4. Check that calculated size of the encoded data is equal to the result
//    of get_size function and equal to expected size of the message
TEST(Serialization, MapOfSerializables) {
  Repeated_serializable var;

  var.field.insert(std::make_pair(0, Repeated_serializable_internal()));
  var.field.insert(std::make_pair(1, Repeated_serializable_internal()));

  using Serializer_type = Serializer_default<Archive_binary>;

  Serializer_type serializer;

  serializer << var;
  ASSERT_EQ(serializer.is_good(), true);

  // serializable_overhead_small + map_typecode + map_size +
  // (key + serializable_overhead_no_id + type_a + size_a) * 2
  ASSERT_EQ(serializer.get_archive().get_raw_data().size(),
            serializable_overhead_small + 1 + 1 +
                (1 + serializable_overhead_small_no_id + 1 + 1) * 2);

  Repeated_serializable read_var;

  serializer >> read_var;
  ASSERT_EQ(serializer.is_good(), true);
}

// Serializable structure of fields using "unknown field" functionality
struct Unknown_fields_version_1
    : public Serializable<Unknown_fields_version_1> {
  uint32_t a = 1;  // non-ignorable
  uint32_t b = 2;  // ignorable
  uint32_t c = 3;  // non-ignorable

  decltype(auto) define_fields() {
    return std::make_tuple(define_field(a), define_field(b), define_field(c));
  }

  decltype(auto) define_fields() const {
    return std::make_tuple(
        define_field(a, Field_encode_predicate([this]() -> auto {
                       return this->a != 1;
                     }),
                     Unknown_field_policy::error),
        define_field(b, Field_encode_predicate([this]() -> auto {
                       return this->b != 2;
                     })),
        define_field(c, Field_encode_predicate([this]() -> auto {
                       return this->c != 3;
                     }),
                     Unknown_field_policy::error));
  }
};

// Serializable structure of fields using "unknown field" functionality
// Simulates "new" version of Unknown_fields_version_1
struct Unknown_fields_version_2
    : public Serializable<Unknown_fields_version_2> {
  uint32_t a = 4;  // non-ignorable
  uint32_t b = 5;  // ignorable
  uint32_t c = 6;  // non-ignorable
  uint32_t d = 7;  // ignorable
  uint32_t e = 8;  // non-ignorable

  decltype(auto) define_fields() {
    return std::make_tuple(define_field(a), define_field(b), define_field(c),
                           define_field(d), define_field(e));
  }
  decltype(auto) define_fields() const {
    return std::make_tuple(
        define_field(a, Field_encode_predicate([this]() -> auto {
                       return this->a != 4;
                     }),
                     Unknown_field_policy::error),
        define_field(b, Field_encode_predicate([this]() -> auto {
                       return this->b != 5;
                     })),
        define_field(c, Field_encode_predicate([this]() -> auto {
                       return this->c != 6;
                     }),
                     Unknown_field_policy::error),
        define_field(d, Field_encode_predicate([this]() -> auto {
                       return this->d != 7;
                     })),
        define_field(e, Field_encode_predicate([this]() -> auto {
                       return this->e != 8;
                     }),
                     Unknown_field_policy::error));
  }
};

// Test verifying binary format implementation, reading and writing
// of a structure having Unknown_field_policy defined
// (new->old)
//
// R18. Serialization framework shall provide a functionality to generate
//      an error during message decoding in case there are fields in the
//      packet that receiver should know about
//
// Use Serializer_default and Archive_binary
//
// T1.
// 1. Write a structure *new* with fields known to *old* decoder.
// 2. Decode data using the *old* datatype
// 3. Check that data was decoded successfully
// 4. Check that expected field-missing-functors were run
// 5. Check that provided fields have expected values
//
// T2.
// 1. Write a structure *new* with fields known and unknown to *old* decoder.
//    Provided unknown fields are ignorable
// 2. Decode data using the *old* datatype
// 3. Check that data was decoded successfully
// 4. Check that expected field-missing-functors were run
// 5. Check that provided fields have expected values
//
// T3.
// 1. Write a structure *new* with fields known or unknown to *old* decoder.
//    Provided unknown fields are ignorable. Provide known non-ignorable fields
// 2. Decode data using the *old* datatype
// 3. Check that data was decoded successfully
// 4. Check that expected field-missing-functors were run
// 5. Check that provided fields have expected values
//
// T4.
// 1. Write a structure *new* with fields known or unknown to *old* decoder.
//    Provide both unknown
// 2. Decode data using the *old* datatype
// 3. Check that decoding failed with error
// 4. Check that expected field-missing-functors were run
TEST(Serialization, UnknownFields) {
  using Serializer_type = Serializer_default<Archive_binary>;
  // case 1, providing only known fields
  {
    Unknown_fields_version_2 var;
    var.b = 0;  // only b is provided

    Serializer_type serializer;
    serializer << var;
    ASSERT_EQ(serializer.is_good(), true);
    Unknown_fields_version_1 read_var;
    serializer >> read_var;

    ASSERT_EQ(serializer.is_good(), true);
    ASSERT_EQ(read_var.a, 1);
    ASSERT_EQ(read_var.b, var.b);
    ASSERT_EQ(read_var.c, 3);
  }
  // case 2, providing unknown ignorable fields
  {
    Unknown_fields_version_2 var;
    var.d = 0;  // only d is provided

    Serializer_type serializer;

    serializer << var;
    ASSERT_EQ(serializer.is_good(), true);

    Unknown_fields_version_1 read_var;

    serializer >> read_var;
    ASSERT_EQ(serializer.is_good(), true);
    ASSERT_EQ(read_var.a, 1);
    ASSERT_EQ(read_var.b, 2);
    ASSERT_EQ(read_var.c, 3);
  }
  // case 3, providing known, non-ignorable fields
  {
    Unknown_fields_version_2 var;
    var.a = 0;
    var.c = 0;

    Serializer_type serializer;

    serializer << var;
    ASSERT_EQ(serializer.is_good(), true);

    Unknown_fields_version_1 read_var;

    serializer >> read_var;
    ASSERT_EQ(serializer.is_good(), true);
    ASSERT_EQ(read_var.a, var.a);
    ASSERT_EQ(read_var.b, 2);
    ASSERT_EQ(read_var.c, var.c);
  }
  // case 4, providing unknown, non-ignorable fields (error)
  {
    Unknown_fields_version_2 var;
    var.a = 0;
    var.c = 0;
    var.e = 0;

    Serializer_type serializer;

    serializer << var;
    ASSERT_EQ(serializer.is_good(), true);

    Unknown_fields_version_1 read_var;

    serializer >> read_var;
    ASSERT_EQ(serializer.is_good(), false);
    ASSERT_EQ(serializer.get_error().get_type(),
              Serialization_error_type::unknown_field);
  }
}

struct Unknown_fields_version_v1_nested
    : public Serializable<Unknown_fields_version_v1_nested> {
  Unknown_fields_version_1 aa;  // non-ignorable
  Unknown_fields_version_1 bb;  // ignorable
  Unknown_fields_version_1 cc;  // non-ignorable

  decltype(auto) define_fields() {
    return std::make_tuple(define_field(aa), define_field(bb),
                           define_field(cc));
  }

  decltype(auto) define_fields() const {
    return std::make_tuple(
        define_field(aa, Field_encode_predicate([this]() -> auto {
                       return this->aa.a != 1;
                     }),
                     Unknown_field_policy::error),
        define_field(bb, Field_encode_predicate([this]() -> auto {
                       return this->bb.b != 2;
                     })),
        define_field(cc, Field_encode_predicate([this]() -> auto {
                       return this->cc.c != 3;
                     }),
                     Unknown_field_policy::error));
  }
};

struct Unknown_fields_version_v2_nested
    : public Serializable<Unknown_fields_version_v2_nested> {
  Unknown_fields_version_2 aa;  // non-ignorable
  Unknown_fields_version_2 bb;  // ignorable
  Unknown_fields_version_2 cc;  // non-ignorable
  Unknown_fields_version_2 dd;  // ignorable
  Unknown_fields_version_2 ee;  // non-ignorable

  bool aa_provided = false;
  bool bb_provided = false;
  bool cc_provided = false;
  bool dd_provided = false;
  bool ee_provided = false;

  decltype(auto) define_fields() {
    return std::make_tuple(define_field(aa), define_field(bb), define_field(cc),
                           define_field(dd), define_field(ee));
  }

  decltype(auto) define_fields() const {
    return std::make_tuple(
        define_field(aa, Field_encode_predicate([this]() -> auto {
                       return this->aa_provided == true;
                     }),
                     Unknown_field_policy::error),
        define_field(bb, Field_encode_predicate([this]() -> auto {
                       return this->bb_provided == true;
                     })),
        define_field(cc, Field_encode_predicate([this]() -> auto {
                       return this->cc_provided == true;
                     }),
                     Unknown_field_policy::error),
        define_field(dd, Field_encode_predicate([this]() -> auto {
                       return this->dd_provided == true;
                     })),
        define_field(ee, Field_encode_predicate([this]() -> auto {
                       return this->ee_provided == true;
                     }),
                     Unknown_field_policy::error));
  }
};

// Test verifying binary format implementation, reading and writing
// of a structure having Unknown_field_policy defined. This structure
// aggregates a different serializable data structure.
// (new->old)
//
// R18. Serialization framework shall provide a functionality to generate
//      an error during message decoding in case there are fields in the
//      packet that receiver should know about
//
// Use Serializer_default and Archive_binary
//
// T1.
// 1. Write a structure *new* with fields known to *old* decoder.
// 2. Decode data using the *old* datatype
// 3. Check that data was decoded successfully
// 4. Check that expected field-missing-functors were run
// 5. Check that provided fields have expected values
//
// T2.
// 1. Write a structure *new* with fields known and unknown to *old* decoder.
//    Provided unknown fields are ignorable
// 2. Decode data using the *old* datatype
// 3. Check that data was decoded successfully
// 4. Check that expected field-missing-functors were run
// 5. Check that provided fields have expected values
//
// T3.
// 1. Write a structure *new* with fields known or unknown to *old* decoder.
//    Provided unknown fields are ignorable. Provide known non-ignorable fields
// 2. Decode data using the *old* datatype
// 3. Check that data was decoded successfully
// 4. Check that expected field-missing-functors were run
// 5. Check that provided fields have expected values
//
// T4. (nested message error)
// 1. Write a structure *new* with fields known or unknown to *old* decoder.
//    Provide non-ignorable fields on nested-structure level
// 2. Decode data using the *old* datatype
// 3. Check that decoding failed with error
// 4. Check that expected field-missing-functors were run
//
// T5. (outer message error)
// 1. Write a structure *new* with fields known or unknown to *old* decoder.
//    Provide non-ignorable fields on top level
// 2. Decode data using the *old* datatype
// 3. Check that decoding failed with error
// 4. Check that expected field-missing-functors were run
TEST(Serialization, UnknownFieldsError) {
  using Serializer_type = Serializer_default<Archive_binary>;
  // case 1, providing only known fields
  {
    Unknown_fields_version_v2_nested var;
    // only bb, b is provided
    var.bb_provided = true;
    var.bb.b = 0;

    Serializer_type serializer;
    serializer << var;
    ASSERT_EQ(serializer.is_good(), true);
    Unknown_fields_version_v1_nested read_var;
    serializer >> read_var;

    ASSERT_EQ(serializer.is_good(), true);
    ASSERT_EQ(read_var.aa.a, 1);
    ASSERT_EQ(read_var.aa.b, 2);
    ASSERT_EQ(read_var.aa.c, 3);
    ASSERT_EQ(read_var.bb.a, 1);
    ASSERT_EQ(read_var.bb.b, var.bb.b);
    ASSERT_EQ(read_var.bb.c, 3);
    ASSERT_EQ(read_var.cc.a, 1);
    ASSERT_EQ(read_var.cc.b, 2);
    ASSERT_EQ(read_var.cc.c, 3);
  }
  // case 2, providing unknown ignorable fields
  {
    Unknown_fields_version_v2_nested var;
    // only dd is provided
    var.dd_provided = true;
    // inside, a,b,c,d,e are provided
    // although some are non-ignorable, dd is unknown is ignorable, so
    // decoding should pass with no error
    var.dd.a = 10;
    var.dd.b = 11;
    var.dd.d = 12;
    var.dd.d = 13;
    var.dd.e = 14;

    Serializer_type serializer;

    serializer << var;
    ASSERT_EQ(serializer.is_good(), true);

    Unknown_fields_version_v1_nested read_var;

    serializer >> read_var;
    ASSERT_EQ(serializer.is_good(), true);
    // all values are default
    ASSERT_EQ(read_var.aa.a, 1);
    ASSERT_EQ(read_var.aa.b, 2);
    ASSERT_EQ(read_var.aa.c, 3);
    ASSERT_EQ(read_var.bb.a, 1);
    ASSERT_EQ(read_var.bb.b, 2);
    ASSERT_EQ(read_var.bb.c, 3);
    ASSERT_EQ(read_var.cc.a, 1);
    ASSERT_EQ(read_var.cc.b, 2);
    ASSERT_EQ(read_var.cc.c, 3);
  }
  // case 3, providing known, non-ignorable fields and unknown ignorable
  {
    Unknown_fields_version_v2_nested var;
    // providing known fields of bb (known)
    var.bb_provided = true;
    var.bb.a = 10;
    var.bb.b = 11;
    var.bb.c = 12;
    var.bb.d = 13;

    Serializer_type serializer;

    serializer << var;
    ASSERT_EQ(serializer.is_good(), true);

    Unknown_fields_version_v1_nested read_var;

    serializer >> read_var;
    ASSERT_EQ(serializer.is_good(), true);
    ASSERT_EQ(read_var.aa.a, 1);
    ASSERT_EQ(read_var.aa.b, 2);
    ASSERT_EQ(read_var.aa.c, 3);
    ASSERT_EQ(read_var.bb.a, var.bb.a);
    ASSERT_EQ(read_var.bb.b, var.bb.b);
    ASSERT_EQ(read_var.bb.c, var.bb.c);
    ASSERT_EQ(read_var.cc.a, 1);
    ASSERT_EQ(read_var.cc.b, 2);
    ASSERT_EQ(read_var.cc.c, 3);
  }
  // case 4, providing unknown, non-ignorable fields (error)
  {
    Unknown_fields_version_v2_nested var;
    // providing known fields of bb (known) and e - unknown, non-ignorable
    var.bb_provided = true;
    var.bb.a = 10;
    var.bb.b = 11;
    var.bb.c = 12;
    var.bb.d = 13;
    var.bb.e = 14;
    Serializer_type serializer;

    serializer << var;
    ASSERT_EQ(serializer.is_good(), true);

    Unknown_fields_version_v1_nested read_var;

    serializer >> read_var;
    ASSERT_EQ(serializer.is_good(), false);
    ASSERT_EQ(serializer.get_error().get_type(),
              Serialization_error_type::unknown_field);
  }
  // case 5, providing unknown, non-ignorable fields on top level
  {
    Unknown_fields_version_v2_nested var;
    // providing known fields of bb (known) and ee - unknown, non-ignorable
    var.bb_provided = true;
    var.bb.a = 10;
    var.bb.b = 11;
    var.bb.c = 12;
    var.ee_provided = true;
    var.ee.a = 10;
    Serializer_type serializer;

    serializer << var;
    ASSERT_EQ(serializer.is_good(), true);

    Unknown_fields_version_v1_nested read_var;

    serializer >> read_var;
    ASSERT_EQ(serializer.is_good(), false);
    ASSERT_EQ(serializer.get_error().get_type(),
              Serialization_error_type::unknown_field);
  }
}

// Test verifying calculation of maximum encoded message size for basic types
//
// R19. Serialization framework shall provide functionality to calculate
//      maximum size of encoded message
//
// Check that calculated maximum size for supported basic types is as expected.
// Test:
// - bounded strings
// - fixed/variable-length size integers
// - floating-point numbers
TEST(Serialization, MessageMaxSize) {
  // Below statement cause compilation to fail, as expected - this
  // function is disabled for unlimited strings.
  // This line is commented out.
  // Archive_binary_field_max_size_calculator<std::string, 0>::get_max_size();

  ASSERT_EQ((Archive_binary_field_max_size_calculator<std::string,
                                                      2>::get_max_size()),
            2 + 9);
  ASSERT_EQ((Archive_binary_field_max_size_calculator<std::string,
                                                      99>::get_max_size()),
            99 + 9);

  ASSERT_EQ(
      (Archive_binary_field_max_size_calculator<uint32_t, 3>::get_max_size()),
      3);
  ASSERT_EQ(
      (Archive_binary_field_max_size_calculator<uint32_t, 0>::get_max_size()),
      5);
  ASSERT_EQ(
      (Archive_binary_field_max_size_calculator<uint64_t, 0>::get_max_size()),
      9);
  ASSERT_EQ(
      (Archive_binary_field_max_size_calculator<uint64_t, 3>::get_max_size()),
      3);

  ASSERT_EQ(
      (Archive_binary_field_max_size_calculator<double, 0>::get_max_size()),
      sizeof(double));
  ASSERT_EQ(
      (Archive_binary_field_max_size_calculator<double, 8>::get_max_size()),
      sizeof(double));
  ASSERT_EQ(
      (Archive_binary_field_max_size_calculator<float, 0>::get_max_size()),
      sizeof(float));
  ASSERT_EQ(
      (Archive_binary_field_max_size_calculator<float, 4>::get_max_size()),
      sizeof(float));
}

namespace max_size {

struct S_a : public Serializable<S_a> {
  uint64_t a = 5;
  uint64_t b = 4;
  uint32_t c = 6;
  uint32_t d = 7;
  decltype(auto) define_fields() const {
    return std::make_tuple(define_field(a), define_field(b), define_field(c),
                           define_field(d));
  }
};

struct S_b : public Serializable<S_b> {
  uint64_t a = 5;
  std::string b;
  decltype(auto) define_fields() const {
    return std::make_tuple(define_field(a), define_field(b));
  }
};

struct S_c : public Serializable<S_c> {
  uint64_t a = 5;
  std::string b;
  decltype(auto) define_fields() const {
    return std::make_tuple(define_field(a), define_field_with_size<4>(b));
  }
};

struct S_d : public Serializable<S_d> {
  uint64_t a = 5;
  std::vector<uint32_t> b;
  decltype(auto) define_fields() const {
    return std::make_tuple(define_field(a), define_field_with_size<4>(b));
  }
};

struct S_e : public Serializable<S_e> {
  S_c a;
  decltype(auto) define_fields() const {
    return std::make_tuple(define_compound_field(a));
  }
};

struct S_f : public Serializable<S_f> {
  std::string a;
  decltype(auto) define_fields() const {
    return std::make_tuple(define_field(a));
  }
};

}  // namespace max_size

// Test verifying calculation of maximum encoded message size for serializable
// types
//
// R19. Serialization framework shall provide functionality to calculate
//      maximum size of encoded message
//
// Check that calculated maximum size for various types of messages is as
// expected
TEST(Serialization, MessageMaxSizeAllTypes) {
  using Serializer_type = Serializer_default<Archive_binary>;

  ASSERT_EQ(Serializer_type::get_max_size<max_size::S_a>(),
            serializable_overhead_max + id_max_size + 9 + id_max_size + 9 +
                id_max_size + 5 + id_max_size + 5);

  // Below line does not compile, as expected. Line is commented out
  // Serializer_type::get_max_size<max_size::S_b>();

  // Below line should compile, since string in c has limited size
  ASSERT_EQ(Serializer_type::get_max_size<max_size::S_c>(),
            serializable_overhead_max + id_max_size + 9 + id_max_size + 4);

  // Below line does not compile, as expected (vector). Line is commented out
  // Serializer_type::get_max_size<max_size::S_d>();

  // max size of array and c array
  std::size_t expected_array_size =
      serializable_overhead_max + id_max_size + format_char_array_field_size;
  ASSERT_EQ(Serializer_type::get_max_size<Format_char_array>(),
            expected_array_size);
  ASSERT_EQ(Serializer_type::get_max_size<Format_char_carray>(),
            expected_array_size);

  // Below line should compile, since string in c in S_c has limited size
  ASSERT_EQ(Serializer_type::get_max_size<max_size::S_e>(),
            serializable_overhead_max + serializable_overhead_max +
                id_max_size + 9 + id_max_size + 4);

  // Below line does not compile, as expected (unbounded string).
  // Line is commented out
  // Serializer_type::get_max_size<max_size::S_f>();
}

// Serializable structure with fixed-size integers
struct Unsigned_integers_fixed_size
    : public Serializable<Unsigned_integers_fixed_size> {
  uint8_t a = 0;
  uint16_t b = 0;
  uint32_t c = 0;
  uint32_t d = 0;
  uint64_t e = 0;
  uint64_t f = 0;
  uint64_t g = 0;
  uint64_t h = 0;
  decltype(auto) define_fields() const {
    return std::make_tuple(
        define_field_with_size<1>(a), define_field_with_size<2>(b),
        define_field_with_size<3>(c), define_field_with_size<4>(d),
        define_field_with_size<5>(e), define_field_with_size<6>(f),
        define_field_with_size<7>(g), define_field_with_size<8>(h));
  }
  decltype(auto) define_fields() {
    return std::make_tuple(
        define_field_with_size<1>(a), define_field_with_size<2>(b),
        define_field_with_size<3>(c), define_field_with_size<4>(d),
        define_field_with_size<5>(e), define_field_with_size<6>(f),
        define_field_with_size<7>(g), define_field_with_size<8>(h));
  }
};

// Test verifying calculation of maximum encoded message size for serializable
// types with fixed-length integers
//
// R19. Serialization framework shall provide functionality to calculate
//      maximum size of encoded message
//
// Check that calculated maximum size for various types of messages is as
// expected
TEST(Serialization, FixedIntegers) {
  using Serializer_type = Serializer_default<Archive_binary>;

  Serializer_type serializer;
  Unsigned_integers_fixed_size var_written, var_read;
  var_written.a = 1;
  var_written.b = 2;
  var_written.c = 3;
  var_written.d = 4;
  var_written.e = 5;
  var_written.f = 6;
  var_written.g = 7;
  var_written.h = 8;

  serializer << var_written;
  serializer >> var_read;

  ASSERT_EQ(serializer.is_good(), true);
  ASSERT_EQ(serializer.get_archive().get_raw_data().size(),
            Serializer_default<Archive_binary>::get_size(var_written));

  ASSERT_EQ(serializable_overhead_small + 1 + 1 + 1 + 2 + 1 + 3 + 1 + 4 + 1 +
                5 + 1 + 6 + 1 + 7 + 1 + 8,
            Serializer_default<Archive_binary>::get_size(var_written));

  ASSERT_EQ(Serializer_type::get_max_size<Unsigned_integers_fixed_size>(),
            serializable_overhead_max + id_max_size + 1 + id_max_size + 2 +
                id_max_size + 3 + id_max_size + 4 + id_max_size + 5 +
                id_max_size + 6 + id_max_size + 7 + id_max_size + 8);

  ASSERT_EQ(var_written.a, var_read.a);
  ASSERT_EQ(var_written.b, var_read.b);
  ASSERT_EQ(var_written.c, var_read.c);
  ASSERT_EQ(var_written.d, var_read.d);
  ASSERT_EQ(var_written.e, var_read.e);
  ASSERT_EQ(var_written.f, var_read.f);
  ASSERT_EQ(var_written.g, var_read.g);
  ASSERT_EQ(var_written.h, var_read.h);
}

// A serializable structure with unsigned, variable-length integers
struct Unsigned_integers_variable_length_size
    : public Serializable<Unsigned_integers_variable_length_size> {
  uint8_t a = 0;
  uint16_t b = 0;
  uint32_t c = 0;
  uint64_t d = 0;
  decltype(auto) define_fields() const {
    return std::make_tuple(define_field(a), define_field(b), define_field(c),
                           define_field(d));
  }
  decltype(auto) define_fields() {
    return std::make_tuple(define_field(a), define_field(b), define_field(c),
                           define_field(d));
  }
};

// Test verifying calculation of maximum encoded message size for serializable
// types with variable-length integers
//
// R19. Serialization framework shall provide functionality to calculate
//      maximum size of encoded message
// R20. Serialization framework shall provide functionality for optimized
//      encoding of integer types
//
// Check that calculated maximum size for various types of messages is as
// expected
TEST(Serialization, UnsignedVarlenIntegers) {
  using Serializer_type = Serializer_default<Archive_binary>;

  Serializer_type serializer;
  Unsigned_integers_variable_length_size var_written, var_read;
  var_written.a = 1;
  var_written.b = 5;
  var_written.c = 65536;
  var_written.d = 17179869184;

  serializer << var_written;
  serializer >> var_read;

  ASSERT_EQ(serializer.is_good(), true);
  ASSERT_EQ(serializer.get_archive().get_raw_data().size(),
            Serializer_default<Archive_binary>::get_size(var_written));

  ASSERT_EQ(serializable_overhead_small + 1 + 1 + 1 + 1 + 1 + 3 + 1 + 5,
            Serializer_default<Archive_binary>::get_size(var_written));

  ASSERT_EQ(
      Serializer_type::get_max_size<Unsigned_integers_variable_length_size>(),
      serializable_overhead_max + id_max_size + 2 + id_max_size + 3 +
          id_max_size + 5 + id_max_size + 9);

  ASSERT_EQ(var_written.a, var_read.a);
  ASSERT_EQ(var_written.b, var_read.b);
  ASSERT_EQ(var_written.c, var_read.c);
  ASSERT_EQ(var_written.d, var_read.d);
}

// A serializable structure with signed, variable-length integers
struct Signed_integers_variable_length_size
    : public Serializable<Signed_integers_variable_length_size> {
  int8_t a = 0;
  int16_t b = 0;
  int32_t c = 0;
  int64_t d = 0;
  decltype(auto) define_fields() const {
    return std::make_tuple(define_field(a), define_field(b), define_field(c),
                           define_field(d));
  }
  decltype(auto) define_fields() {
    return std::make_tuple(define_field(a), define_field(b), define_field(c),
                           define_field(d));
  }
};

// Test verifying calculation of maximum encoded message size for serializable
// types with signed, variable-length integers
//
// R19. Serialization framework shall provide functionality to calculate
//      maximum size of encoded message
// R20. Serialization framework shall provide functionality for optimized
//      encoding of integer types
//
// Check that calculated maximum size for various types of messages is as
// expected
TEST(Serialization, SignedVarlenIntegers) {
  using Serializer_type = Serializer_default<Archive_binary>;

  Serializer_type serializer;
  Signed_integers_variable_length_size var_written, var_read;
  var_written.a = -1;
  var_written.b = -5;
  var_written.c = -65536;
  var_written.d = -17179869184;

  serializer << var_written;
  serializer >> var_read;

  ASSERT_EQ(serializer.is_good(), true);
  ASSERT_EQ(serializer.get_archive().get_raw_data().size(),
            Serializer_default<Archive_binary>::get_size(var_written));

  ASSERT_EQ(serializable_overhead_small + 1 + 1 + 1 + 1 + 1 + 3 + 1 + 5,
            Serializer_default<Archive_binary>::get_size(var_written));

  ASSERT_EQ(
      Serializer_type::get_max_size<Unsigned_integers_variable_length_size>(),
      serializable_overhead_max + id_max_size + 2 + id_max_size + 3 +
          id_max_size + 5 + id_max_size + 9);

  ASSERT_EQ(var_written.a, var_read.a);
  ASSERT_EQ(var_written.b, var_read.b);
  ASSERT_EQ(var_written.c, var_read.c);
  ASSERT_EQ(var_written.d, var_read.d);
}

// A serializable structure with set field
struct Struct_with_set : public Serializable<Struct_with_set> {
  std::set<uint32_t> field_a;
  decltype(auto) define_fields() const {
    return std::make_tuple(define_field(field_a));
  }
  decltype(auto) define_fields() {
    return std::make_tuple(define_field(field_a));
  }
};

// Test verifying binary format implementation, reading and writing
// of a structure having a set field of simple types defined
//
// R13. When using binary format, serialization framework shall correctly
//      decode and encode supported containers
//
// 1. Write a structure. Use Serializer_default and Archive_binary
//    Provide all fields.
// 2. Decode data
// 3. Check that provided fields are equal to encoded fields.
// 4. Check that calculated size of the encoded data is equal to the result
//    of get_size function.
TEST(Serialization, SetField) {
  using Serializer_type = Serializer_default<Archive_binary>;

  Serializer_type serializer;
  Struct_with_set var_written, var_read;
  size_t num_entries = 5;
  for (size_t eid = 0; eid < num_entries; ++eid) {
    var_written.field_a.insert(eid);
  }

  serializer << var_written;
  serializer >> var_read;

  ASSERT_EQ(serializer.is_good(), true);
  ASSERT_EQ(serializer.get_archive().get_raw_data().size(),
            Serializer_default<Archive_binary>::get_size(var_written));

  ASSERT_EQ(var_written.field_a.size(), var_read.field_a.size());
  auto read_it = var_read.field_a.begin();
  for (const auto &written_entry : var_written.field_a) {
    ASSERT_EQ(*read_it, written_entry);
    ++read_it;
  }
}

std::size_t read_serializable_size(unsigned const char *stream) {
  uint64_t serializable_size = 0;
  Primitive_type_codec<uint64_t>::read_bytes<0>(stream + 1, 9,
                                                serializable_size);
  return serializable_size;
}

void write_serializable_size(unsigned char *stream, std::size_t size) {
  Primitive_type_codec<uint64_t>::write_bytes<0>(stream + 1, size);
}

// A random structure to test corrupted messages, here, we use
// fixed length integers (for simplicity of calculations)
struct CorruptedMessageStruct : public Serializable<CorruptedMessageStruct> {
  uint8_t a = 0;
  uint16_t b = 0;
  uint32_t c = 0;
  uint64_t d = 0;
  decltype(auto) define_fields() const {
    return std::make_tuple(
        define_field_with_size<1>(a), define_field_with_size<2>(b),
        define_field_with_size<3>(c), define_field_with_size<7>(d));
  }
  decltype(auto) define_fields() {
    return std::make_tuple(
        define_field_with_size<1>(a), define_field_with_size<2>(b),
        define_field_with_size<3>(c), define_field_with_size<7>(d));
  }
};

// Test verifying binary format implementation - reading of corrupted
// messages.
//
// R21. Serialization framework shall return an error in case
//      decoder receives a corrupted message
//
// T1.
// 1. Write a structure. Use Serializer_default and Archive_binary
//    Provide all fields.
// 2. Decode data, using incorrect number of bytes
// 3. Check that decoder failed with expected error
//
// T2.
// 1. Data reused from T1
// 2. Alter the saved message size
// 3. Decode data, using incorrect number of bytes
// 4. Check that decoder failed with expected error
//
// T3.
// 1. Data reused from T1
// 2. Alter the saved message size (curring field id)
// 3. Decode data, using incorrect number of bytes (curring field id)
// 4. Check that decoder failed with expected error
//
// T4.
// 1. Perform T1 using Archive_binary
TEST(Serialization, CorruptedMessage) {
  // read_archive_binary
  {
    using Rw_serializer = Serializer_default<Archive_binary>;

    Rw_serializer rw_serializer;
    CorruptedMessageStruct var_written, var_read;

    rw_serializer << var_written;

    auto ar_size = rw_serializer.get_archive().get_raw_data().size();
    auto ar = rw_serializer.get_archive().get_raw_data();

    ASSERT_EQ(rw_serializer.is_good(), true);

    using Reader = Serializer_default<Read_archive_binary>;
    Reader reader;

    reader.get_archive().set_stream(ar.data(), ar_size - 5);  // cut data

    reader >> var_read;

    ASSERT_EQ(reader.is_good(), false);

    ASSERT_EQ(reader.get_error().get_type(),
              Serialization_error_type::archive_read_error);

    Reader reader2;

    // decrease size written into the packet
    write_serializable_size(ar.data(), read_serializable_size(ar.data()) - 7);
    reader2.get_archive().set_stream(ar.data(), ar_size - 7);  // cut data

    reader2 >> var_read;

    ASSERT_EQ(reader2.is_good(), false);

    ASSERT_EQ(reader2.get_error().get_type(),
              Serialization_error_type::archive_read_error);

    Reader reader3;

    // cut id from data
    write_serializable_size(ar.data(), read_serializable_size(ar.data()) - 1);
    reader3.get_archive().set_stream(ar.data(), ar_size - 8);

    reader3 >> var_read;

    ASSERT_EQ(reader3.is_good(), true);
  }

  // archive_binary
  {
    using Rw_serializer = Serializer_default<Archive_binary>;

    Rw_serializer rw_serializer;
    Unsigned_integers_fixed_size var_written, var_read;

    rw_serializer << var_written;

    auto initial_size = rw_serializer.get_archive().get_raw_data().size();
    auto &ar = rw_serializer.get_archive().get_raw_data();

    ASSERT_EQ(rw_serializer.is_good(), true);

    ar.resize(initial_size - 5);

    rw_serializer >> var_read;

    ASSERT_EQ(rw_serializer.is_good(), false);

    ASSERT_EQ(rw_serializer.get_error().get_type(),
              Serialization_error_type::archive_read_error);
  }
}

// A structure to test corrupted, nested messages
struct CorruptedNestedMessageStruct
    : public Serializable<CorruptedNestedMessageStruct> {
  CorruptedMessageStruct a;
  decltype(auto) define_fields() const {
    return std::make_tuple(define_compound_field(a));
  }
  decltype(auto) define_fields() {
    return std::make_tuple(define_compound_field(a));
  }
};

// Test verifying binary format implementation - reading of corrupted
// nested messages
//
// R21. Serialization framework shall return an error in case
//      decoder receives a corrupted message
//
// T1.
// 1. Write a structure. Use Serializer_default and Archive_binary
//    Provide all fields.
// 2. Corrupt id of the serializable
// 3. Check that decoder failed with expected error
TEST(Serialization, CorruptedNestedMessage) {
  using Rw_serializer = Serializer_default<Archive_binary>;

  Rw_serializer rw_serializer;
  CorruptedNestedMessageStruct var_written, var_read;

  rw_serializer << var_written;

  auto &ar = rw_serializer.get_archive().get_raw_data();
  ASSERT_EQ(rw_serializer.is_good(), true);

  // corrupt id
  ar[3] = 2;

  rw_serializer >> var_read;

  ASSERT_EQ(rw_serializer.is_good(), false);

  ASSERT_EQ(rw_serializer.get_error().get_type(),
            Serialization_error_type::field_id_mismatch);
}

struct Last_unknown_field_id_v1
    : public Serializable<Last_unknown_field_id_v1> {
  uint32_t a;

  auto define_fields() const {
    return std::make_tuple(define_field(a, Unknown_field_policy::error));
  }

  auto define_fields() {
    return std::make_tuple(define_field(a, Unknown_field_policy::error));
  }
};

struct Last_unknown_field_id_nested_v2
    : public Serializable<Last_unknown_field_id_nested_v2> {
  uint32_t n;

  auto define_fields() const {
    return std::make_tuple(define_field(n, Unknown_field_policy::error));
  }

  auto define_fields() {
    return std::make_tuple(define_field(n, Unknown_field_policy::error));
  }
};

struct Last_unknown_field_id_v2
    : public Serializable<Last_unknown_field_id_v2> {
  uint32_t a;
  Last_unknown_field_id_nested_v2 b;

  auto define_fields() const {
    return std::make_tuple(
        define_field(a, Unknown_field_policy::error),
        define_compound_field(b, Unknown_field_policy::error));
  }

  auto define_fields() {
    return std::make_tuple(
        define_field(a, Unknown_field_policy::error),
        define_compound_field(b, Unknown_field_policy::error));
  }
};

// Test verifying binary format implementation, reading and writing
// of a structure having Unknown_field_policy defined. Here, tested is
// calculation of the last_non_ignorable_field_id in case new non-ignorable
// nested type is introduced in later version of the software. Testing new->old
//
// R18. Serialization framework shall provide a functionality to generate
//      an error during message decoding in case there are fields in the
//      packet that receiver should know about
//
// T1.
// 1. Write a structure *new* with fields known and unknown to *old* decoder,
//    including new, non-ignorable nested type
// 2. Decode data using the *old* datatype
// 3. Check that decoding failed with expected error
TEST(Serialization, LastUnknownFieldIdNewNestedType) {
  using Rw_serializer = Serializer_default<Archive_binary>;
  Rw_serializer rw_serializer;

  Last_unknown_field_id_v1 var_read;
  Last_unknown_field_id_v2 var_written;

  rw_serializer << var_written;

  ASSERT_EQ(rw_serializer.is_good(), true);

  rw_serializer >> var_read;

  ASSERT_EQ(rw_serializer.is_good(), false);

  ASSERT_EQ(rw_serializer.get_error().get_type(),
            Serialization_error_type::unknown_field);
}

// Test verifying calculation of maximum size of encoded vlen types
//
// R19. Serialization framework shall provide functionality to calculate
//      maximum size of encoded message
//
// Check that calculated maximum size for supported vlen types is as expected.
TEST(Serialization, SizeVlenBasic) {
  auto max_vlen_size_uint64_t =
      Archive_binary::template get_max_size<uint64_t, 0>();
  ASSERT_EQ(max_vlen_size_uint64_t, 9);
}

template <size_t bound>
struct Bounded_length_string
    : public Serializable<Bounded_length_string<bound>> {
  std::string s;
  decltype(auto) define_fields() {
    return std::make_tuple(define_field_with_size<bound>(s));
  }

  decltype(auto) define_fields() const {
    return std::make_tuple(define_field_with_size<bound>(s));
  }
};

// Test verifying binary format implementation, reading and writing
// of bounded string fields
//
// R22. When using binary format, serialization framework shall correctly
//     decode and encode bounded string fields
//
// T1.
// 1. Define a bounded string field
// 2. Assign to the string field more data than allowed in field definition
// 3. Check that encoding failed with expected error
TEST(Serialization, BoundedLengthStringWriteError) {
  Bounded_length_string<10> original_short;
  Serializer_default<Write_archive_binary> encoder;
  Serializer_default<Archive_binary> encoder_2;

  constexpr auto max_size = 1000;
  std::vector<unsigned char> data(max_size);
  encoder.get_archive().set_stream(data.data(), max_size);

  // Make the actual length bigger than the length declared in
  // define_field_with_size.
  original_short.s = std::string(100, 'a');

  encoder << original_short;
  encoder_2 << original_short;
  ASSERT_EQ(encoder.is_good(), false);
  ASSERT_EQ(encoder.get_error().get_type(),
            Serialization_error_type::archive_write_error);
  ASSERT_EQ(encoder_2.is_good(), false);
  ASSERT_EQ(encoder_2.get_error().get_type(),
            Serialization_error_type::archive_write_error);
}

// Test verifying binary format implementation, reading and writing
// of bounded string fields
//
// R22. When using binary format, serialization framework shall correctly
//     decode and encode bounded string fields
//
// T1.
// 1. Define a bounded string field to write from
// 2. Define a bounded string field to read into
// 2. Fill string field with values with more data than allowed in decoder field
//    definition
// 3. Encode a message, check that encoding passed with no error
// 4. Decode a message, check that decoding failed with expected error
TEST(Serialization, BoundedLengthStringReadError) {
  Bounded_length_string<100> original_short;
  Bounded_length_string<10> restored;
  Serializer_default<Write_archive_binary> encoder;
  Serializer_default<Read_archive_binary> decoder;
  Serializer_default<Archive_binary> serializer;

  constexpr auto max_size = 1000;
  std::vector<unsigned char> data(max_size);
  encoder.get_archive().set_stream(data.data(), max_size);
  decoder.get_archive().set_stream(data.data(), max_size);

  // Make the actual length bigger than the length declared in
  // define_field_with_size.
  original_short.s = std::string(100, 'a');

  encoder << original_short;
  ASSERT_EQ(encoder.is_good(), true);
  decoder >> restored;
  ASSERT_EQ(decoder.is_good(), false);
  ASSERT_EQ(decoder.get_error().get_type(),
            Serialization_error_type::archive_read_error);
  serializer << original_short;
  ASSERT_EQ(serializer.is_good(), true);
  serializer >> restored;
  ASSERT_EQ(serializer.is_good(), false);
  ASSERT_EQ(serializer.get_error().get_type(),
            Serialization_error_type::archive_read_error);
  auto original_string_length = original_short.s.length();
  auto predicted_max_packet_size =
      decltype(encoder)::get_max_size<decltype(original_short)>();
  auto predicted_packet_size = decltype(encoder)::get_size(original_short);
  auto written_packet_size = encoder.get_archive().get_size_written();
  auto restored_string_length = restored.s.size();
  // The encoder (when given an overlong string) writes more than max
  // packet size, and the decoder (when given an overlong field in the
  // packet) reads more than the max string length.
  if constexpr (debug_print) {
    std::cout << "original_string_length=" << original_string_length
              << " predicted_max_packet_size=" << predicted_max_packet_size
              << " predicted_packet_size=" << predicted_packet_size
              << " written_packet_size=" << written_packet_size
              << " restored_string_length=" << restored_string_length
              << std::endl;
  }
}

// Simple message format with an unbounded string
struct Unbounded_string_message
    : public Serializable<Unbounded_string_message> {
  uint32_t field_a = 10;
  uint32_t field_b = 11;
  float field_c = 0.0;
  std::string field_d = "hello";

  decltype(auto) define_fields() {
    return std::make_tuple(define_field(field_a), define_field(field_b),
                           define_field(field_c), define_field(field_d));
  }

  decltype(auto) define_fields() const {
    return std::make_tuple(define_field(field_a), define_field(field_b),
                           define_field(field_c), define_field(field_d));
  }
};

// Test verifying binary format implementation, reading and writing
// of unbounded string fields
//
// R23. When using binary format, serialization framework shall correctly
//     decode and encode unbounded string fields
//
// T1.
// 1. Define an unbounded string field to write from
// 2. Define an unbounded string field to read into
// 2. Fill string field with values with more data than allowed in decoder field
//    definition
// 3. Encode a message, check that encoding passed with no error
// 4. Decode a message, check that decoding failed with no error
// 5. Check that decoded message fields match encoded data fields
TEST(Serialization, UnboundedLengthString) {
  Serializer_default<Archive_binary> serializer;

  Unbounded_string_message data_sent, data_received;
  data_sent.field_a = 1;
  data_sent.field_b = 1;
  data_sent.field_c = 0.5;
  data_sent.field_d = "bye";

  serializer << data_sent;
  ASSERT_TRUE(serializer.is_good());
  serializer >> data_received;
  ASSERT_TRUE(serializer.is_good());

  ASSERT_EQ(data_sent.field_a, data_received.field_a);
  ASSERT_EQ(data_sent.field_b, data_received.field_b);
  ASSERT_EQ(data_sent.field_c, data_received.field_c);
  ASSERT_EQ(data_sent.field_d, data_received.field_d);
}

}  // namespace mysql::serialization
