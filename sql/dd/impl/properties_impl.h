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

#ifndef DD__PROPERTIES_IMPL_INCLUDED
#define DD__PROPERTIES_IMPL_INCLUDED

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "lex_string.h"
#include "mem_root_fwd.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_sys.h"                // strmake_root
#include "sql/dd/properties.h"     // dd::Properties
#include "sql/dd/string_type.h"    // dd::String_type

namespace dd {

///////////////////////////////////////////////////////////////////////////


/**
  The Properties_impl class implements the Properties interface.

  The key=value pairs are stored in a std::map. An instance can be created
  either by means of the default constructor, which creates an object
  with an empty map, or alternatively, it can be created by means of the
  static parse_properties function with a String_type argument. The string
  is supposed to contain a semicolon separated list of key=value pairs,
  where the characters '=' and ';' also may be part of key or value by
  escaping using the '\' as an escape character. The escape character
  itself must also be escaped if being part of key or value. All characters
  between '=' and ';' are considered part of key or value, whitespace is
  not ignored.

  Escaping is removed during parsing so the strings in the map are not
  escaped. Escaping is only relevant in the context of raw strings that
  are to be parsed, and raw strings that are returned containing all
  key=value pairs.

  Example (note \\ due to escaping of C string literals):
    parse_properties("a=b;b = c")     -> ("a", "b"), ("b ", " c")
    parse_properties("a\\==b;b=\\;c") -> ("a=", "b"), ("b", ";c")

    get("a=") == "b"
    get("b")  == ";c"

  Additional key=value pairs may be added by means of the set function,
  which takes a string argument that is assumed to be unescaped.

  Further comments can be found in the file properties_impl.cc where most
  of the functions are implemented. Please also refer to the comments in the
  file properties.h where the interface is defined; the functions in the
  interface are commented there.
*/

class Properties_impl : public Properties
{
public:
  static Properties *parse_properties(const String_type &raw_properties);

public:
  Properties_impl();

  virtual const Properties_impl *impl() const
  { return this; }

  virtual Properties::Iterator begin()
  { return m_map->begin(); }

  /* purecov: begin inspected */
  virtual Properties::Const_iterator begin() const
  { return m_map->begin(); }
  /* purecov: end */

  virtual Properties::Iterator end()
  { return m_map->end(); }

  /* purecov: begin inspected */
  virtual Properties::Const_iterator end() const
  { return m_map->end(); }
  /* purecov: end */

  virtual size_type size() const
  { return m_map->size(); }

  virtual bool empty() const
  { return m_map->empty(); }

  /* purecov: begin deadcode */
  virtual void clear()
  { return m_map->clear(); }
  /* purecov: end */

  virtual bool exists(const String_type &key) const
  { return m_map->find(key) != m_map->end(); }

  virtual bool remove(const String_type &key)
  {
    Properties::Iterator it= m_map->find(key);

    if (it == m_map->end())
      return true;

    m_map->erase(it);
    return false;
  }

  virtual const String_type raw_string() const;

  /*
    The following methods value(), value_cstr(), get*() assert '
    if the key supplied does not exist. OR if the value could not
    be converted to desired numeric value.

    If these functions assert, that means that there is something
    wrong in the code and needs to be fixed. DD user should
    invoke these function after making sure that such a key
    exists.
  */

  virtual const String_type &value(const String_type &key) const
  {
    Properties::Const_iterator it= m_map->find(key);
    if (it == m_map->end())
    {
      // Key not present.
      DBUG_ASSERT(false); /* purecov: inspected */
      return Properties_impl::EMPTY_STR;
    }

    return it->second;
  }

  virtual const char* value_cstr(const String_type &key) const
  { return value(key).c_str(); }

  virtual bool get(const String_type &key,
                   String_type &value) const
  {
    if (exists(key))
    {
      value= this->value(key);
      return false;
    }
    return true;
  }

  virtual bool get(const String_type &key,
                   LEX_STRING &value,
                   MEM_ROOT *mem_root) const
  {
    if (exists(key))
    {
      String_type str= this->value(key);
      value.length= str.length();
      value.str= (char*) strmake_root(
                           mem_root,
                           str.c_str(),
                           str.length());
      return false;
    }
    return true;
  }

  virtual bool get_int64(const String_type &key, int64 *value) const
  {
    String_type str= this->value(key);

    if (to_int64(str, value))
    {
      DBUG_ASSERT(false); /* purecov: inspected */
      return true;
    }

    return false;
  }

  virtual bool get_uint64(const String_type &key, uint64 *value) const
  {
    String_type str= this->value(key);

    if (to_uint64(str, value))
    {
      DBUG_ASSERT(false); /* purecov: inspected */
      return true;
    }

    return false;
  }

  virtual bool get_int32(const String_type &key, int32 *value) const
  {
    String_type str= this->value(key);

    if (to_int32(str, value))
    {
      DBUG_ASSERT(false); /* purecov: inspected */
      return true;
    }

    return false;
  }

  virtual bool get_uint32(const String_type &key, uint32 *value) const
  {
    String_type str= this->value(key);

    if (to_uint32(str, value))
    {
      DBUG_ASSERT(false); /* purecov: inspected */
      return true;
    }

    return false;
  }

  virtual bool get_bool(const String_type &key, bool *value) const
  {
    String_type str= this->value(key);

    if (to_bool(str, value))
    {
      DBUG_ASSERT(false); /* purecov: inspected */
      return true;
    }

    return false;
  }


  // Set with implicit conversion from primitive types to string

  virtual void set(const String_type &key, const String_type &value)
  {
    if (key != "")
      (*m_map)[key]= value;
  }

  virtual void set_int64(const String_type &key, int64 value)
  { set(key, from_int64(value)); }

  virtual void set_uint64(const String_type &key, uint64 value)
  { set(key, from_uint64(value)); }

  virtual void set_int32(const String_type &key, int32 value)
  { set(key, from_int32(value)); }

  virtual void set_uint32(const String_type &key, uint32 value)
  { set(key, from_uint32(value)); }

  virtual void set_bool(const String_type &key, bool value)
  { set(key, from_bool(value)); }

  virtual Properties& assign(const Properties& properties)
  {
    // The precondition is that this object is empty
    DBUG_ASSERT(empty());
    // Deep copy the m_map.
    *m_map= *(properties.impl()->m_map);
    return *this;
  }

private:
  static const String_type EMPTY_STR;

private:
  std::unique_ptr<Properties::Map> m_map;
};

///////////////////////////////////////////////////////////////////////////

}

#endif // DD__PROPERTIES_IMPL_INCLUDED
