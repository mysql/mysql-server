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

#ifdef __GNUC__
#pragma implementation				// gcc: Class implementation
#endif

#include "mysql_priv.h"
#include "sql_select.h"
#include "opt_ft.h"

/****************************************************************************
** Create a FT or QUICK RANGE based on a key
****************************************************************************/

QUICK_SELECT *get_ft_or_quick_select_for_ref(TABLE *table, JOIN_TAB *tab)
{
  if (tab->type == JT_FT)
    return new FT_SELECT(table, &tab->ref);
  else
    return get_quick_select_for_ref(table, &tab->ref);
}

