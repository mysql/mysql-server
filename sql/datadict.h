#ifndef DATADICT_INCLUDED
#define DATADICT_INCLUDED
/* Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "handler.h"

/*
  Data dictionary API.
*/

enum frm_type_enum
{
  FRMTYPE_ERROR= 0,
  FRMTYPE_TABLE,
  FRMTYPE_VIEW
};

frm_type_enum dd_frm_type(THD *thd, char *path, enum legacy_db_type *dbt);

bool dd_frm_storage_engine(THD *thd, const char *db, const char *table_name,
                           handlerton **table_type);
bool dd_check_storage_engine_flag(THD *thd,
                                  const char *db, const char *table_name,
                                  uint32 flag,
                                  bool *yes_no);
bool dd_recreate_table(THD *thd, const char *db, const char *table_name);

#endif // DATADICT_INCLUDED
