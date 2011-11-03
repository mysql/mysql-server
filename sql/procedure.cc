/* Copyright (c) 2000, 2010, Oracle and/or its affiliates. All rights reserved.

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


/* Procedures (functions with changes output of select) */

#ifdef USE_PRAGMA_IMPLEMENTATION
#pragma implementation				// gcc: Class implementation
#endif

#include "sql_priv.h"
#include "procedure.h"
#include "sql_analyse.h"			// Includes procedure
#ifdef USE_PROC_RANGE
#include "proc_range.h"
#endif

static struct st_procedure_def {
  const char *name;
  Procedure *(*init)(THD *thd,ORDER *param,select_result *result,
		     List<Item> &field_list);
} sql_procs[] = {
#ifdef USE_PROC_RANGE
  { "split_sum",proc_sum_range_init },		// Internal procedure at TCX
  { "split_count",proc_count_range_init },	// Internal procedure at TCX
  { "matris_ranges",proc_matris_range_init },	// Internal procedure at TCX
#endif
  { "analyse",proc_analyse_init }		// Analyse a result
};


my_decimal *Item_proc_string::val_decimal(my_decimal *decimal_value)
{
  if (null_value)
    return 0;
  string2my_decimal(E_DEC_FATAL_ERROR, &str_value, decimal_value);
  return (decimal_value);
}


my_decimal *Item_proc_int::val_decimal(my_decimal *decimal_value)
{
  if (null_value)
    return 0;
  int2my_decimal(E_DEC_FATAL_ERROR, value, unsigned_flag, decimal_value);
  return (decimal_value);
}


my_decimal *Item_proc_real::val_decimal(my_decimal *decimal_value)
{
  if (null_value)
    return 0;
  double2my_decimal(E_DEC_FATAL_ERROR, value, decimal_value);
  return (decimal_value);
}


/**
  Setup handling of procedure.

  @return
    Return 0 if everything is ok
*/


Procedure *
setup_procedure(THD *thd,ORDER *param,select_result *result,
		List<Item> &field_list,int *error)
{
  uint i;
  DBUG_ENTER("setup_procedure");
  *error=0;
  if (!param)
    DBUG_RETURN(0);
  for (i=0 ; i < array_elements(sql_procs) ; i++)
  {
    if (!my_strcasecmp(system_charset_info,
                       (*param->item)->name,sql_procs[i].name))
    {
      Procedure *proc=(*sql_procs[i].init)(thd,param,result,field_list);
      *error= !proc;
      DBUG_RETURN(proc);
    }
  }
  my_error(ER_UNKNOWN_PROCEDURE, MYF(0), (*param->item)->name);
  *error=1;
  DBUG_RETURN(0);
}
