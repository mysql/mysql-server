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

/*
  Key cache assignments
*/

#include "myisamdef.h"


/*
  Assign pages of the index file for a table to a key cache

  SYNOPSIS
    mi_assign_to_keycache()
      info          open table
      map           map of indexes to assign to the key cache 
      key_cache_ptr pointer to the key cache handle

  RETURN VALUE
    0 if a success. error code - otherwise.

  NOTES.
    At present pages for all indexes must be assigned to the same key cache.
    In future only pages for indexes specified in the key_map parameter
    of the table will be assigned to the specified key cache.
*/

int mi_assign_to_keycache(MI_INFO *info, ulonglong key_map, 
                          KEY_CACHE_HANDLE *reg_keycache)
{
  int error= 0;
  MYISAM_SHARE* share= info->s;

  DBUG_ENTER("mi_assign_to_keycache");

  share->reg_keycache= reg_keycache;
  if (!(info->lock_type == F_WRLCK && share->w_locks))
  {
    if (flush_key_blocks(*share->keycache, share->kfile, FLUSH_RELEASE))
    {
      error=my_errno;
      mi_mark_crashed(info);		/* Mark that table must be checked */
    }
    share->keycache= reg_keycache;
  }
    
  DBUG_RETURN(error);
}

