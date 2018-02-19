/* Copyright (c) 2014, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef DD__PROPERTIES_INCLUDED
#define DD__PROPERTIES_INCLUDED

#include <limits>
#include <map>

#include "m_string.h"  // my_strtoll10
#include "my_dbug.h"
#include "sql/dd/string_type.h"  // String_type, Stringstream_type

struct MEM_ROOT;

namespace dd {

class Properties;

///////////////////////////////////////////////////////////////////////////

/**
  The Properties class defines an interface for storing key=value pairs,
  where both key and value may be UTF-8 strings.

  The interface contains functions for testing whether a key exists,
  replacing or removing key=value pairs, iteration etc. The interface
  also defines a set of static conversion functions for converting between
  strings and various primitive types. The conversion functions are
  also wrapped by corresponding set and get functions to e.g. set an
  int64 directly.

  Please note the difference between the value() function, which returns the
  string value for a given key, and the get_XXX() functions, which return a
  bool indicating operation outcome, and returns the actual value by a
  parameter. The motivation here is to easily support handling conversion
  errors.

  The raw_string function returns a semicolon separated list of all
  key=value pairs. Characters '=' and ';' that are part of key or value
  are escaped using the '\' as an escape character. The escape character
  itself must also be escaped if being part of key or value.

  Examples of usage:

    Add key=value:
      p->set("akey=avalue");

    Add a numeric value:
      p->set_int32("intvalue", 1234);

    Get values:
      String_type str= p->value("akey");
      const char* c_str= p->value_cstr("akey");

      int32 num;
      p->get_int32("intvalue", &num);

    Get raw string:
      String_type mylist= p->raw_string();

  Further comments can be found in the files properties_impl.{h,cc}
  where the interface is implemented.
 */

class Properties {
 public:
  // A wrapper for Properties_impl::parse_properties()
  static Properties *parse_properties(const String_type &raw_properties);

 public:
  typedef std::map<String_type, String_type> Map;
  typedef std::map<String_type, String_type>::size_type size_type;
  typedef Map::iterator Iterator;
  typedef Map::const_iterator Const_iterator;

 public:
  virtual Iterator begin() = 0;
  virtual Const_iterator begin() const = 0;

  virtual Iterator end() = 0;
  virtual Const_iterator end() const = 0;

 private:
  /**
    Hide the assignment operator by declaring it private
  */
  Properties &operator=(const Properties &properties);

 public:
  /**
    Get implementation object

    @return pointer to the instance implementing this interface
  */
  virtual const class Properties_impl *impl() const = 0;

  /**
    Assign a different property object by deep copy

    @pre The 'this' object shall be empty

    @param properties Object which will have its properties
                      copied to 'this' object

    @return Reference to 'this'
  */
  virtual Properties &assign(const Properties &properties) = 0;

  /**
    Get the number of key=value pairs.

    @return number of key=value pairs
  */
  virtual size_type size() const = 0;

  /**
    Are there any key=value pairs ?

    @return true if there is no key=value pair else false
  */
  virtual bool empty() const = 0;

  /**
    Remove all key=value pairs.
  */
  virtual void clear() = 0;

  /**
    Check for the existence of a key=value pair given the key.

    @return true if the given key exists, false otherwise
  */
  virtual bool exists(const String_type &key) const = 0;

  /**
    Remove the key=value pair for the given key if it exists.
    Otherwise, do nothing.

    @param key key to lookup
    @return    false if the given key existed, true otherwise
  */
  virtual bool remove(const String_type &key) = 0;

  /**
    Create a string containing all key=value pairs as a semicolon
    separated list. Key and value are separated by '='. The '=' and
    ';' characters are escaped using '\' if part of key or value, hence,
    the escape character '\' must also be escaped.

    @return a string listing all key=value pairs
  */
  virtual const String_type raw_string() const = 0;

  /**
    Return the string value for a given key.
    Asserts if the key does not exist.

    @param key key to lookup the value for
    @return the value
  */
  virtual const String_type &value(const String_type &key) const = 0;

  /**
    Return the '\0' terminated char * value for a given key.
    Asserts if the key does not exist.

    @param key key to lookup the value for
    @return the char * value
  */
  virtual const char *value_cstr(const String_type &key) const = 0;

