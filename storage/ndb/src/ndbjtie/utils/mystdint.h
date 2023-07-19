/*
 Copyright (c) 2010, 2023, Oracle and/or its affiliates.

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
 * mystdint.h
 */

#ifndef mystdint_h
#define mystdint_h

/*
 * Definition of C99's exact-width integral types for JTie.
 *
 * JTie has pre-defined type mappings for the C99 exact-width type aliases:
 * int8_t, uint8_t, ... int64_t, uint64_t, which are a more natural fit for
 * Java than the native, integral C types, char ... long long, 
 *
 * Unfortunately, the C99 <stdint.h> file is not provided by some C/C++
 * compilers.  (It's a crying shame.  For instance, MS Visual Studio 
 * provides <stdint.h> not until VS2010.)  Therefore, this header deals in a
 * single place with the presence or absence of the <stdint.h> file.
 *
 * While JTie applications (like NDB JTie) may have their own type aliases
 * for exact-width types, it is preferable not use these as the basis for
 * JTie's implementation and tests itself -- for platform testing has proven
 * much easier with a self-contained, standalone-compilable and -testable
 * JTie unit tests, where problematic patterns can be readily identified.
 * Hence, applications with their own, non-stdint-based exact-width type
 * definitions should add and use corresponding JTie type mapping aliases.
 */

#include <ndb_global.h>
#include "my_config.h"

#ifdef HAVE_STDINT_H

#include <stdint.h> // not using namespaces yet

#else

// this covers ILP32 and LP64 programming models
#ifndef __SunOS_5_9
typedef signed char int8_t;
typedef signed long int32_t;
typedef unsigned long uint32_t;
typedef signed long long int64_t;
typedef unsigned long long uint64_t;
#endif
typedef unsigned char uint8_t;
typedef signed short int16_t;
typedef unsigned short uint16_t;

#endif // !HAVE_STDINT_H

#endif // mystdint_h

