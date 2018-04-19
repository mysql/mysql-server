/*
   Copyright (c) 2014, Oracle and/or its affiliates. All rights reserved.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef ERROR_HANDLING_HPP
#define ERROR_HANDLING_HPP

template <typename T>
inline static void print_if_not_equal(T got,
                                      T expected,
                                      const char* msg,
                                      const char* file,
                                      int line)
{
  std::cout << "Got value " << got << " instead of expected value " << expected
            << " in " << file << ":" << line;
}

#define PRINT_IF_NOT_EQUAL(got, expected, msg) {                        \
    if (got != expected) {                                              \
      print_if_not_equal(got, expected, msg, __FILE__, __LINE__);       \
      exit(-1);                                                         \
    }                                                                   \
  }

#define PRINT_ERROR(code,msg)                                           \
  std::cout << "Error in " << __FILE__ << ", line: " << __LINE__        \
            << ", code: " << code                                       \
            << ", msg: " << msg << "." << std::endl

#define APIERROR(error) {                \
  PRINT_ERROR(error.code,error.message); \
  exit(-1); }

#endif 
