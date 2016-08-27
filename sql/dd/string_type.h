/* Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.

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

#include "my_global.h"

#include "stateless_allocator.h"        // Stateless_allocator

#include <string>
#include <sstream>

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

typedef Char_string_template<String_type_allocator> String_type_alias;

// TODO: String_type is an alias for std::string until all references to
// std::string has been replaced with String_type
typedef std::string String_type;


typedef std::basic_stringstream<char, std::char_traits<char>,
                                String_type_allocator> Stringstream_type_alias;

// TODO: Stringstream_type is an alias for std::stringstream until all
// references to std::stringstream has been replaced with String_type
typedef std::stringstream Stringstream_type;
}

namespace std {

/**
  Specialize std::hash for dd::String_type (_alias) so that it can be
  used with unordered_ containers. Uses our standard murmur3_32
  implementation, which is a bit of a waste since size_t is 64 bit on
  most platforms.
*/
template <>
struct hash<dd::String_type_alias> // TODO: Switch to dd::String_type
{
  typedef dd::String_type_alias argument_type;
  typedef size_t result_type;

  size_t operator()(const dd::String_type_alias &s) const;
};
}
#endif /* DD__STRING_TYPE */
