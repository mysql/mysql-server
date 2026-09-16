// Copyright (c) 2026, Oracle and/or its affiliates.
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

#ifndef MYSQL_CONCURRENCY_CACHE_LINE_SIZE
#define MYSQL_CONCURRENCY_CACHE_LINE_SIZE

#include <new>  // std::hardware_destructive_interference_size

#include "my_compiler.h"  // MY_COMPILER_DIAGNOSTIC_*

namespace mysql::concurrency::detail {

MY_COMPILER_DIAGNOSTIC_PUSH()
#if defined(__GNUC__) && !defined(__clang__) && (__GNUC__ >= 12)
MY_COMPILER_GCC_DIAGNOSTIC_IGNORE("-Winterference-size")
#endif
#ifdef __cpp_lib_hardware_interference_size
constexpr inline std::size_t hardware_destructive_interference_size =
    std::hardware_destructive_interference_size;
#else
// Fallback for pre-C++17 libs; typical x86/ARM cache line size
constexpr inline std::size_t hardware_destructive_interference_size = 64;
#endif
MY_COMPILER_DIAGNOSTIC_POP()

}  // namespace mysql::concurrency::detail

#endif  // MYSQL_CONCURRENCY_CACHE_LINE_SIZE