  /**
    Get the string value for a given key. Return true if the operation
    fails, i.e., if the key does not exist.

    @param      key   key to lookup the value for
    @param[out] value string value
    @return           Operation outcome, false if success, otherwise true
  */
  virtual bool get(const String_type &key, String_type &value) const = 0;

  /**
    Get the string value for a given key. Return true if the operation
    fails, i.e., if the key does not exist.

    @param      key      key to lookup the value for
    @param[out] value    LEX_STRING value
    @param[in]  mem_root MEM_ROOT to allocate string
    @return              Operation outcome, false if success, otherwise true
  */
  virtual bool get(const String_type &key, LEX_STRING &value,
                   MEM_ROOT *mem_root) const = 0;

  /**
    Get string value for key and convert the string to int64 (signed).

    @param      key   key to lookup
    @param[out] value converted value
    @return           Operation outcome, false if success, otherwise true
  */
  virtual bool get_int64(const String_type &key, int64 *value) const = 0;

  /**
    Get string value for key and convert the string to uint64 (unsigned).

    @param      key   key to lookup
    @param[out] value converted value
    @return           Operation outcome, false if success, otherwise true
  */
  virtual bool get_uint64(const String_type &key, uint64 *value) const = 0;

  /**
    Get string value for key and convert the string to int32 (signed).

    @param      key   key to lookup
    @param[out] value converted value
    @return           Operation outcome, false if success, otherwise true
  */
  virtual bool get_int32(const String_type &key, int32 *value) const = 0;

  /**
    Get string value for key and convert the string to uint32 (unsigned).

    @param      key   key to lookup
    @param[out] value converted value
    @return           Operation outcome, false if success, otherwise true
  */
  virtual bool get_uint32(const String_type &key, uint32 *value) const = 0;

  /**
    Get string value for key and convert the string to bool. Valid
    values are "true", "false", and decimal numbers, where "0" will
    be taken to mean false, and numbers != 0 will be taken to mean
    true.

    @param      key   key to lookup
    @param[out] value converted value
    @return           Operation outcome, false if success, otherwise true
  */
  virtual bool get_bool(const String_type &key, bool *value) const = 0;

  /**
    Add a new key=value pair. If the key already exists, the
    associated value will be replaced by the new value argument.

    @param key   key to lookup
    @param value value to be associated with the key
  */
  virtual void set(const String_type &key, const String_type &value) = 0;

  /**
    Add a new key=value pair where the value is an int64. The
    integer is converted to string.

    @param key   key to lookup
    @param value int64 value to be associated with the key
  */
  virtual void set_int64(const String_type &key, int64 value) = 0;

  /**
    Add a new key=value pair where the value is a uint64. The
    integer is converted to string.

    @param key   key to lookup
    @param value uint64 value to be associated with the key
  */
  virtual void set_uint64(const String_type &key, uint64 value) = 0;

  /**
    Add a new key=value pair where the value is an int32. The
    integer is converted to string.

    @param key   key to lookup
    @param value int32 value to be associated with the key
  */
  virtual void set_int32(const String_type &key, int32 value) = 0;

  /**
    Add a new key=value pair where the value is a uint32. The
    integer is converted to string.

    @param key   key to lookup
    @param value uint32 value to be associated with the key
  */
  virtual void set_uint32(const String_type &key, uint32 value) = 0;

  /**
    Add a new key=value pair where the value is a bool. The
    bool is converted to string, 'false' is represented as "0"
    while 'true' is represented as "1".

    @param key   key to lookup
    @param value bool value to be associated with the key
  */
  virtual void set_bool(const String_type &key, bool value) = 0;

  /**
    Convert a string to int64 (signed).

    @param      number string containing decimal number to convert
    @param[out] value  converted value
    @return            Operation outcome, false if success, otherwise true
  */
  static bool to_int64(const String_type &number, int64 *value) {
    return to_int<int64>(number, value);
  }

  /**
    Convert a string to uint64 (unsigned).

    @param      number string containing decimal number to convert
    @param[out] value  converted value
    @return            Operation outcome, false if success, otherwise true
  */
  static bool to_uint64(const String_type &number, uint64 *value) {
    return to_int<uint64>(number, value);
  }

  /**
    Convert a string to int32 (signed).

    @param      number string containing decimal number to convert
    @param[out] value  converted value
    @return            Operation outcome, false if success, otherwise true
  */
  static bool to_int32(const String_type &number, int32 *value) {
    return to_int<int32>(number, value);
  }

