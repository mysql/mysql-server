/*
 Copyright (c) 2013, Oracle and/or its affiliates. All rights
 reserved.
 
 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; version 2 of
 the License.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 02110-1301  USA
 */

/* Compatibility wrappers for changing NDBAPI 
*/

#include <ndb_version.h>

// 7.1 
#if (NDB_VERSION_MAJOR == 7 && NDB_VERSION_MINOR == 1)

#define MULTIWAIT_ENABLED 1
#define USE_OLD_MULTIWAIT_API 1

// 7.2
#elif (NDB_VERSION_MAJOR == 7) && (NDB_VERSION_MINOR == 2)
#define MULTIWAIT_ENABLED 1

#if NDB_VERSION_BUILD < 14
#define USE_OLD_MULTIWAIT_API 1
#endif

// 7.3
#elif (NDB_VERSION_MAJOR == 7) && (NDB_VERSION_MINOR == 3)

#if NDB_VERSION_BUILD < 3
#define USE_OLD_MULTIWAIT_API 1
#define MULTIWAIT_ENABLED 0
#else
#define MULTIWAIT_ENABLED 1
#endif

#else
#error "What NDB Version?"

#endif

