/*****************************************************************************

Copyright (c) 2005, 2024, Oracle and/or its affiliates.

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

/** @file include/handler0alter.h
 Smart ALTER TABLE
 *******************************************************/

#ifndef handler0alter_h
#define handler0alter_h

constexpr uint32_t ERROR_STR_LENGTH = 1024;

/** Adjust the persistent statistics after rebuilding ALTER TABLE.
Remove statistics for dropped indexes, add statistics for created indexes
and rename statistics for renamed indexes.
@param table InnoDB table that was rebuilt by ALTER TABLE
@param table_name Table name in MySQL
@param thd MySQL connection
*/
void alter_stats_rebuild(dict_table_t *table, const char *table_name, THD *thd);

/** Copies an InnoDB record to table->record[0].
@param[in,out] table Mysql table
@param[in] rec Record
@param[in] index Index
@param[in] offsets rec_get_offsets( rec, index, ...) */
void innobase_rec_to_mysql(struct TABLE *table, const rec_t *rec,
                           const dict_index_t *index, const ulint *offsets);

/** Copies an InnoDB index entry to table->record[0].
@param[in,out] table Mysql table
@param[in] index Innodb index
@param[in] fields Innodb index fields */
void innobase_fields_to_mysql(struct TABLE *table, const dict_index_t *index,
                              const dfield_t *fields);

/** Copies an InnoDB row to table->record[0].
@param[in,out] table Mysql table
@param[in] itab Innodb table
@param[in] row Innodb row */
void innobase_row_to_mysql(struct TABLE *table, const dict_table_t *itab,
                           const dtuple_t *row);

/** Resets table->record[0]. */
void innobase_rec_reset(struct TABLE *table); /*!< in/out: MySQL table */

#endif /* handler0alter_h */
