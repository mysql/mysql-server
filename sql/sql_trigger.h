#ifndef SQL_TRIGGER_INCLUDED
#define SQL_TRIGGER_INCLUDED

/*
   Copyright (c) 2004, 2013, Oracle and/or its affiliates. All rights reserved.

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

///////////////////////////////////////////////////////////////////////////

#include "m_string.h"

class sp_name;
class THD;

struct TABLE_LIST;

///////////////////////////////////////////////////////////////////////////

extern const char * const TRG_EXT;
extern const char * const TRN_EXT;

///////////////////////////////////////////////////////////////////////////

bool mysql_create_or_drop_trigger(THD *thd, TABLE_LIST *tables, bool create);

bool add_table_for_trigger(THD *thd,
                           const sp_name *trg_name,
                           bool continue_if_not_exist,
                           TABLE_LIST **table);

void build_trn_path(THD *thd, const sp_name *trg_name, LEX_STRING *trn_path);

bool check_trn_exists(const LEX_STRING *trn_path);

bool load_table_name_for_trigger(THD *thd,
                                 const sp_name *trg_name,
                                 const LEX_STRING *trn_path,
                                 LEX_STRING *tbl_name);

///////////////////////////////////////////////////////////////////////////

#endif /* SQL_TRIGGER_INCLUDED */
