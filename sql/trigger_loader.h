/*
   Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.

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

#ifndef TRIGGER_LOADER_H_INCLUDED
#define TRIGGER_LOADER_H_INCLUDED

///////////////////////////////////////////////////////////////////////////

#include "sql_list.h"

struct TABLE;

class THD;
class Trigger;

class Trigger_loader
{
public:
  /************************************************************************
   * Functions to work with TRN files.
   ***********************************************************************/

  static LEX_STRING build_trn_path(char *trn_file_name_buffer,
                                   int trn_file_name_buffer_size,
                                   const char *db_name,
                                   const char *trigger_name);

  static bool check_trn_exists(const LEX_STRING &trn_path);

  static bool load_trn_file(THD *thd,
                            const LEX_STRING &trigger_name,
                            const LEX_STRING &trn_path,
                            LEX_STRING *tbl_name);

public:
  static bool trg_file_exists(const char *db_name,
                              const char *table_name);

  static bool load_triggers(THD *thd,
                            MEM_ROOT *mem_root,
                            const char *db_name,
                            const char *table_name,
                            List<Trigger> *triggers);

  static bool store_trigger(const LEX_STRING &db_name,
                            const LEX_STRING &table_name,
                            MEM_ROOT *mem_root,
                            Trigger *new_trigger,
                            List<Trigger> *triggers);

  static bool drop_trigger(const LEX_STRING &db_name,
                           const LEX_STRING &table_name,
                           const LEX_STRING &trigger_name,
                           MEM_ROOT *mem_root,
                           List<Trigger> *triggers,
                           bool *trigger_found);

  static bool drop_all_triggers(const char *db_name,
                                const char *table_name,
                                List<Trigger> *triggers);

  static bool rename_subject_table(MEM_ROOT *mem_root,
                                   List<Trigger> *triggers,
                                   const char *old_db_name,
                                   LEX_STRING *old_table_name,
                                   const char *new_db_name,
                                   LEX_STRING *new_table_name,
                                   bool upgrading50to51);

private:
  Trigger_loader()
  { }
};

///////////////////////////////////////////////////////////////////////////

#endif // TRIGGER_LOADER_H_INCLUDED
