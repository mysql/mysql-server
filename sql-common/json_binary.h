#ifndef JSON_BINARY_INCLUDED
#define JSON_BINARY_INCLUDED

/* Copyright (c) 2015, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

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

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
  @file

  This file specifies the interface for serializing JSON values into
  binary representation, and for reading values back from the binary
  representation.

  The binary format is as follows:

  Each JSON value (scalar, object or array) has a one byte type
  identifier followed by the actual value.

  If the value is a JSON object, its binary representation will have a
  header that contains:

  - the member count
  - the size of the binary value in bytes
  - a list of pointers to each key
  - a list of pointers to each value

  The actual keys and values will come after the header, in the same
  order as in the header.

  Similarly, if the value is a JSON array, the binary representation
  will have a header with

  - the element count
  - the size of the binary value in bytes
  - a list of pointers to each value

  followed by the actual values, in the same order as in the header.

  @verbatim
  doc ::= type value

  type ::=
      0x00 |       // small JSON object
      0x01 |       // large JSON object
      0x02 |       // small JSON array
      0x03 |       // large JSON array
      0x04 |       // literal (true/false/null)
      0x05 |       // int16
      0x06 |       // uint16
      0x07 |       // int32
      0x08 |       // uint32
      0x09 |       // int64
      0x0a |       // uint64
      0x0b |       // double
      0x0c |       // utf8mb4 string
      0x0f         // custom data (any MySQL data type)

  value ::=
      object  |
      array   |
      literal |
      number  |
      string  |
      custom-data

  object ::= element-count size key-entry* value-entry* key* value*

  array ::= element-count size value-entry* value*

  // number of members in object or number of elements in array
  element-count ::=
      uint16 |  // if used in small JSON object/array
      uint32    // if used in large JSON object/array

  // number of bytes in the binary representation of the object or array
  size ::=
      uint16 |  // if used in small JSON object/array
      uint32    // if used in large JSON object/array

  key-entry ::= key-offset key-length

  key-offset ::=
      uint16 |  // if used in small JSON object
      uint32    // if used in large JSON object

  key-length ::= uint16    // key length must be less than 64KB

  value-entry ::= type offset-or-inlined-value

  // This field holds either the offset to where the value is stored,
  // or the value itself if it is small enough to be inlined (that is,
  // if it is a JSON literal or a small enough [u]int).
  offset-or-inlined-value ::=
      uint16 |   // if used in small JSON object/array
      uint32     // if used in large JSON object/array

  key ::= utf8mb4-data

  literal ::=
      0x00 |   // JSON null literal
      0x01 |   // JSON true literal
      0x02 |   // JSON false literal

  number ::=  ....  // little-endian format for [u]int(16|32|64), whereas
                    // double is stored in a platform-independent, eight-byte
                    // format using float8store()

  string ::= data-length utf8mb4-data

  custom-data ::= custom-type data-length binary-data

  custom-type ::= uint8   // type identifier that matches the
                          // internal enum_field_types enum

  data-length ::= uint8*  // If the high bit of a byte is 1, the length
                          // field is continued in the next byte,
                          // otherwise it is the last byte of the length
                          // field. So we need 1 byte to represent
                          // lengths up to 127, 2 bytes to represent
                          // lengths up to 16383, and so on...
  @endverbatim
*/

#include <stddef.h>
#include <cassert>
#include <cstdint>
#include <string>

/*
  This file is part of our public interface for the 'json_binary' library.
  This means that #includes of MySQL headers here should be limited to
  files that are INSTALLed.
*/
#include "field_types.h"  // enum_field_types
#include "sql-common/json_error_handler.h"

#ifdef MYSQL_SERVER
class Field_json;
class Json_wrapper;
class THD;
#endif

class Json_dom;
class String;

#if defined(WIN32) && defined(EXPORT_JSON_FUNCTIONS)
#define EXPORT_JSON_FUNCTION __declspec(dllexport)
#else
#define EXPORT_JSON_FUNCTION
#endif

