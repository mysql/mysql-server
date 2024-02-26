/*
 Copyright (c) 2012, 2023, Oracle and/or its affiliates.

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
/*
 * ndbjtie_unit_tests_consts.hpp
 */

#include "jtie_stdint.h"

#ifndef ndbjtie_unit_tests_consts_hpp
#define ndbjtie_unit_tests_consts_hpp

// magic value if a constant name is unknown
const int64_t UNKNOWN_CONSTANT = 0x0abcdef00fedcba0LL;

// Returns the integral value of a constant passed by its qualified JVM name,
// or UNKNOWN_CONSTANT if the passed name is unknown to this function.
extern int64_t nativeConstValue(const char * p0);

#endif // ndbjtie_unit_tests_consts_hpp
