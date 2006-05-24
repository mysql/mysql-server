/* Copyright (C) 2006 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

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
  This function is for interface functions between fulltext and maria
*/

#include "ma_ftdefs.h"

FT_INFO *maria_ft_init_search(uint flags, void *info, uint keynr,
			      byte *query, uint query_len, CHARSET_INFO *cs,
			      byte *record)
{
  FT_INFO *res;
  if (flags & FT_BOOL)
    res= maria_ft_init_boolean_search((MARIA_HA *) info, keynr, query,
				      query_len, cs);
  else
    res= maria_ft_init_nlq_search((MARIA_HA *) info, keynr, query, query_len,
				  flags, record);
  return res;
}

const struct _ft_vft _ma_ft_vft_nlq = {
  maria_ft_nlq_read_next, maria_ft_nlq_find_relevance,
  maria_ft_nlq_close_search, maria_ft_nlq_get_relevance,
  maria_ft_nlq_reinit_search
};
const struct _ft_vft _ma_ft_vft_boolean = {
  maria_ft_boolean_read_next, maria_ft_boolean_find_relevance,
  maria_ft_boolean_close_search, maria_ft_boolean_get_relevance,
  maria_ft_boolean_reinit_search
};