namespace json_binary {

void write_offset_or_size(char *dest, size_t offset_or_size, bool large);

bool append_offset_or_size(String *dest, size_t offset_or_size, bool large);

bool append_int16(String *dest, int16_t value);

bool attempt_inline_value(const Json_dom *value, String *dest, size_t pos,
                          bool large);

uint32_t read_offset_or_size(const char *data, bool large);

bool inlined_type(uint8_t type, bool large);

uint8_t offset_size(bool large);
uint8_t key_entry_size(bool large);
uint8_t value_entry_size(bool large);

/**
  Serialize the JSON document represented by dom to binary format in
  the destination string, replacing any content already in the
  destination string.

  @param[in]     dom   the input DOM tree
  @param error_handler a handler that is invoked if an error occurs
  @param[in,out] dest  the destination string
  @retval false on success
  @retval true if an error occurred
      */
bool serialize(const Json_dom *dom,
               const JsonSerializationErrorHandler &error_handler,
               String *dest);

/**
  Class used for reading JSON values that are stored in the binary
  format. Values are parsed lazily, so that only the parts of the
  value that are interesting to the caller, are read. Array elements
  can be looked up in constant time using the element() function.
  Object members can be looked up in O(log n) time using the lookup()
  function.
*/
class Value {
 public:
  enum enum_type : uint8_t {
    OBJECT,
    ARRAY,
    STRING,
    INT,
    UINT,
    DOUBLE,
    LITERAL_NULL,
    LITERAL_TRUE,
    LITERAL_FALSE,
    OPAQUE,
    ERROR /* Not really a type. Used to signal that an
             error was detected. */
  };

  /**
    Does this value, and all of its members, represent a valid JSON
    value?
  */
  EXPORT_JSON_FUNCTION
  bool is_valid() const;

  EXPORT_JSON_FUNCTION
  enum_type type() const { return m_type; }

  /// Does this value use the large storage format?
  EXPORT_JSON_FUNCTION
  bool large_format() const { return m_large; }

  /**
    Get a pointer to the beginning of the STRING or OPAQUE data
    represented by this instance.
  */
  EXPORT_JSON_FUNCTION
  const char *get_data() const {
    assert(m_type == STRING || m_type == OPAQUE);
    return m_data;
  }

  /**
    Get the length in bytes of the STRING or OPAQUE value represented by
    this instance.
  */
  EXPORT_JSON_FUNCTION
  uint32_t get_data_length() const {
    assert(m_type == STRING || m_type == OPAQUE);
    return m_length;
  }

  /**
    Get the length in bytes of the STRING or OPAQUE value represented by
    this instance.
  */
  EXPORT_JSON_FUNCTION
  uint32_t get_container_length() const {
    assert(m_type == ARRAY || m_type == OBJECT);
    return m_length;
  }

  EXPORT_JSON_FUNCTION
  const char *get_container_data() const {
    assert(m_type == ARRAY || m_type == OBJECT);
    return m_data;
  }

  /** Get the value of an INT. */
  EXPORT_JSON_FUNCTION
  int64_t get_int64() const {
    assert(m_type == INT);
    return m_int_value;
  }

  /** Get the value of a UINT. */
  EXPORT_JSON_FUNCTION
  uint64_t get_uint64() const {
    assert(m_type == UINT);
    return static_cast<uint64_t>(m_int_value);
  }

  /** Get the value of a DOUBLE. */
  EXPORT_JSON_FUNCTION
  double get_double() const {
    assert(m_type == DOUBLE);
    return m_double_value;
  }

  /**
    Get the number of elements in an array, or the number of members in
    an object.
  */
  EXPORT_JSON_FUNCTION
  uint32_t element_count() const {
    assert(m_type == ARRAY || m_type == OBJECT);
    return m_element_count;
  }

  /**
    Get the MySQL field type of an opaque value. Identifies the type of
    the value stored in the data portion of an opaque value.
  */
  EXPORT_JSON_FUNCTION
  enum_field_types field_type() const {
    assert(m_type == OPAQUE);
    return m_field_type;
  }

  EXPORT_JSON_FUNCTION
  Value element(size_t pos) const;

