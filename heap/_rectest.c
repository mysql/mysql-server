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

/* Test if a record has changed since last read */
/* In heap this is only used when debugging */

#include "heapdef.h"

int _hp_rectest(register HP_INFO *info, register const byte *old)
{
  DBUG_ENTER("_hp_rectest");

  if (memcmp(info->current_ptr,old,(size_t) info->s->reclength))
  {
    DBUG_RETURN((my_errno=HA_ERR_RECORD_CHANGED)); /* Record have changed */
  }
  DBUG_RETURN(0);
} /* _heap_rectest */
