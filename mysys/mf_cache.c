/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
   
   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA */

/* Open a temporary file and cache it with io_cache. Delete it on close */

#include "mysys_priv.h"

	/*
	** Open a cached tempfile by IO_CACHE
	** Should be used when no seeks are done (only reinit_io_buff)
	** Return 0 if cache is inited ok
	** The actual file is created when the IO_CACHE buffer gets filled
	*/

my_bool open_cached_file(IO_CACHE *cache, const char* dir, const char *prefix,
			  uint cache_size, myf cache_myflags)
{
  DBUG_ENTER("open_cached_file");

  cache->buffer=0;				/* Mark that not open */
  if (!(cache->file_name=my_tempnam(dir,prefix,MYF(MY_WME))))
    DBUG_RETURN(1);
  if (!init_io_cache(cache,-1,cache_size,WRITE_CACHE,0L,0,
		     MYF(cache_myflags | MY_NABP)))
  {
    DBUG_RETURN(0);
  }
  (*free)(cache->file_name);			/* my_tempnam uses malloc() */
  cache->file_name=0;
  DBUG_RETURN(0);
}


my_bool real_open_cached_file(IO_CACHE *cache)
{
  DBUG_ENTER("real_open_cached_file");
  if ((cache->file=my_create(cache->file_name,0,
			     (int) (O_RDWR | O_BINARY | O_TRUNC | O_TEMPORARY |
				    O_SHORT_LIVED),
			     MYF(MY_WME))) >= 0)
  {
#if O_TEMPORARY == 0 && !defined(CANT_DELETE_OPEN_FILES)
    VOID(my_delete(cache->file_name,MYF(MY_WME | ME_NOINPUT)));
#endif
    DBUG_RETURN(0);
  }
  DBUG_RETURN(1);
}


void close_cached_file(IO_CACHE *cache)
{
  DBUG_ENTER("close_cached_file");

  if (my_b_inited(cache))
  {
    VOID(end_io_cache(cache));
    if (cache->file >= 0)
    {
      VOID(my_close(cache->file,MYF(MY_WME)));
#ifdef CANT_DELETE_OPEN_FILES
      VOID(my_delete(cache->file_name,MYF(MY_WME | ME_NOINPUT)));
#endif
    }
    if (cache->file_name)
      (*free)(cache->file_name);		/* my_tempnam uses malloc() */
  }
  DBUG_VOID_RETURN;
}