  EXPORT_JSON_FUNCTION
  Value key(size_t pos) const;

  EXPORT_JSON_FUNCTION
  Value lookup(const char *key, size_t length) const;

  EXPORT_JSON_FUNCTION
  Value lookup(const std::string &key) const {
    return lookup(key.c_str(), key.length());
  }

  EXPORT_JSON_FUNCTION
  size_t lookup_index(const char *key, size_t length) const;

  EXPORT_JSON_FUNCTION
  size_t lookup_index(const std::string &key) const {
    return lookup_index(key.c_str(), key.length());
  }

  EXPORT_JSON_FUNCTION
  bool is_backed_by(const String *str) const;

  EXPORT_JSON_FUNCTION
  bool raw_binary(const JsonSerializationErrorHandler &error_handler,
                  String *buf) const;
  EXPORT_JSON_FUNCTION
  bool get_free_space(const JsonSerializationErrorHandler &error_handler,
                      size_t *space) const;
#ifdef MYSQL_SERVER
  bool update_in_shadow(const Field_json *field, size_t pos,
                        Json_wrapper *new_value, size_t data_offset,
                        size_t data_length, const char *original,
                        char *destination, bool *changed) const;
  bool remove_in_shadow(const Field_json *field, size_t pos,
                        const char *original, char *destination) const;
#endif
  EXPORT_JSON_FUNCTION
  bool has_space(size_t pos, size_t needed, size_t *offset) const;

  /** Constructor for values that represent literals or errors. */
  EXPORT_JSON_FUNCTION
  explicit Value(enum_type t) : m_data(nullptr), m_type(t) {
    assert(t == LITERAL_NULL || t == LITERAL_TRUE || t == LITERAL_FALSE ||
           t == ERROR);
  }

  /** Constructor for values that represent ints or uints. */
  EXPORT_JSON_FUNCTION
  explicit Value(enum_type t, int64_t val) : m_int_value(val), m_type(t) {
    assert(t == INT || t == UINT);
  }

  /** Constructor for values that represent doubles. */
  EXPORT_JSON_FUNCTION
  explicit Value(double val) : m_double_value(val), m_type(DOUBLE) {}

  /** Constructor for values that represent strings. */
  EXPORT_JSON_FUNCTION
  Value(const char *data, uint32_t len)
      : m_data(data), m_length(len), m_type(STRING) {}

  /**
    Constructor for values that represent arrays or objects.

    @param t type
    @param data pointer to the start of the binary representation
    @param bytes the number of bytes in the binary representation of the value
    @param element_count the number of elements or members in the value
    @param large true if the value should be stored in the large
    storage format with 4 byte offsets instead of 2 byte offsets
  */
  EXPORT_JSON_FUNCTION
  Value(enum_type t, const char *data, uint32_t bytes, uint32_t element_count,
        bool large)
      : m_data(data),
        m_element_count(element_count),
        m_length(bytes),
        m_type(t),
        m_large(large) {
    assert(t == ARRAY || t == OBJECT);
  }

  /** Constructor for values that represent opaque data. */
  EXPORT_JSON_FUNCTION
  Value(enum_field_types ft, const char *data, uint32_t len)
      : m_data(data), m_length(len), m_field_type(ft), m_type(OPAQUE) {}

  /** Empty constructor. Produces a value that represents an error condition. */
  EXPORT_JSON_FUNCTION
  Value() : Value(ERROR) {}

  /** Is this value an array? */
  EXPORT_JSON_FUNCTION
  bool is_array() const { return m_type == ARRAY; }

  /** Is this value an object? */
  EXPORT_JSON_FUNCTION
  bool is_object() const { return m_type == OBJECT; }

  /**
    Format the JSON value to an external JSON string in buffer in
    the format of ISO/IEC 10646.

    @param[in,out] buffer      the formatted string is appended, so make sure
                               the length is set correctly before calling
    @param[in] depth_handler   Pointer to a function that should handle error
                               occurred when depth is exceeded.

    @return false formatting went well, else true
  */
  EXPORT_JSON_FUNCTION
  bool to_std_string(std::string *buffer,
                     const JsonErrorHandler &depth_handler) const;

