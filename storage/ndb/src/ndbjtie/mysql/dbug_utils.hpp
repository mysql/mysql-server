/*
 Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.

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
 * debug_utils.hpp
 */

#ifndef dbug_utils_h
#define dbug_utils_h

inline
void
dbugPush(const char* new_state) {
    DBUG_PUSH(new_state);
}

inline
void
dbugPop() {
    DBUG_POP();
}

inline
void
dbugSet(const char* new_state) {
    DBUG_SET(new_state);
}

inline
const char*
dbugExplain(char * buffer, int length)
  {
    int result = 1;
#if !defined(DBUG_OFF)
    result = DBUG_EXPLAIN(buffer, length);
#endif
    if (result) {
      return NULL;
    } else {
      return buffer;
    }
  }

#endif // dbug_utils_h
