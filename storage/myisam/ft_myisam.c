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

/* Written by Sergei A. Golubchik, who has a shared copyright to this code */

/*
  This function is for interface functions between fulltext and myisam
*/

#include "ftdefs.h"

FT_INFO *ft_init_search(uint flags, void *info, uint keynr,
                        byte *query, uint query_len, CHARSET_INFO *cs,
                        byte *record)
{
  FT_INFO *res;
  if (flags & FT_BOOL)
    res= ft_init_boolean_search((MI_INFO *)info, keynr, query, query_len,cs);
  else
    res= ft_init_nlq_search((MI_INFO *)info, keynr, query, query_len, flags,
			    record);
  return res;
}
