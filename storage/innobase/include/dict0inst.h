/*****************************************************************************
Copyright (c) 2018, 2024, Oracle and/or its affiliates.

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

/** @file dict0inst.h
Instant DDL interface

Created 2020-04-24 by Mayank Prasad. */

#ifndef dict0inst_h
#define dict0inst_h

#include "dict0dd.h"
#include "ha_innopart.h"
#include "handler0alter.h"

/** Flags indicating if current operation can be done instantly */
enum class Instant_Type : uint16_t {
  /** Impossible to alter instantly */
  INSTANT_IMPOSSIBLE,

  /** Can be instant without any change */
  INSTANT_NO_CHANGE,

  /** Adding or dropping virtual columns only */
  INSTANT_VIRTUAL_ONLY,

  /** ADD/DROP COLUMN which can be done instantly, including adding/dropping
  stored column only (or along with adding/dropping virtual columns) */
  INSTANT_ADD_DROP_COLUMN,

  /** Column rename */
  INSTANT_COLUMN_RENAME
};

using Columns = std::vector<Field *>;

template <typename Table>
class Instant_ddl_impl {
 public:
  Instant_ddl_impl() = delete;

  /** Constructor
  @param[in]        alter_info    inplace alter information
  @param[in]        thd           user thread
  @param[in]        trx           transaction
  @param[in,out]    dict_table    innodb dictionary cache object
  @param[in]        old_table     old global DD cache object
  @param[in,out]    altered_table new MySQL table object
  @param[in]        old_dd_tab    old global DD cache object
  @param[in,out]    new_dd_tab    new global DD cache object
  @param[in]        autoinc       auto increment */
  Instant_ddl_impl(Alter_inplace_info *alter_info, THD *thd, trx_t *trx,
                   dict_table_t *dict_table, const TABLE *old_table,
                   const TABLE *altered_table, const Table *old_dd_tab,
                   Table *new_dd_tab, uint64_t *autoinc)
      : m_ha_alter_info(alter_info),
        m_thd(thd),
        m_trx(trx),
        m_dict_table(dict_table),
        m_old_table(old_table),
        m_altered_table(altered_table),
        m_old_dd_tab(old_dd_tab),
        m_new_dd_tab(new_dd_tab),
        m_autoinc(autoinc) {}

  /** Destructor */
  ~Instant_ddl_impl() {}

  /** Commit instant DDL
  @retval true Failure
  @retval false Success */
  bool commit_instant_ddl();

  /** Check if INSTANT ADD/DROP can be done.
  @param[in]  ha_alter_info alter info
  @param[in]  table         MySQL table before ALTER
  @param[in]  altered_table MySQL table after ALTER
  @param[in]  dict_table    InnoDB table definition cache
  @return true if INSTANT ADD/DROP can be done, false otherwise. */
  static bool is_instant_add_drop_possible(
      const Alter_inplace_info *ha_alter_info, const TABLE *table,
      const TABLE *altered_table, const dict_table_t *dict_table);

 private:
  /** Add column instantly
  @retval true Failure
  @retval false Success */
  bool commit_instant_add_col_low();

  /** Add column instantly
  @retval true Failure
  @retval false Success */
  bool commit_instant_drop_col_low();

  /** Add columns instantly
  @retval true Failure
  @retval false Success */
  bool commit_instant_add_col();

  /** Drop columns instantly
  @retval true Failure
  @retval false Success */
  bool commit_instant_drop_col();

  /** Fetch columns which are to be added or dropped instantly */
  void populate_to_be_instant_columns();

  /** Update metadata in commit phase when the alter table does
  no change to the table
  @param[in]      ignore_fts      ignore FTS update if true */
  void dd_commit_inplace_no_change(bool ignore_fts);

 private:
  /* Columns which are to be added instantly */
  Columns m_cols_to_add;

  /* Columns which are to be dropped instantly */
  Columns m_cols_to_drop;

  /* Inpalce alter info */
  Alter_inplace_info *m_ha_alter_info;

  /* User thread */
  THD *m_thd;

  /* Transaction */
  trx_t *m_trx;

  /* InnoDB dictionary table object */
  dict_table_t *m_dict_table;

  /* MySQL table as it is before the ALTER operation */
  const TABLE *m_old_table;

  /* MySQL table that is being altered */
  const TABLE *m_altered_table;

  /* Old Table/Partition definition */
  const Table *m_old_dd_tab;

  /* New Table/Partition definition */
  Table *m_new_dd_tab;

  uint64_t *m_autoinc;
};

#endif /* dict0inst_h */
