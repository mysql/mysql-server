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

#include "ftdefs.h"

/* queries myisam and returns list of documents matched */

static int FT_DOC_cmp(FT_DOC *a, FT_DOC *b)
{
    return sgn(b->weight - a->weight);
}

FT_DOCLIST *ft_init_search(void *info, uint keynr, byte *query,
			    uint query_len, my_bool presort)
{
  FT_DOCLIST *dlist;
  my_off_t saved_lastpos=((MI_INFO *)info)->lastpos;

/* black magic ON */
  if ((int) (keynr = _mi_check_index((MI_INFO *)info,keynr)) < 0)
    return NULL;
  if (_mi_readinfo((MI_INFO *)info,F_RDLCK,1))
    return NULL;
/* black magic OFF */

  if (is_boolean(query, query_len))
    dlist=ft_boolean_search(info,keynr,query,query_len);
  else
    dlist=ft_nlq_search(info,keynr,query,query_len);

  if(dlist && presort)
  {
    qsort(dlist->doc, dlist->ndocs, sizeof(FT_DOC), (qsort_cmp)&FT_DOC_cmp);
  }

  ((MI_INFO *)info)->lastpos=saved_lastpos;
  return dlist;
}

int ft_read_next(FT_DOCLIST *handler, char *record)
{
  MI_INFO *info= (MI_INFO *) handler->info;

  if (++handler->curdoc >= handler->ndocs)
  {
    --handler->curdoc;
    return HA_ERR_END_OF_FILE;
  }

  info->update&= (HA_STATE_CHANGED | HA_STATE_ROW_CHANGED);

  info->lastpos=handler->doc[handler->curdoc].dpos;
  if (!(*info->read_record)(info,info->lastpos,record))
  {
    info->update|= HA_STATE_AKTIV;		/* Record is read */
    return 0;
  }
  return my_errno;
}
