/* Copyright (c) 2014, 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#include "dd/impl/tables/version.h"

#include "dd/impl/dictionary_impl.h"
#include "dd/impl/transaction_impl.h"
#include "dd/impl/raw/raw_table.h"
#include "field.h"

namespace dd {
namespace tables {

///////////////////////////////////////////////////////////////////////////

uint Version::get_actual_dd_version(THD *thd) const
{
  // Start a DD transaction to get the version number.
  // Please note that we must do this read using isolation
  // level ISO_READ_UNCOMMITTED because the SE undo logs may not yet
  // be available.
  Transaction_ro trx(thd, ISO_READ_UNCOMMITTED);
  uint version= 0;

  trx.otx.add_table<Version>();

  if (!trx.otx.open_tables())
  {
    // This code accesses the handler interface directly. It could be
    // generalized and added to the raw module, but if this is the only
    // use case, it may as well be kept here.
    Raw_table *raw_t= trx.otx.get_table(table_name());
    DBUG_ASSERT(raw_t);
    TABLE *t= raw_t->get_table();
    DBUG_ASSERT(t);
    t->use_all_columns();
    if (!t->file->ha_rnd_init(true) &&
        !t->file->ha_rnd_next(t->record[0]))
      version= t->field[FIELD_VERSION]->val_int();

    t->file->ha_rnd_end();
  }
  return version;
}

///////////////////////////////////////////////////////////////////////////

}
}
