/*
 Copyright (c) 2011, 2024, Oracle and/or its affiliates.

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License, version 2.0,
 as published by the Free Software Foundation.

 This program is designed to work with certain software (including
 but not limited to OpenSSL) that is licensed under separate terms,
 as designated in a particular file or component or in included license
 documentation.  The authors of MySQL hereby grant you an additional
 permission to link the program and your derivative works with the
 separately licensed software that they have either included with
 the program or referenced in the documentation.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License, version 2.0, for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/
/*
 * debug_utils.hpp
 */

#ifndef dbug_utils_hpp
#define dbug_utils_hpp

#include "my_dbug.h"

/*
 * These macros are robuster versions of the ones in MySQL's DBUG package.
 *
 * As the DBUG macros/functions don't check arguments, the caller (or JVM!)
 * crashes in case, for instance, of NULL args.  Also, macros returning a
 * value (like DBUG_EXPLAIN) ought to do so even if NDEBUG was defined.
 */

#ifndef NDEBUG

#define MY_DBUG_PUSH(a1)     \
  do {                       \
    if ((a1)) DBUG_PUSH(a1); \
  } while (0)
#define MY_DBUG_POP() DBUG_POP()
#define MY_DBUG_SET(a1)     \
  do {                      \
    if ((a1)) DBUG_SET(a1); \
  } while (0)
#define MY_DBUG_EXPLAIN(buf, len) \
  ((!(buf) || (len) <= 0) ? 1 : DBUG_EXPLAIN(buf, len))
#define MY_DBUG_PRINT(keyword, arglist)          \
  do {                                           \
    if ((keyword)) DBUG_PRINT(keyword, arglist); \
  } while (0)

#else  // NDEBUG

#define MY_DBUG_PUSH(a1)
#define MY_DBUG_POP()
#define MY_DBUG_SET(a1)
#define MY_DBUG_EXPLAIN(buf, len) 1
#define MY_DBUG_PRINT(keyword, arglist)

#endif  // NDEBUG

/*
 * These DBUG functions provide suitable mapping targets for use from Java.
 */

/** Push the state of the DBUG package */
inline void dbugPush(const char *state [[maybe_unused]]) {
  MY_DBUG_PUSH(state);
}

/** Pop the state of the DBUG package */
inline void dbugPop() { MY_DBUG_POP(); }

/** Set the state of the DBUG package */
inline void dbugSet(const char *state [[maybe_unused]]) { MY_DBUG_SET(state); }

/** Return the state of the DBUG package */
inline const char *dbugExplain(char *buffer, int length [[maybe_unused]]) {
  if (!MY_DBUG_EXPLAIN(buffer, length)) {
    return buffer;
  }
  return nullptr;
}

/** Print a message */
inline void dbugPrint(const char *keyword [[maybe_unused]],
                      const char *message [[maybe_unused]]) {
  MY_DBUG_PRINT(keyword, ("%s", message));
}

#endif  // dbug_utils_hpp
