/*****************************************************************************

Copyright (c) 1996, 2018, Oracle and/or its affiliates. All Rights Reserved.
Copyright (c) 2012, Facebook Inc.

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

/** @file dict/mem.h
 Data dictionary memory object creation

 Created 1/8/1996 Heikki Tuuri
 *******************************************************/

#ifndef dict_mem_h
#define dict_mem_h
/** Creates a table memory object.
 @return own: table object */
dict_table_t *dict_mem_table_create(
    const char *name, /*!< in: table name */
    space_id_t space, /*!< in: space where the clustered index
                      of the table is placed */
    ulint n_cols,     /*!< in: total number of columns
                      including virtual and non-virtual
                      columns */
    ulint n_v_cols,   /*!< in: number of virtual columns */
    ulint flags,      /*!< in: table flags */
    ulint flags2);    /*!< in: table flags2 */
/** Free a table memory object. */
void dict_mem_table_free(dict_table_t *table); /*!< in: table */

/** Creates an index memory object.
 @return own: index object */
dict_index_t *dict_mem_index_create(
    const char *table_name, /*!< in: table name */
    const char *index_name, /*!< in: index name */
    ulint space,            /*!< in: space where the index tree is
                            placed, ignored if the index is of
                            the clustered type */
    ulint type,             /*!< in: DICT_UNIQUE,
                            DICT_CLUSTERED, ... ORed */
    ulint n_fields);        /*!< in: number of fields */

/** Adds a column definition to a table. */
void dict_mem_table_add_col(
    dict_table_t *table, /*!< in: table */
    mem_heap_t *heap,    /*!< in: temporary memory heap, or NULL */
    const char *name,    /*!< in: column name, or NULL */
    ulint mtype,         /*!< in: main datatype */
    ulint prtype,        /*!< in: precise type */
    ulint len);          /*!< in: precision */

/** This function populates a dict_col_t memory structure with
 supplied information. */
void dict_mem_fill_column_struct(
    dict_col_t *column, /*!< out: column struct to be
                        filled */
    ulint col_pos,      /*!< in: column position */
    ulint mtype,        /*!< in: main data type */
    ulint prtype,       /*!< in: precise type */
    ulint col_len);     /*!< in: column length */
/** Append 'name' to 'col_names'.  @see dict_table_t::col_names
 @return new column names array */
const char *dict_add_col_name(
    const char *col_names, /*!< in: existing column names, or
                           NULL */
    ulint cols,            /*!< in: number of existing columns */
    const char *name,      /*!< in: new column name */
    mem_heap_t *heap);     /*!< in: heap */
#endif
