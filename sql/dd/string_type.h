/* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#ifndef DD__STRING_TYPE
#define DD__STRING_TYPE


#include <stddef.h>
#include <sstream>
#include <string>
#include <system_error>

#include "sql/stateless_allocator.h"    // Stateless_allocator

namespace dd {
/**
  Functor class which allocates memory for String_type. Implementation
  uses my_malloc with key_memory_DD_String_type.
*/
struct String_type_alloc
{
  void *operator()(size_t s) const;
};

typedef Stateless_allocator<char, String_type_alloc> String_type_allocator;

/**
  Template alias for char-based std::basic_string.
*/
template <class A>
using Char_string_template= std::basic_string<char, std::char_traits<char>, A>;

typedef Char_string_template<String_type_allocator> String_type;


/**
  Template alias for char-based std::basic_stringstream.
 */
template <class A>
using Char_stringstream_template=
  std::basic_stringstream<char, std::char_traits<char>, A>;

/**
  Instantiation of std::basic_stringstream with the same allocator as
  String_type. This is needed since a std::basic_stringstream::str()
  returns a basic_string allocated with its own allocator. Note that
  this also means that it is diffcult to use a different PSI key for
  the stream memory as that would mean the return type of
  Stringstream_type::str() would be different and incompatible with
  String_type.

  To work around this would require the creation of a temporary
  String_type from the string returned from stringstream::str().
*/
typedef Char_stringstream_template<String_type_allocator>
Stringstream_type;

template <typename LEX_STRING_TYPE>
String_type make_string_type(const LEX_STRING_TYPE &lst)
{
  return { lst.str, lst.length };
}
} // namespace dd

namespace std {

/**
  Specialize std::hash for dd::String_type so that it can be
  used with unordered_ containers. Uses our standard murmur3_32
  implementation, and the same suitability restrictions apply.
  @see murmur3_32
*/
template <>
struct hash<dd::String_type>
{
  typedef dd::String_type argument_type;
  typedef size_t result_type;

  size_t operator()(const dd::String_type &s) const;
};
} // namespace std
#endif /* DD__STRING_TYPE */
