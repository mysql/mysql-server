/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */


/* Procedures (functions with changes output of select) */

#ifdef __GNUC__
#pragma implementation				// gcc: Class implementation
#endif

#include "mysql_priv.h"
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

/*****************************************************************************
** Setup handling of procedure
** Return 0 if everything is ok
*****************************************************************************/

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
