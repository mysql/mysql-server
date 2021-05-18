/*
 Copyright (c) 2013, 2016, Oracle and/or its affiliates. All rights reserved.
 
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

/* Compatibility wrappers for changing NDBAPI 
*/

#include <ndb_version.h>

// Pre-7.0 
#if NDB_VERSION_MAJOR < 7
#error "Node.JS Adapter requires MySQL Cluster 7.1" 
#endif 

// 7.0 
#if (NDB_VERSION_MAJOR == 7 && NDB_VERSION_MINOR == 0)
#error "Node.JS Adapter requires MySQL Cluster 7.1" 
#endif 

// 7.1
// Multiwait enabled; old API
#if (NDB_VERSION_MAJOR == 7 && NDB_VERSION_MINOR == 1)

#define MULTIWAIT_ENABLED 1
#define USE_OLD_MULTIWAIT_API 1

// 7.2
// Multiwait enabled; old API prior to 7.2.14
#elif (NDB_VERSION_MAJOR == 7) && (NDB_VERSION_MINOR == 2)
#define MULTIWAIT_ENABLED 1

#if NDB_VERSION_BUILD < 14
#define USE_OLD_MULTIWAIT_API 1
#endif

// 7.3 prior to 7.3.3
// Multiwait disabled; USE_OLD_MULTIWAIT_API defined to prevent compiler error
#elif (NDB_VERSION_MAJOR == 7) && (NDB_VERSION_MINOR == 3) && (NDB_VERSION_BUILD < 3) 

#define MULTIWAIT_ENABLED 0
#define USE_OLD_MULTIWAIT_API 1

// All other versions
// Multiwait enabled
#else
#define MULTIWAIT_ENABLED 1
#endif