  /**
    Convert a string to uint32 (unsigned).

    @param      number string containing decimal number to convert
    @param[out] value  converted value
    @return            Operation outcome, false if success, otherwise true
  */
  static bool to_uint32(const String_type &number, uint32 *value) {
    return to_int<uint32>(number, value);
  }

  /**
    Convert string to bool. Valid values are "true", "false", and
    decimal numbers, where "0" will be taken to mean false, and
    numbers != 0 will be taken to mean true.

    @param      bool_str string containing boolean value to convert
    @param[out] value    converted value
    @return              Operation outcome, false if success, otherwise true
  */
  static bool to_bool(const String_type &bool_str, bool *value) {
    uint64 tmp_uint64 = 0;
    int64 tmp_int64 = 0;

    DBUG_ASSERT(value != NULL);

    if (bool_str == "true") {
      *value = true;
      return false;
    }

    if (bool_str == "false" || bool_str == "0") {
      *value = false;
      return false;
    }

    // We already tested for "0" above. Now, check if invalid number
    if (to_uint64(bool_str, &tmp_uint64) && to_int64(bool_str, &tmp_int64))
      return true;

    // Valid number, signed or unsigned, != 0 => interpret as true
    *value = true;
    return false;
  }

  /**
    Convert an int64 to a string.

    @param value int64 to convert
    @return      string containing decimal representation of the value
  */
  static String_type from_int64(int64 value) { return from_int<int64>(value); }

  /**
    Convert a uint64 to a string.

    @param value uint64 to convert
    @return      string containing decimal representation of the value
  */
  static String_type from_uint64(uint64 value) {
    return from_int<uint64>(value);
  }

  /**
    Convert an int32 to a string.

    @param value int32 to convert
    @return      string containing decimal representation of the value
  */
  static String_type from_int32(int32 value) { return from_int<int32>(value); }

  /**
    Convert a uint32 to a string.

    @param value uint32 to convert
    @return      string containing decimal representation of the value
  */
  static String_type from_uint32(uint32 value) {
    return from_int<uint32>(value);
  }

  /**
    Convert a bool to string, 'true' is encoded as "1", 'false'
    is encoded as "0".

    @param    value   bool variable to convert to string
    @return           string containing converted value
      @retval "1"     if value == true
      @retval "0"     if value == false
  */
  static String_type from_bool(bool value) {
    String_type str("0");
    if (value) str = "1";

    return str;
  }

 protected:
  /**
    Convert a string to an integer. Verify correct sign, check
    for overflow and conversion errors.

    @param      number string containing decimal number to convert
    @param[out] value  converted value

    @return            Operation status
      @retval true     if an error occurred
      @retval false    if success
  */
  template <class T>
  static bool to_int(const String_type &number, T *value) {
    int error_code;
    int64 tmp = 0;
    int64 trg_min = static_cast<int64>(std::numeric_limits<T>::min());
    int64 trg_max = static_cast<int64>(std::numeric_limits<T>::max());

    DBUG_ASSERT(value != NULL);

    // The target type must be an integer
    if (!(std::numeric_limits<T>::is_integer)) return true;

    // Do the conversion to an 8 byte signed integer
    tmp = my_strtoll10(number.c_str(), NULL, &error_code);

    // Check for conversion errors, including boundaries for 8 byte integers
    if (error_code != 0 && error_code != -1) return true;

    // Signs must match
    if (error_code == -1 && !std::numeric_limits<T>::is_signed) return true;

    // Overflow if positive source, negative result and signed target type
    if (error_code == 0 && tmp < 0 && std::numeric_limits<T>::is_signed)
      return true;

    // If the target type is less than 8 bytes, check boundaries
    if (sizeof(T) < 8 && (tmp < trg_min || tmp > trg_max)) return true;

    // Finally, cast to target type
    *value = static_cast<T>(tmp);
    return false;
  }

  /**
    Convert an integer to a string. Create an output stream and write the
    integer.

    @param value number to convert
    @return      string containing decimal representation of the value
  */
  template <class T>
  static String_type from_int(T value) {
    Stringstream_type ostream;
    ostream << value;
    return ostream.str();
  }

 public:
  virtual ~Properties() {}
};

///////////////////////////////////////////////////////////////////////////

}  // namespace dd

#endif  // DD__PROPERTIES_INCLUDED
