/*****************************************************************************

Copyright (c) 1996, 2024, Oracle and/or its affiliates.

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

/** @file include/dict0crea.h
 Database object creation

 Created 1/8/1996 Heikki Tuuri
 *******************************************************/

#ifndef dict0crea_h
#define dict0crea_h

#include "dict0dict.h"
#include "dict0types.h"
#include "fsp0space.h"
#include "mtr0mtr.h"
#include "que0types.h"
#include "row0types.h"
#include "sql/handler.h"
#include "univ.i"

/** Build a table definition without updating SYSTEM TABLES
@param[in,out]  table           dict table object
@param[in]      create_info     HA_CREATE_INFO object
@param[in,out]  trx             transaction instance
@return DB_SUCCESS or error code */
dberr_t dict_build_table_def(dict_table_t *table,
                             const HA_CREATE_INFO *create_info, trx_t *trx);

/** Builds a tablespace to store various objects.
@param[in,out]  trx             DD transaction
@param[in,out]  tablespace      Tablespace object describing what to build.
@return DB_SUCCESS or error code. */
dberr_t dict_build_tablespace(trx_t *trx, Tablespace *tablespace);

/** Builds a tablespace to contain a table, using file-per-table=1.
@param[in,out]  table           Table to build in its own tablespace.
@param[in]      create_info     HA_CREATE_INFO object
@param[in,out]  trx             Transaction
@return DB_SUCCESS or error code */
dberr_t dict_build_tablespace_for_table(dict_table_t *table,
                                        const HA_CREATE_INFO *create_info,
                                        trx_t *trx);

/** Assign a new table ID and put it into the table cache and the transaction.
@param[in,out]  table   Table that needs an ID */
void dict_table_assign_new_id(dict_table_t *table);

/** Builds an index definition but doesn't update sys_table. */
void dict_build_index_def(const dict_table_t *table, /*!< in: table */
                          dict_index_t *index,       /*!< in/out: index */
                          trx_t *trx); /*!< in/out: InnoDB transaction
                                       handle */
/** Creates an index tree for the index if it is not a member of a cluster.
 @param[in,out] index   InnoDB index object
 @param[in,out] trx     transaction
 @return        DB_SUCCESS or DB_OUT_OF_FILE_SPACE */
dberr_t dict_create_index_tree_in_mem(dict_index_t *index, trx_t *trx);

/** Drop an index tree belonging to a temporary table.
@param[in]      index           index in a temporary table
@param[in]      root_page_no    index root page number */
void dict_drop_temporary_table_index(const dict_index_t *index,
                                     page_no_t root_page_no);

/** Generate a foreign key constraint name when it was not named by the user.
A generated constraint has a name of the format dbname/tablename_ibfk_NUMBER,
where the numbers start from 1, and are given locally for this table, that is,
the number is not global, as it used to be before MySQL 4.0.18.
@param[in,out]  id_nr   number to use in id generation; incremented if used
@param[in]      name    table name
@param[in,out]  foreign foreign key */
static inline dberr_t dict_create_add_foreign_id(ulint *id_nr, const char *name,
                                                 dict_foreign_t *foreign);

/** Check if a foreign constraint is on columns served as base columns
of any stored column. This is to prevent creating SET NULL or CASCADE
constraint on such columns
@param[in]      local_fk_set    set of foreign key objects, to be added to
the dictionary tables
@param[in]      table           table to which the foreign key objects in
local_fk_set belong to
@return true if yes, otherwise, false */
bool dict_foreigns_has_s_base_col(const dict_foreign_set &local_fk_set,
                                  const dict_table_t *table);

/* Table create node structure */
struct tab_node_t {
  que_common_t common;   /*!< node type: QUE_NODE_TABLE_CREATE */
  dict_table_t *table;   /*!< table to create, built as a
                         memory data structure with
                         dict_mem_... functions */
  ins_node_t *tab_def;   /*!< child node which does the insert of
                         the table definition; the row to be
                         inserted is built by the parent node  */
  ins_node_t *col_def;   /*!< child node which does the inserts
                         of the column definitions; the row to
                         be inserted is built by the parent
                         node  */
  ins_node_t *v_col_def; /*!< child node which does the inserts
                         of the sys_virtual row definitions;
                         the row to be inserted is built by
                         the parent node  */
  /*----------------------*/
  /* Local storage for this graph node */
  ulint state;       /*!< node execution state */
  ulint col_no;      /*!< next column definition to insert */
  ulint base_col_no; /*!< next base column to insert */
  mem_heap_t *heap;  /*!< memory heap used as auxiliary
                     storage */
};

/** Create in-memory tablespace dictionary index & table
@param[in]      space           tablespace id
@param[in]      space_discarded true if space is discarded
@param[in]      in_flags        space flags to use when space_discarded is true
@param[in]      is_create       true when creating SDI index
@return in-memory index structure for tablespace dictionary or NULL */
dict_index_t *dict_sdi_create_idx_in_mem(space_id_t space, bool space_discarded,
                                         uint32_t in_flags, bool is_create);

/* Index create node struct */

struct ind_node_t {
  que_common_t common;   /*!< node type: QUE_NODE_INDEX_CREATE */
  dict_index_t *index;   /*!< index to create, built as a
                         memory data structure with
                         dict_mem_... functions */
  ins_node_t *ind_def;   /*!< child node which does the insert of
                         the index definition; the row to be
                         inserted is built by the parent node  */
  ins_node_t *field_def; /*!< child node which does the inserts
                         of the field definitions; the row to
                         be inserted is built by the parent
                         node  */
  /*----------------------*/
  /* Local storage for this graph node */
  ulint state;                   /*!< node execution state */
  ulint page_no;                 /* root page number of the index */
  dict_table_t *table;           /*!< table which owns the index */
  dtuple_t *ind_row;             /* index definition row built */
  ulint field_no;                /* next field definition to insert */
  mem_heap_t *heap;              /*!< memory heap used as auxiliary
                                 storage */
  const dict_add_v_col_t *add_v; /*!< new virtual columns that being
                                 added along with an add index call */
};

/** Compose a column number for a virtual column, stored in the "POS" field
of Sys_columns. The column number includes both its virtual column sequence
(the "nth" virtual column) and its actual column position in original table
@param[in]      v_pos           virtual column sequence
@param[in]      col_pos         column position in original table definition
@return composed column position number */
static inline ulint dict_create_v_col_pos(ulint v_pos, ulint col_pos);

/** Get the column number for a virtual column (the column position in
original table), stored in the "POS" field of Sys_columns
@param[in]      pos             virtual column position
@return column position in original table */
static inline ulint dict_get_v_col_mysql_pos(ulint pos);

/** Get a virtual column sequence (the "nth" virtual column) for a
virtual column, stord in the "POS" field of Sys_columns
@param[in]      pos             virtual column position
@return virtual column sequence */
static inline ulint dict_get_v_col_pos(ulint pos);

#include "dict0crea.ic"

#endif
