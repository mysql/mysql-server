/*****************************************************************************

Copyright (c) 2012, 2023, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/** @file include/row0import.h
 Header file for import tablespace functions.

 Created 2012-02-08 by Sunny Bains
 *******************************************************/

#ifndef row0import_h
#define row0import_h

#include "dict0types.h"
#include "univ.i"

// Forward declarations
struct trx_t;
struct dict_table_t;
struct row_prebuilt_t;

/** Imports a tablespace. The space id in the .ibd file must match the space id
of the table in the data dictionary.
@param[in]      table           table
@param[in]      table_def       dd table
@param[in]      prebuilt        prebuilt struct in MySQL
@return error code or DB_SUCCESS */
[[nodiscard]] dberr_t row_import_for_mysql(dict_table_t *table,
                                           dd::Table *table_def,
                                           row_prebuilt_t *prebuilt);

#endif /* row0import_h */
