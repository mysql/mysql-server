/* Copyright (c) 2017, 2024, Oracle and/or its affiliates.

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

#ifndef MYSQL_STRING_SERVICE_H
#define MYSQL_STRING_SERVICE_H

#include <mysql/components/service.h>

#include <mysql/components/services/bits/mysql_string_bits.h>

#include "my_inttypes.h"

/* clang-format off */
/**
  @defgroup group_string_component_services_inventory MySQL string services
  @ingroup group_components_services_inventory
*/

/* clang-format on */

DEFINE_SERVICE_HANDLE(CHARSET_INFO_h);

/**
  Get the "utf8mb4" CHARSET_INFO.
*/
typedef CHARSET_INFO_h (*get_charset_utf8mb4_v1_t)();

/**
  Find a CHARSET_INFO by name.
  @param name The character set name, expressed in ASCII, zero terminated.
*/
typedef CHARSET_INFO_h (*get_charset_by_name_v1_t)(const char *name);

/**
  @ingroup group_string_component_services_inventory

  Lookup available character sets.
  Status: Active.
*/
BEGIN_SERVICE_DEFINITION(mysql_charset)
/** @sa get_charset_utf8mb4_v1_t */
get_charset_utf8mb4_v1_t get_utf8mb4;
/** @sa get_charset_by_name_v1_t */
get_charset_by_name_v1_t get;
END_SERVICE_DEFINITION(mysql_charset)

/**
  The string functions as a service to the mysql_server component.
  So, that by default this service is available to all the components
  register to the server.
*/

DEFINE_SERVICE_HANDLE(my_h_string);
DEFINE_SERVICE_HANDLE(my_h_string_iterator);

/**
  @ingroup group_string_component_services_inventory

  Service for String create and destroy.
*/
BEGIN_SERVICE_DEFINITION(mysql_string_factory)
/**
  Creates a new instance of string object

  @param out_string holds pointer to newly created string object.
  @return Status of performed operation
  @retval false success @retval true failure
*/
DECLARE_BOOL_METHOD(create, (my_h_string * out_string));

/**
  Destroys specified string object and data contained by it.

  @param string String object handle to release.
  @return Status of performed operation
  @retval false success
  @retval true failure
*/
DECLARE_METHOD(void, destroy, (my_h_string string));
END_SERVICE_DEFINITION(mysql_string_factory)

/**
  @ingroup group_string_component_services_inventory

  Service for String case conversions, to lower case and to upper case.
*/
BEGIN_SERVICE_DEFINITION(mysql_string_case)
/**
  Convert a String pointed by handle to lower case. Conversion depends on the
  client character set info

  @param out_string Holds the converted lower case string object.
  @param in_string Pointer to string object to be converted.
  @return Status of performed operation
  @retval false success
  @retval true failure
*/
DECLARE_BOOL_METHOD(tolower, (my_h_string * out_string, my_h_string in_string));

/**
  Convert a String pointed by handle to upper case. Conversion depends on the
  client character set info

  @param out_string Holds the converted upper case string object.
  @param in_string Pointer to string object to be converted.
  @return Status of performed operation
  @retval false success
  @retval true failure
*/
DECLARE_BOOL_METHOD(toupper, (my_h_string * out_string, my_h_string in_string));
END_SERVICE_DEFINITION(mysql_string_case)

/**
  @ingroup group_string_component_services_inventory

  Service for conversions, string to buffer and buffer to string.
  Status: Deprecated, use mysql_string_charset_converter instead.
*/
BEGIN_SERVICE_DEFINITION(mysql_string_converter)
/**
  allocates a string object and converts the character buffer to string
  of specified charset_name.
  please call destroy() api to free the allocated string after this api.

  @param [out] out_string Pointer to string object handle to set new string
    to.
  @param in_buffer Pointer to the buffer with data to be interpreted as
    string.
  @param length Length of the buffer to copy into string, in bytes, not in
    character count.
  @param charset_name charset that is used for conversion.
  @return Status of performed operation
  @retval false success
  @retval true failure
*/
DECLARE_BOOL_METHOD(convert_from_buffer,
                    (my_h_string * out_string, const char *in_buffer,
                     uint64 length, const char *charset_name));

/**
  converts the mysql_string to the character set specified by
  charset_name parameter.

  @param in_string Pointer to string object handle to set new string
    to.
  @param [out] out_buffer Pointer to the buffer with data to be interpreted
    as characters.
  @param length Length of the buffer to hold out put in characters.
  @param charset_name Handle to charset that is used for conversion.
  @return Status of performed operation
  @retval false success
  @retval true failure
*/
DECLARE_BOOL_METHOD(convert_to_buffer,
                    (my_h_string in_string, char *out_buffer, uint64 length,
                     const char *charset_name));
END_SERVICE_DEFINITION(mysql_string_converter)

