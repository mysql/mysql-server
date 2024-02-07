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

#ifndef MYSQL_SERVER_STRING_SERVICE_H
#define MYSQL_SERVER_STRING_SERVICE_H

#include <mysql/components/component_implementation.h>
#include <mysql/components/service_implementation.h>
#include <mysql/components/services/mysql_string.h>

/*
  This header file is used in mysql_server component, which is not
  server-enabled, and can't have following declaration acquired by including
  mysql headers.
*/

class String;

/**
  The string functions as a service to the mysql_server component.
  So, that by default this service is available to all the components
  register to the server.
  successful invocations of the underlying String Service Implementation
  methods.
*/
class mysql_string_imp {
 public: /* service implementations */
  /* mysql_charset service. */

  static DEFINE_METHOD(CHARSET_INFO_h, get_charset_utf8mb4, ());

  static DEFINE_METHOD(CHARSET_INFO_h, get_charset_by_name, (const char *name));

  /**
    Creates a new instance of string object

    @param out_string holds pointer to newly created string object.
    @return Status of performed operation
    @retval false success
    @retval true failure
  */
  static DEFINE_BOOL_METHOD(create, (my_h_string * out_string));

  /**
    Destroys specified string object and data contained by it.

    @param string String object handle to release.
  */
  static DEFINE_METHOD(void, destroy, (my_h_string string));

  /**
    Convert a String pointed by handle to lower case. Conversion depends on the
    client character set info

    @param out_string Holds the converted lower case string object.
    @param in_string Pointer to string object to be converted.
    @return Status of performed operation
    @retval false success
    @retval true failure
  */
  static DEFINE_BOOL_METHOD(tolower,
                            (my_h_string * out_string, my_h_string in_string));

  /**
    Convert a String pointed by handle to upper case. Conversion depends on the
    client character set info

    @param out_string Holds the converted upper case string object.
    @param in_string Pointer to string object to be converted.
    @return Status of performed operation
    @retval false success
    @retval true failure
  */
  static DEFINE_BOOL_METHOD(toupper,
                            (my_h_string * out_string, my_h_string in_string));

  /* mysql_string_converter service. */

  /**
    Allocates a string object and converts the character buffer to string
    and just sets the specified charset_name in the string object. It does
    not performs the conversion of buffer into the specified character set.
    Caller must free the allocated string by calling destroy() api.

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
  static DEFINE_BOOL_METHOD(convert_from_buffer,
                            (my_h_string * out_string, const char *in_buffer,
                             uint64 length, const char *charset_name));

  /**
    Converts the mysql_string to the character set specified by
    charset_name parameter.

    @param in_string Pointer to string object handle to set new string
      to.
    @param [out] out_buffer Pointer to the buffer with data to be interpreted
      as characters.
    @param length Length of the buffer to hold out put in characters.
    @param charset_name charset that is used for conversion.
    @return Status of performed operation
    @retval false success
    @retval true failure
  */
  static DEFINE_BOOL_METHOD(convert_to_buffer,
                            (my_h_string in_string, char *out_buffer,
                             uint64 length, const char *charset_name));

  /* mysql_string_converter_v2 service. */

  static DEFINE_BOOL_METHOD(convert_from_buffer_v2,
                            (my_h_string dest_string, const char *src_buffer,
                             uint64 src_length, CHARSET_INFO_h src_charset));

  static DEFINE_BOOL_METHOD(convert_to_buffer_v2,
                            (my_h_string src_string, char *dest_buffer,
                             uint64 dest_length, CHARSET_INFO_h dest_charset));

  /**
    Gets character code of character on specified index position in
    string to a specified buffer.

    @param string String object handle to get character from.
    @param index Index, position of character to query.
    @param [out] out_char Pointer to unsigned long value to store character to.
    @return Status of performed operation
    @retval false success
    @retval true failure
  */
  static DEFINE_BOOL_METHOD(get_char,
                            (my_h_string string, uint index, ulong *out_char));

  /**
    Gets length of specified string expressed as number of characters.

    @param string String object handle to get length of.
    @param [out] out_length Pointer to 64bit value to store length of string to.
    @return Status of performed operation
    @retval false success
    @retval true failure
  */
  static DEFINE_BOOL_METHOD(get_char_length,
                            (my_h_string string, uint *out_length));

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
  static DEFINE_BOOL_METHOD(get_byte,
                            (my_h_string string, uint index, uint *out_char));

  /**
    Gets length of specified string expressed as number of bytes.

    @param string String object handle to get length of.
    @param [out] out_length Pointer to 32bit value to store length of string to.
    @return Status of performed operation
    @retval false success
    @retval true failure
  */
  static DEFINE_BOOL_METHOD(get_byte_length,
                            (my_h_string string, uint *out_length));

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
  static DEFINE_BOOL_METHOD(iterator_create,
                            (my_h_string string,
                             my_h_string_iterator *out_iterator));

  /**
    Retrieves character code at current iterator position and advances the
      iterator.

    @param iter String iterator object handle to advance.
    @param [out] out_char Pointer to 64bit value to store character to. May be
      NULL to omit retrieval of character and just advance the iterator.
    @return Status of performed operation
    @retval false success
    @retval true failure
  */
  static DEFINE_BOOL_METHOD(iterator_get_next,
                            (my_h_string_iterator iter, int *out_char));

  /**
    Releases the string iterator object specified.

    @param iter String iterator object handle to release.
  */
  static DEFINE_METHOD(void, iterator_destroy, (my_h_string_iterator iter));

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
  static DEFINE_BOOL_METHOD(is_upper, (my_h_string_iterator iter, bool *out));

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
  static DEFINE_BOOL_METHOD(is_lower, (my_h_string_iterator iter, bool *out));

  /**
    Checks if character on current position the iterator points to is a digit.

    @param iter String iterator object handle to advance.
    @param [out] out Pointer to bool value to store if character is a digit.
    @return Status of performed operation
    @retval false success
    @retval true failure
  */
  static DEFINE_BOOL_METHOD(is_digit, (my_h_string_iterator iter, bool *out));

  /**
    Retrieves character value at current iterator position

    @param iter       String iterator object handle
    @param [out] out  Pointer to long value to store character to

    @return Status of performed operation
      @retval false success
      @retval true failure
  */
  static DEFINE_BOOL_METHOD(get, (my_h_string_iterator iter, ulong *out));

  static DEFINE_BOOL_METHOD(reset, (my_h_string s));

  static DEFINE_BOOL_METHOD(append, (my_h_string s1, my_h_string s2));

  /**
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
  static DEFINE_BOOL_METHOD(substr, (my_h_string in_string, uint offset,
                                     uint count, my_h_string *out_string));

  static DEFINE_BOOL_METHOD(compare,
                            (my_h_string s1, my_h_string s2, int *cmp));

  static DEFINE_BOOL_METHOD(get_data,
                            (my_h_string s, const char **buffer_pointer,
                             size_t *buffer_length,
                             CHARSET_INFO_h *buffer_charset));
};
#endif /* MYSQL_SERVER_STRING_SERVICE_H */
