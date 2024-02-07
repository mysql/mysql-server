/*****************************************************************************

Copyright (c) 1996, 2024, Oracle and/or its affiliates.
Copyright (c) 2012, Facebook Inc.

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

/** @file dict/mem.h
 Data dictionary memory object creation

 Created 1/8/1996 Heikki Tuuri
 *******************************************************/

#ifndef dict_mem_h
#define dict_mem_h
/** Creates a table memory object.
@param[in]  name        table name
@param[in]  space       space where the clustered index of the table is placed
@param[in]  n_cols      total number of columns including virtual and
                        non-virtual columns
@param[in]  n_v_cols    number of virtual columns
@param[in]  n_m_v_cols  number of multi-value virtual columns
@param[in]  flags       table flags
@param[in]  flags2      table flags2
@param[in]  n_drop_cols Number of INSTANT drop cols
@return own: table object */
dict_table_t *dict_mem_table_create(const char *name, space_id_t space,
                                    ulint n_cols, ulint n_v_cols,
                                    ulint n_m_v_cols, uint32_t flags,
                                    uint32_t flags2, uint32_t n_drop_cols = 0);

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

/** Adds a column definition to a table.
@param[in] table        table
@param[in] heap         temporary memory heap, or NULL
@param[in] name         column name, or NULL
@param[in] mtype        main datatype
@param[in] prtype       precise type
@param[in] len          length of column
@param[in] is_visible   true if column is visible
@param[in] phy_pos      position of col in physical record
@param[in] v_added      table row version when col was added INSTANTly
@param[in] v_dropped    table_row version when col was dropped INSTANTly */
void dict_mem_table_add_col(dict_table_t *table, mem_heap_t *heap,
                            const char *name, ulint mtype, ulint prtype,
                            ulint len, bool is_visible,
                            uint32_t phy_pos = UINT32_UNDEFINED,
                            uint8_t v_added = UINT8_UNDEFINED,
                            uint8_t v_dropped = UINT8_UNDEFINED);

/** This function populates a dict_col_t memory structure with
supplied information.
@param[out] column      column struct to be filled
@param[in]  col_pos     column position
@param[in]  mtype       main data type
@param[in]  prtype      precise type
@param[in]  col_len     column length
@param[in]  is_visible  true if column is visible
@param[in]  phy_pos     position of col in physical record
@param[in]  v_added     table row version when col was added INSTANTly
@param[in]  v_dropped   table row version when col was dropped INSTANTly */
void dict_mem_fill_column_struct(dict_col_t *column, ulint col_pos, ulint mtype,
                                 ulint prtype, ulint col_len, bool is_visible,
                                 uint32_t phy_pos, uint8_t v_added,
                                 uint8_t v_dropped);

/** Append 'name' to 'col_names'.  @see dict_table_t::col_names
 @return new column names array */
const char *dict_add_col_name(const char *col_names, /*!< in: existing column
                                                     names, or NULL */
                              ulint cols, /*!< in: number of existing columns */
                              const char *name,  /*!< in: new column name */
                              mem_heap_t *heap); /*!< in: heap */
#endif