  /**
    Format the JSON value to an external JSON string in buffer in the format of
    ISO/IEC 10646. Add newlines and indentation for readability.

    @param[in,out] buffer     the buffer that receives the formatted string
                              (the string is appended, so make sure the length
                              is set correctly before calling)
    @param[in] depth_handler Pointer to a function that should handle error
                             occurred when depth is exceeded.

    @retval false on success
    @retval true on error
  */
  EXPORT_JSON_FUNCTION
  bool to_pretty_std_string(std::string *buffer,
                            const JsonErrorHandler &depth_handler) const;

  /**
    Compare two Values
    @note This function is limited to scalars only, for objects/arrays it
    asserts. The main purpose is to separate old/new scalar values for updates
    on multi-valued indexes.
    @returns
      -1  this < val
       0  this == val
       1  this > val
  */
#ifdef MYSQL_SERVER
  int eq(const Value &val) const;
#endif

  EXPORT_JSON_FUNCTION
  size_t key_entry_offset(size_t pos) const;

  EXPORT_JSON_FUNCTION
  size_t value_entry_offset(size_t pos) const;

 private:
  /*
    Instances use only one of m_data, m_int_value and m_double_value,
    so keep them in a union to save space in memory.
  */
  union {
    /**
      Pointer to the start of the binary representation of the value. Only
      used by STRING, OPAQUE, OBJECT and ARRAY.

      The memory pointed to by this member is not owned by this Value
      object. Callers that create Value objects must make sure that the
      memory is not freed as long as the Value object is alive.
    */
    const char *m_data;
    /** The value if the type is INT or UINT. */
    int64_t m_int_value;
    /** The value if the type is DOUBLE. */
    double m_double_value;
  };

  /**
    Element count for arrays and objects. Unused for other types.
  */
  uint32_t m_element_count;

  /**
    The full length (in bytes) of the binary representation of an array or
    object, or the length of a string or opaque value. Unused for other types.
  */
  uint32_t m_length;

  /**
    The MySQL field type of the value, in case the type of the value is
    OPAQUE. Otherwise, it is unused.
  */
  enum_field_types m_field_type;

  /** The JSON type of the value. */
  enum_type m_type;

  /**
    True if an array or an object uses the large storage format with 4
    byte offsets instead of 2 byte offsets.
  */
  bool m_large;

  bool first_value_offset(size_t *offset) const;
  bool element_offsets(size_t pos, size_t *start, size_t *end,
                       bool *inlined) const;
};

/**
  Parse a JSON binary document.

  @param[in] data  a pointer to the binary data
  @param[in] len   the size of the binary document in bytes
  @return an object that allows access to the contents of the document
*/
EXPORT_JSON_FUNCTION
Value parse_binary(const char *data, size_t len);

/**
  How much space is needed for a JSON value when it is stored in the binary
  format.

  @param[in]  value   the JSON value to add to a document
  @param[in]  large   true if the large storage format is used
  @param[out] needed  gets set to the amount of bytes needed to store
                      the value
  @retval false if successful
  @retval true if an error occurred while calculating the needed space
*/
#ifdef MYSQL_SERVER
bool space_needed(const Json_wrapper *value, bool large, size_t *needed);
#endif

/**
  Apply a function to every value in a JSON document. That is, apply
  the function to the root node of the JSON document, to all its
  children, grandchildren and so on.

  @param  value the root of the JSON document
  @param  func  the function to apply
  @retval true  if the processing was stopped
  @retval false if the processing was completed

  @tparam Func a functor type that takes a #json_binary::Value
  parameter and returns a `bool` which is `true` if the processing
  should stop or `false` if the processing should continue with the
  next node
*/
template <typename Func>
bool for_each_node(const Value &value, const Func &func) {
  if (func(value)) return true;

  if (value.is_array() || value.is_object())
    for (size_t i = 0, size = value.element_count(); i < size; ++i)
      if (for_each_node(value.element(i), func)) return true;

  return false;
}
}  // namespace json_binary

#endif /* JSON_BINARY_INCLUDED */