/**
  Converts a character buffer to string of specified charset
  to a string object.
  The caller provides the destination string object,
  which content will be modified.

  @param [in,out] dest_string Destination string.
  @param src_buffer Source buffer
  @param src_length Length of the source buffer, in bytes
  @param src_charset CHARSET is the source buffer
  @return Conversion status
  @retval false success
  @retval true failure
*/
typedef mysql_service_status_t (*convert_from_buffer_v2_t)(
    my_h_string dest_string, const char *src_buffer, uint64 src_length,
    CHARSET_INFO_h src_charset);

/**
  Converts the mysql_string to a given character set.

  @param src_string Source string to convert
  @param [out] dest_buffer Destination buffer
  @param dest_length Length, in charset characters, of the destination buffer
  @param dest_charset Destination CHARSET, that is, character set to convert to
  @return Conversion status
  @retval false success
  @retval true failure
*/
typedef mysql_service_status_t (*convert_to_buffer_v2_t)(
    my_h_string src_string, char *dest_buffer, uint64 dest_length,
    CHARSET_INFO_h dest_charset);

/**
  @ingroup group_string_component_services_inventory

  Service for conversions, string to buffer and buffer to string.
  Status: Active.
*/
BEGIN_SERVICE_DEFINITION(mysql_string_charset_converter)
/** @sa convert_from_buffer_v2_t */
convert_from_buffer_v2_t convert_from_buffer;
/** @sa convert_to_buffer_v2_t */
convert_to_buffer_v2_t convert_to_buffer;
END_SERVICE_DEFINITION(mysql_string_charset_converter)

/**
  @ingroup group_string_component_services_inventory

  Service to get a character in String and number of characters in string
*/
BEGIN_SERVICE_DEFINITION(mysql_string_character_access)
/**
  Gets client character code of character on specified index position in
  string to a specified buffer.

  @param string String object handle to get character from.
  @param index Index, position of character to query.
  @param [out] out_char Pointer to long value to store character to.
  @return Status of performed operation
  @retval false success
  @retval true failure
*/
DECLARE_BOOL_METHOD(get_char,
                    (my_h_string string, uint index, ulong *out_char));

/**
  Gets length of specified string expressed as number of characters.

  @param string String object handle to get length of.
  @param [out] out_length Pointer to 64bit value to store length of string to.
  @return Status of performed operation
  @retval false success
  @retval true failure
*/
DECLARE_BOOL_METHOD(get_char_length, (my_h_string string, uint *out_length));
END_SERVICE_DEFINITION(mysql_string_character_access)

/**
  @ingroup group_string_component_services_inventory

  Service to get a byte in String and number of bytes in string
*/
BEGIN_SERVICE_DEFINITION(mysql_string_byte_access)
/**
  Gets byte code of string at specified index position to a
  specified 32-bit buffer.

  @param string String object handle to get character from.
  @param index Index, position of character to query.
  @param [out] out_char Pointer to 32bit value to store byte to.
  @return Status of performed operation
  @retval false success
  @retval true failure
*/
DECLARE_BOOL_METHOD(get_byte, (my_h_string string, uint index, uint *out_char));

/**
  Gets length of specified string expressed as number of bytes.

  @param string String object handle to get length of.
  @param [out] out_length Pointer to 32bit value to store length of string to.
  @return Status of performed operation
  @retval false success
  @retval true failure
*/
DECLARE_BOOL_METHOD(get_byte_length, (my_h_string string, uint *out_length));
END_SERVICE_DEFINITION(mysql_string_byte_access)

/**
  @ingroup group_string_component_services_inventory

  Service for listing Strings by iterator.
*/
BEGIN_SERVICE_DEFINITION(mysql_string_iterator)
/**
  Creates an iterator for a specified string to allow iteration through all
    characters in the string.

  @param string String object handle to get iterator to.
  @param [out] out_iterator Pointer to string iterator handle to store result
    object to.
  @return Status of performed operation
  @retval false success
  @retval true failure
*/
DECLARE_BOOL_METHOD(iterator_create,
                    (my_h_string string, my_h_string_iterator *out_iterator));

/**
  Retrieves character type code at current iterator position and advances the
  iterator. Character type code is a bit mask describing various properties.
  Refer to types in mysql_string_bits.h

  @param iter String iterator object handle to advance.
  @param [out] out_ctype Pointer to 64bit value to store character type.
                         May be NULL to omit retrieval and just advance
                         the iterator.
  @return Status of performed operation
  @retval false success
  @retval true failure
*/
DECLARE_BOOL_METHOD(iterator_get_next,
                    (my_h_string_iterator iter, int *out_ctype));

/**
  Releases the string iterator object specified.

  @param iter String iterator object to handle the release.
  @return Status of performed operation
  @retval false success
  @retval true failure
*/
DECLARE_METHOD(void, iterator_destroy, (my_h_string_iterator iter));
END_SERVICE_DEFINITION(mysql_string_iterator)

