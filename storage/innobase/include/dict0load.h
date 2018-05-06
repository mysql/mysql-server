/*****************************************************************************

Copyright (c) 1996, 2018, Oracle and/or its affiliates. All Rights Reserved.

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

/** @file include/dict0load.h
 Loads to the memory cache database object definitions
 from dictionary tables

 Created 4/24/1996 Heikki Tuuri
 *******************************************************/

#ifndef dict0load_h
#define dict0load_h

#include "btr0types.h"
#include "dict0types.h"
#include "fil0fil.h"
#include "mem0mem.h"
#include "trx0types.h"
#include "univ.i"
#include "ut0byte.h"
#include "ut0new.h"

#include <deque>

/** A stack of table names related through foreign key constraints */
typedef std::deque<const char *, ut_allocator<const char *>> dict_names_t;

/** enum that defines all system table IDs. @see SYSTEM_TABLE_NAME[] */
enum dict_system_id_t {
  SYS_TABLES = 0,
  SYS_INDEXES,
  SYS_COLUMNS,
  SYS_FIELDS,
  SYS_FOREIGN,
  SYS_FOREIGN_COLS,
  SYS_TABLESPACES,
  SYS_DATAFILES,
  SYS_VIRTUAL,

  /* This must be last item. Defines the number of system tables. */
  SYS_NUM_SYSTEM_TABLES
};

/** Status bit for dict_process_sys_tables_rec_and_mtr_commit() */
enum dict_table_info_t {
  DICT_TABLE_LOAD_FROM_RECORD = 0, /*!< Directly populate a dict_table_t
                                   structure with information from
                                   a SYS_TABLES record */
  DICT_TABLE_LOAD_FROM_CACHE = 1   /*!< Check first whether dict_table_t
                                   is in the cache, if so, return it */
};

extern const char *SYSTEM_TABLE_NAME[];

/** Finds the first table name in the given database.
 @return own: table name, NULL if does not exist; the caller must free
 the memory in the string! */
char *dict_get_first_table_name_in_db(
    const char *name); /*!< in: database name which ends to '/' */

/** Get the first filepath from SYS_DATAFILES for a given space_id.
@param[in]	space_id	Tablespace ID
@return First filepath (caller must invoke ut_free() on it)
@retval NULL if no SYS_DATAFILES entry was found. */
char *dict_get_first_path(ulint space_id);

/** Make sure the data_file_name is saved in dict_table_t if needed.
Try to read it from the fil_system first, then from SYS_DATAFILES.
@param[in]	table		Table object
@param[in]	dict_mutex_own	true if dict_sys->mutex is owned already */
void dict_get_and_save_data_dir_path(dict_table_t *table, bool dict_mutex_own);

/** Make sure the tablespace name is saved in dict_table_t if needed.
Try to read it from the file dictionary first, then from SYS_TABLESPACES.
@param[in]	table		Table object
@param[in]	dict_mutex_own	true if dict_sys->mutex is owned already */
void dict_get_and_save_space_name(dict_table_t *table, bool dict_mutex_own);

/** Loads a table definition and also all its index definitions, and also
the cluster definition if the table is a member in a cluster. Also loads
all foreign key constraints where the foreign key is in the table or where
a foreign key references columns in this table.
@param[in]	name		Table name in the dbname/tablename format
@param[in]	cached		true=add to cache, false=do not
@param[in]	ignore_err	Error to be ignored when loading
                                table and its index definition
@return table, NULL if does not exist; if the table is stored in an
.ibd file, but the file does not exist, then we set the ibd_file_missing
flag in the table object we return. */
dict_table_t *dict_load_table(const char *name, bool cached,
                              dict_err_ignore_t ignore_err);

/** Loads a table object based on the table id.
 @return table; NULL if table does not exist */
dict_table_t *dict_load_table_on_id(
    table_id_t table_id,           /*!< in: table id */
    dict_err_ignore_t ignore_err); /*!< in: errors to ignore
                                   when loading the table */
/** This function is called when the database is booted.
 Loads system table index definitions except for the clustered index which
 is added to the dictionary cache at booting before calling this function. */
void dict_load_sys_table(dict_table_t *table); /*!< in: system table */
/** Loads foreign key constraints where the table is either the foreign key
 holder or where the table is referenced by a foreign key. Adds these
 constraints to the data dictionary.

 The foreign key constraint is loaded only if the referenced table is also
 in the dictionary cache.  If the referenced table is not in dictionary
 cache, then it is added to the output parameter (fk_tables).

 @return DB_SUCCESS or error code */
dberr_t dict_load_foreigns(
    const char *table_name,       /*!< in: table name */
    const char **col_names,       /*!< in: column names, or NULL
                                  to use table->col_names */
    bool check_recursive,         /*!< in: Whether to check
                                  recursive load of tables
                                  chained by FK */
    bool check_charsets,          /*!< in: whether to check
                                  charset compatibility */
    dict_err_ignore_t ignore_err, /*!< in: error to be ignored */
    dict_names_t &fk_tables)      /*!< out: stack of table names
                                  which must be loaded
                                  subsequently to load all the
                                  foreign key constraints. */
    MY_ATTRIBUTE((warn_unused_result));

/** This function opens a system table, and return the first record.
 @return first record of the system table */
const rec_t *dict_startscan_system(
    btr_pcur_t *pcur,            /*!< out: persistent cursor to
                                 the record */
    mtr_t *mtr,                  /*!< in: the mini-transaction */
    dict_system_id_t system_id); /*!< in: which system table to open */
/** This function get the next system table record as we scan the table.
 @return the record if found, NULL if end of scan. */
const rec_t *dict_getnext_system(
    btr_pcur_t *pcur, /*!< in/out: persistent cursor
                      to the record */
    mtr_t *mtr);      /*!< in: the mini-transaction */

/** This function parses a SYS_TABLESPACES record, extracts necessary
 information from the record and returns to caller.
 @return error message, or NULL on success */
const char *dict_process_sys_tablespaces(
    mem_heap_t *heap,  /*!< in/out: heap memory */
    const rec_t *rec,  /*!< in: current SYS_TABLESPACES rec */
    space_id_t *space, /*!< out: space id */
    const char **name, /*!< out: tablespace name */
    ulint *flags);     /*!< out: tablespace flags */
/** Opens a tablespace for dict_load_table_one()
@param[in,out]	table		A table that refers to the tablespace to open
@param[in,out]	heap		A memory heap
@param[in]	ignore_err	Whether to ignore an error. */
void dict_load_tablespace(dict_table_t *table, mem_heap_t *heap,
                          dict_err_ignore_t ignore_err);

/** Using the table->heap, copy the null-terminated filepath into
table->data_dir_path. The data directory patch is derived form the
filepath by stripping the the table->name.m_name component suffix.
@param[in,out]	table		table obj
@param[in]	filepath	filepath of tablespace */
void dict_save_data_dir_path(dict_table_t *table, char *filepath);

/** Load all tablespaces during upgrade */
void dict_load_tablespaces_for_upgrade();

/* Comparator for missing_spaces. */
struct space_compare {
  bool operator()(const fil_space_t *lhs, const fil_space_t *rhs) const {
    return (lhs->id < rhs->id);
  }
};

/* This is set of tablespaces that are not found in SYS_TABLESPACES.
InnoDB tablespaces before 5.6 are not registered in SYS_TABLESPACES.
So we maintain a std::set, which is later used to register the
tablespaces to dictionary table mysql.tablespaces */
using missing_sys_tblsp_t = std::set<fil_space_t *, space_compare>;
extern missing_sys_tblsp_t missing_spaces;

#include "dict0load.ic"

#endif
