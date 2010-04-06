/*
 Copyright (C) 2009 Sun Microsystems, Inc.
 All rights reserved. Use is subject to license terms.

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
 * mystdint.h
 */

#ifndef mystdint_h
#define mystdint_h

/*
 * Definition of C99's exact-width integral types for NDB-independent code.
 *
 * JTie pre-defines type mappings for the C99 exact-width type aliases
 *   int8_t, uint8_t, int16_t, uint16_t, int32_t, uint32_t, int64_t, uint64_t
 * as defined in <stdint.h> as well as uses these types internally.
 *
 * Unfortunately, some C/C++ compiler still lack a stdint.h header file.
 * (For instance, MS Visual Studio until VS2010.)  We delegate to a helper
 * file that handles the absence of the <stdint.h>.
 *
 * Applications of JTie (like NDB JTie) may define their own type aliases
 * for exact-width types (like Uint8, Int8 etc), for which type mappings can
 * be easily provided with just a few (NDB) JTie (_trait alias) definitions.
 *
 * Here, we do NOT want to introduce artificial dependencies by referring to
 * application-specific aliases for JTie's generic implementation and tests:
 * compiler/platform testing has proven (!) much easier when the JTie core
 * code is self-contained and the JTie unit tests compile & run standalone.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#else
// MS Visual Studio provides <stdint.h> not until VS2010
// (VC7/VS2003=1310, VC8/VS2005=1400, VC9/VS2008=1500, VC10/VS2010=1600)
#  if defined(_MSC_VER) && (_MSC_VER >= 1600)
#    define HAVE_STDINT_H 1
#  endif // defined(_MSC_VER) && (_MSC_VER >= 1600)
#endif // !HAVE_STDINT_H


#ifdef HAVE_STDINT_H
#  include <stdint.h> // not using namespaces yet
#else
// C99 stdint type aliases for ILP32 and LP64 programming models
typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef signed short int16_t;
typedef unsigned short uint16_t;
typedef signed long int32_t;
typedef unsigned long uint32_t;
typedef signed long long int64_t;
typedef unsigned long long uint64_t;
#endif // !HAVE_STDINT_H

#endif // mystdint_h
