/* Copyright (C) 2000 MySQL AB

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

#ifdef SAFEMALLOC			/* We don't need SAFEMALLOC here */
#undef SAFEMALLOC
#endif

#include "mysys_priv.h"
#include "mysys_err.h"

	/* My memory re allocator */

gptr my_realloc(gptr oldpoint, uint size, myf my_flags)
{
  gptr point;
  DBUG_ENTER("my_realloc");
  DBUG_PRINT("my",("ptr: 0x%lx  size: %u  my_flags: %d",oldpoint, size,
		   my_flags));

  if (!oldpoint && (my_flags & MY_ALLOW_ZERO_PTR))
    DBUG_RETURN(my_malloc(size,my_flags));
#ifdef USE_HALLOC
  if (!(point = malloc(size)))
  {
    if (my_flags & MY_FREE_ON_ERROR)
      my_free(oldpoint,my_flags);
    if (my_flags & MY_HOLD_ON_ERROR)
      DBUG_RETURN(oldpoint);
    my_errno=errno;
    if (my_flags & MY_FAE+MY_WME)
      my_error(EE_OUTOFMEMORY, MYF(ME_BELL+ME_WAITTANG),size);
  }
  else
  {
    memcpy(point,oldpoint,size);
    free(oldpoint);
  }
#else
  if ((point = (char*)realloc(oldpoint,size)) == NULL)
  {
    if (my_flags & MY_FREE_ON_ERROR)
      my_free(oldpoint, my_flags);
    if (my_flags & MY_HOLD_ON_ERROR)
      DBUG_RETURN(oldpoint);
    my_errno=errno;
    if (my_flags & (MY_FAE+MY_WME))
      my_error(EE_OUTOFMEMORY, MYF(ME_BELL+ME_WAITTANG), size);
  }
#endif
  DBUG_PRINT("exit",("ptr: 0x%lx",point));
  DBUG_RETURN(point);
} /* my_realloc */
