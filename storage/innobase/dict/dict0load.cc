/*****************************************************************************

Copyright (c) 1996, 2026, Oracle and/or its affiliates.

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

/** @file dict/dict0load.cc
 Loads to the memory cache database object definitions
 from dictionary tables

 Created 4/24/1996 Heikki Tuuri
 *******************************************************/

#include "current_thd.h"
#include "ha_prototypes.h"

#include <set>
#include <stack>
#include "dict0load.h"

#include "btr0btr.h"
#include "btr0pcur.h"
#include "dict0boot.h"
#include "dict0crea.h"
#include "dict0dd.h"
#include "dict0dict.h"
#include "dict0mem.h"
#include "dict0priv.h"
#include "dict0stats.h"
#include "fsp0file.h"
#include "fsp0sysspace.h"
#include "fts0priv.h"
#include "mach0data.h"

#include "my_dbug.h"

#include "fil0fil.h"
#include "fts0fts.h"
#include "mysql_version.h"
#include "page0page.h"
#include "rem0cmp.h"
#include "srv0srv.h"
#include "srv0start.h"
#include "ut0math.h"

/* If this flag is true, then we will load the cluster index's (and tables')
metadata even if it is marked as "corrupted". */
bool srv_load_corrupted = false;

/** Using the table->heap, copy the null-terminated filepath into
table->data_dir_path. The data directory path is derived from the
filepath by stripping the the table->name.m_name component suffix.
If the filepath is not of the correct form (".../db/table.ibd"),
then table->data_dir_path will remain nullptr.
@param[in,out]  table           table instance
@param[in]      filepath        filepath of tablespace */
void dict_save_data_dir_path(dict_table_t *table, char *filepath) {
  ut_ad(dict_sys_mutex_own());
  ut_ad(DICT_TF_HAS_DATA_DIR(table->flags));
  ut_ad(table->data_dir_path == nullptr);
  ut_a(Fil_path::has_suffix(IBD, filepath));

  /* Ensure this filepath is not the default filepath. */
  char *default_filepath = Fil_path::make("", table->name.m_name, IBD);

  if (default_filepath == nullptr) {
    /* Memory allocation problem. */
    return;
  }

  if (strcmp(filepath, default_filepath) != 0) {
    size_t pathlen = strlen(filepath);

    ut_a(pathlen < OS_FILE_MAX_PATH);
    ut_a(Fil_path::has_suffix(IBD, filepath));

    char *data_dir_path = mem_heap_strdup(table->heap, filepath);

    Fil_path::make_data_dir_path(data_dir_path);

    if (strlen(data_dir_path)) {
      table->data_dir_path = data_dir_path;
    }
  }

  ut::free(default_filepath);
}

void dict_get_and_save_space_name(dict_table_t *table) {
  /* Do this only for general tablespaces. */
  if (!DICT_TF_HAS_SHARED_SPACE(table->flags)) {
    return;
  }

  if (table->tablespace != nullptr) {
    return;
  }

  fil_space_t *space = fil_space_acquire_silent(table->space);

  if (space != nullptr) {
    /* Use this name unless it is a temporary general
    tablespace name and we can now replace it. */
    if (!srv_sys_tablespaces_open ||
        !dict_table_has_temp_general_tablespace_name(space->name)) {
      /* Use this tablespace name */
      table->tablespace = mem_heap_strdup(table->heap, space->name);

      fil_space_release(space);
      return;
    }
    fil_space_release(space);
  }
}