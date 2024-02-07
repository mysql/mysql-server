/*****************************************************************************

Copyright (c) 2010, 2024, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/** @file include/dict0priv.h
 Data dictionary private functions

 Created  Fri 2 Jul 2010 13:30:38 EST - Sunny Bains
 *******************************************************/

#ifndef dict0priv_h
#define dict0priv_h

#include "univ.i"

/** Gets a table; loads it to the dictionary cache if necessary. A low-level
function. Note: Not to be called from outside dict0*c functions.
@param[in]   table_name   the table name
@param[in]   prev_table   previous table name. The current table load
                          is happening because of the load of the
                          previous table name.  This parameter is used
                          to check for cyclic calls.
@return table, NULL if not found */
inline dict_table_t *dict_table_get_low(
    const char *table_name,
    const std::string *prev_table = nullptr); /*!< in: table name */

/** Checks if a table is in the dictionary cache.
 @return table, NULL if not found */
static inline dict_table_t *dict_table_check_if_in_cache_low(
    const char *table_name); /*!< in: table name */

#include "dict0priv.ic"

#endif /* dict0priv.h */
