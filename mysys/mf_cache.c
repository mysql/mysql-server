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
#include <m_string.h>
#include "my_static.h"
#include "mysys_err.h"

	/*
	  Remove an open tempfile so that it doesn't survive
	  if we crash;	If the operating system doesn't support
	  this, just remember the file name for later removal
	*/

static my_bool cache_remove_open_tmp(IO_CACHE *cache, const char *name)
{
#if O_TEMPORARY == 0
#if !defined(CANT_DELETE_OPEN_FILES)
  /* The following should always succeed */
  (void) my_delete(name,MYF(MY_WME | ME_NOINPUT));
#else
  int length;
  if (!(cache->file_name=
	(char*) my_malloc((length=strlen(name)+1),MYF(MY_WME)))
  {
    my_close(cache->file,MYF(0));
    cache->file = -1;
    errno=my_error=ENOMEM;
    return 1;
  }
  memcpy(cache->file_name,name,length);
#endif
#endif /* O_TEMPORARY == 0 */
  return 0;
}

	/*
	** Open tempfile cached by IO_CACHE
	** Should be used when no seeks are done (only reinit_io_buff)
	** Return 0 if cache is inited ok
	** The actual file is created when the IO_CACHE buffer gets filled
	** If dir is not given, use TMPDIR.
	*/

my_bool open_cached_file(IO_CACHE *cache, const char* dir, const char *prefix,
			  uint cache_size, myf cache_myflags)
{
  DBUG_ENTER("open_cached_file");
  cache->dir=	 dir ? my_strdup(dir,MYF(cache_myflags & MY_WME)) : (char*) 0;
  cache->prefix= (prefix ? my_strdup(prefix,MYF(cache_myflags & MY_WME)) :
		 (char*) 0);
  cache->file_name=0;
  cache->buffer=0;				/* Mark that not open */
  if (!init_io_cache(cache,-1,cache_size,WRITE_CACHE,0L,0,
		     MYF(cache_myflags | MY_NABP)))
  {
    DBUG_RETURN(0);
  }
  my_free(cache->dir,	MYF(MY_ALLOW_ZERO_PTR));
  my_free(cache->prefix,MYF(MY_ALLOW_ZERO_PTR));
  DBUG_RETURN(1);
}

	/* Create the temporary file */

my_bool real_open_cached_file(IO_CACHE *cache)
{
  char name_buff[FN_REFLEN];
  int error=1;
  DBUG_ENTER("real_open_cached_file");
  if ((cache->file=create_temp_file(name_buff, cache->dir, cache->prefix,
				    (O_RDWR | O_BINARY | O_TRUNC |
				     O_TEMPORARY | O_SHORT_LIVED),
				    MYF(MY_WME))) >= 0)
  {
    error=0;
    cache_remove_open_tmp(cache, name_buff);
  }
  DBUG_RETURN(error);
}


void close_cached_file(IO_CACHE *cache)
{
  DBUG_ENTER("close_cached_file");
  if (my_b_inited(cache))
  {
    File file=cache->file;
    cache->file= -1;				/* Don't flush data */
    (void) end_io_cache(cache);
    if (file >= 0)
    {
      (void) my_close(file,MYF(0));
#ifdef CANT_DELETE_OPEN_FILES
      if (cache->file_name)
      {
	(void) my_delete(cache->file_name,MYF(MY_WME | ME_NOINPUT));
	my_free(cache->file_name,MYF(0));
      }
#endif
    }
    my_free(cache->dir,MYF(MY_ALLOW_ZERO_PTR));
    my_free(cache->prefix,MYF(MY_ALLOW_ZERO_PTR));
  }
  DBUG_VOID_RETURN;
}
