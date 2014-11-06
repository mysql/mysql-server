/*
   Copyright (c) 2014 Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

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
