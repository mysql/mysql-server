/*
 Copyright (c) 2012, Oracle and/or its affiliates. All rights reserved.

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
