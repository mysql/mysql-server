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

/* Initialize an maria-database */

#include "maria_def.h"
#include <ft_global.h>

static int maria_inited= 0;
pthread_mutex_t THR_LOCK_maria;

/*
  Initialize maria

  SYNOPSIS
    maria_init()

  TODO
    Open log files and do recovery if need

  RETURN
  0  ok
  #  error number
*/

int maria_init(void)
{
  if (!maria_inited)
  {
    maria_inited= 1;
    pthread_mutex_init(&THR_LOCK_maria,MY_MUTEX_INIT_SLOW);
  }
  return 0;
}


void maria_end(void)
{
  if (maria_inited)
  {
    maria_inited= 0;
    ft_free_stopwords();
    pthread_mutex_destroy(&THR_LOCK_maria);
  }
}
