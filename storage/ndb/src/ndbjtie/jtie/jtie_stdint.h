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
 * jtie_stdint.h
 */

#ifndef jtie_stdint_h
#define jtie_stdint_h

/*
 * JTie pre-defines type mappings for the C99 exact-width type aliases
 * int8_t, uint8_t, ... int64_t, uint64_t as defined in <stdint.h> as
 * well as uses these types internally.
 *
 * Unfortunately, some C/C++ compiler still lack a stdint.h header file.
 * (For instance, MS Visual Studio until VS2010.)  We delegate to a helper
 * file that handles the absence of the <stdint.h>.
 *
 * While JTie applications (like NDB JTie) may have their own type aliases
 * for exact-width types, it is preferrable not use these as the basis for
 * JTie's implementation and tests itself -- for platform testing has proven
 * much easier with self-contained, standalone-compilable and -testable
 * JTie unit tests, where problematic patterns can be readily identified.
 * Hence, applications with their own, non-stdint-based exact-width type
 * definitions should add and use corresponding JTie type mapping aliases.
 */
#include "mystdint.h"

#endif // jtie_stdint_h
