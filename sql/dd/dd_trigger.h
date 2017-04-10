/*
   Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

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
#ifndef DD_TRIGGER_INCLUDED
#define DD_TRIGGER_INCLUDED

#include "dd/string_type.h"  // dd::String_type
#include "lex_string.h"
#include "m_string.h"        // LEX_CSTRING
#include "trigger_def.h"     // enum_trigger_order_type

class THD;
class Trigger;
typedef struct st_mem_root MEM_ROOT;
template <class T> class List;

namespace dd
{

/**
  Create new trigger in the data dictionary.

  @param [in]  thd                thread handle
  @param [in]  new_trigger        Trigger object from sql-layer that describes
                                  a trigger to add in the Data Dictionary
  @param [in]  ordering_clause    Ordering property for trigger being created
  @param [in]  referenced_trigger_name  Information about trigger referenced by
                                        ordering_clause if any.


  @return Operation status
    @retval true   Failure
    @retval false  Success
*/

bool create_trigger(THD *thd, const ::Trigger *new_trigger,
                    enum_trigger_order_type ordering_clause,
                    const LEX_CSTRING &referenced_trigger_name);


/**
  Load table triggers from the data dictionary.

  @param [in]  thd                thread handle
  @param [in]  mem_root           MEM_ROOT for memory allocation
  @param [in]  schema_name        name of schema
  @param [in]  table_name         subject table name
  @param [out] triggers           pointer to the list where new Trigger
                                  objects will be inserted

  @return Operation status
    @retval true   Failure
    @retval false  Success
*/

bool load_triggers(THD *thd,
                   MEM_ROOT *mem_root,
                   const char *schema_name,
                   const char *table_name,
                   List<::Trigger> *triggers);


/**
  Load table trigger names from the data dictionary.

  @param [in]  thd                thread handle
  @param [in]  mem_root           MEM_ROOT for memory allocation
  @param [in]  schema_name        name of schema
  @param [in]  table_name         subject table name
  @param [out] trigger_names      pointer to the list where trigger names
                                  associated with a table will be inserted

  @return Operation status
    @retval true   Failure
    @retval false  Success
*/

bool load_trigger_names(THD *thd,
                        MEM_ROOT *mem_root,
                        const char *schema_name,
                        const char *table_name,
                        List<LEX_CSTRING> *trigger_names);


/**
  Check in the data dictionary if there is any trigger associated with a table.

  @param [in]  thd                thread handle
  @param [in]  schema_name        name of schema
  @param [in]  table_name         subject table name
  @param [out] table_has_trigger  true in case there is trigger(s) for a table,
                                  else false

  @return Operation status
    @retval true   Failure
    @retval false  Success

*/

bool table_has_triggers(THD *thd, const char *schema_name,
                        const char *table_name, bool *table_has_trigger);


/**
  Check if a trigger exists with specified name in specified schema.

  @param [in]  thd             thread handle
  @param [in]  schema_name     name of schema where to check trigger for
                               existence.
  @param [in]  trigger_name    name of trigger to check for existence.
  @param[out]  trigger_exists  has value true if there is a trigger
                               specified by name, else has value false.

  @return Operation status
    @retval true   Failure
    @retval false  Success
*/

bool check_trigger_exists(THD *thd,
                          const char *schema_name,
                          const char *trigger_name,
                          bool *trigger_exists);


/**
  Drop a trigger from the data dictionary.

  @param [in]  thd            thread handle
  @param [in]  schema_name    name of schema where a table resides
  @param [in]  table_name     name of table for that trigger has to be dropped
  @param [in]  trigger_name   name of trigger to drop
  @param [out] trigger_found  true if trigger found, else false

  @return Operation status
    @retval true   Failure
    @retval false  Success
*/

bool drop_trigger(THD *thd, const char *schema_name,
                  const char *table_name,
                  const char *trigger_name,
                  bool *trigger_found);


/**
  Drop all triggers for a table from the data dictionary.

  @param [in]  thd          thread handle
  @param [in]  schema_name  name of schema where a table resides
  @param [in]  table_name   name of table for that triggers have to be dropped
  @param [in]  triggers     triggers to drop

  @return Operation status
    @retval true   Failure
    @retval false  Success
*/

bool drop_all_triggers(THD *thd,
                       const char *schema_name,
                       const char *table_name,
                       List<::Trigger>* triggers);


/**
  Get name of a table related with a specified trigger.

  @param [in]  thd            thread handle
  @param [in]  schema_name    name of schema where a table resides
  @param [in]  trigger_name   name of trigger
  @param [out] table_name     name of table for that the trigger is associated
  @param [out] trigger_found  true if trigger found, else false
  @param [in]  push_warning_if_not_exist  flag to tell whether to issue an error
                                          in case schema with specified name
                                          not found

  @return Operation status
    @retval true   Failure
    @retval false  Success
*/

bool get_table_name_for_trigger(THD *thd,
                                const char *schema_name,
                                const char *trigger_name,
                                dd::String_type *table_name,
                                bool *trigger_found,
                                bool push_warning_if_not_exist);

}

#endif /* DD_TRIGGER_INCLUDED */