/**
  @ingroup group_string_component_services_inventory

  Service for String c_type.
*/
BEGIN_SERVICE_DEFINITION(mysql_string_ctype)
/**
  Checks if character on current position the iterator points to is an upper
    case.

  @param iter String iterator object handle to advance.
  @param [out] out Pointer to bool value to store if character is an upper
    case.
  @return Status of performed operation
  @retval false success
  @retval true failure
*/
DECLARE_BOOL_METHOD(is_upper, (my_h_string_iterator iter, bool *out));

/**
  Checks if character on current position the iterator points to is a lower
    case.

  @param iter String iterator object handle to advance.
  @param [out] out Pointer to bool value to store if character is a lower
    case.
  @return Status of performed operation
  @retval false success
  @retval true failure
*/
DECLARE_BOOL_METHOD(is_lower, (my_h_string_iterator iter, bool *out));

/**
  Checks if character on current position the iterator points to is a digit.

  @param iter String iterator object handle to advance.
  @param [out] out Pointer to bool value to store if character is a digit.
  @return Status of performed operation
  @retval false success
  @retval true failure
*/
DECLARE_BOOL_METHOD(is_digit, (my_h_string_iterator iter, bool *out));
END_SERVICE_DEFINITION(mysql_string_ctype)

/**
  @ingroup group_string_component_services_inventory

  Service for retrieving one character from a string.
  It relies on string iterator and access ulong representation
  of the character.
*/
BEGIN_SERVICE_DEFINITION(mysql_string_value)

/**
  Retrieves character value at current iterator position

  @param iter       String iterator object handle
  @param [out] out  Pointer to long value to store character to

  @return Status of performed operation
    @retval false success
    @retval true failure
*/
DECLARE_BOOL_METHOD(get, (my_h_string_iterator iter, ulong *out));

END_SERVICE_DEFINITION(mysql_string_value)

/* mysql_string_manipulation_v1 service. */

/**
  Reset a string to the empty string.

  @param [in, out] s The string to reset.
*/
typedef mysql_service_status_t (*mysql_string_reset_v1_t)(my_h_string s);

/**
  @ingroup group_string_component_services_inventory

  Reset a string to the empty string.
*/
BEGIN_SERVICE_DEFINITION(mysql_string_reset)
mysql_string_reset_v1_t reset;
END_SERVICE_DEFINITION(mysql_string_reset)

/**
  Substring.
  Allocates a string object and sets it value as substring of the input
  string. Caller must free the allocated string by calling destroy().

  @param [in]  in_string   String handle to extract substring from.
  @param [in]  offset      Character offset of the substring.
  @param [in]  count       Number of characters of the substring.
  @param [out] out_string  Pointer to string handle holding the created result
string.
  @return Status of performed operation
  @retval false success
  @retval true failure
*/
typedef mysql_service_status_t (*mysql_string_substr_v1_t)(
    my_h_string in_string, uint offset, uint count, my_h_string *out_string);

/**
  @ingroup group_string_component_services_inventory

  Substring a string.
*/
BEGIN_SERVICE_DEFINITION(mysql_string_substr)
mysql_string_substr_v1_t substr;
END_SERVICE_DEFINITION(mysql_string_substr)

/**
  Append a string.

  @param [in, out] s1 The string to append to.
  @param [in] s2 The string to append.
*/
typedef mysql_service_status_t (*mysql_string_append_v1_t)(my_h_string s1,
                                                           my_h_string s2);

/**
  @ingroup group_string_component_services_inventory

  Append a string to another one.
*/
BEGIN_SERVICE_DEFINITION(mysql_string_append)
mysql_string_append_v1_t append;
END_SERVICE_DEFINITION(mysql_string_append)

/**
  Compare two strings.

  @param [in] s1 First string to compare.
  @param [in] s2 Second string to compare.
  @param [out] cmp Comparison result (negative, zero, or positive) .
*/
typedef mysql_service_status_t (*mysql_string_compare_v1_t)(my_h_string s1,
                                                            my_h_string s2,
                                                            int *cmp);

/**
  @ingroup group_string_component_services_inventory

  Compare two strings.
*/
BEGIN_SERVICE_DEFINITION(mysql_string_compare)
mysql_string_compare_v1_t compare;
END_SERVICE_DEFINITION(mysql_string_compare)

/**
  Access the string raw data.
  The raw data returned is usable only while the string object
  is valid.

  @param [in] s String
  @param [out] buffer_pointer String raw buffer.
  @param [out] buffer_length String raw buffer size.
  @param [out] buffer_charset String character set.
*/
typedef mysql_service_status_t (*mysql_string_get_data_v1_t)(
    my_h_string s, const char **buffer_pointer, size_t *buffer_length,
    CHARSET_INFO_h *buffer_charset);

/**
  @ingroup group_string_component_services_inventory

  Access the string raw data.
*/
BEGIN_SERVICE_DEFINITION(mysql_string_get_data_in_charset)
mysql_string_get_data_v1_t get_data;
END_SERVICE_DEFINITION(mysql_string_get_data_in_charset)

#endif /* MYSQL_STRING_SERVICE_H */
