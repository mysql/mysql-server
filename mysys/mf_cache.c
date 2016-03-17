/* Copyright (c) 2000, 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/* Open a temporary file and cache it with io_cache. Delete it on close */

#include "my_global.h"
#include "mysys_priv.h"
#include "mysql/psi/mysql_file.h"
#include "my_sys.h"
#include <m_string.h>
#include "my_static.h"
#include "mysys_err.h"

	/*
	  Remove an open tempfile so that it doesn't survive
	  if we crash.
	*/

static my_bool cache_remove_open_tmp(IO_CACHE *cache MY_ATTRIBUTE((unused)),
				     const char *name)
{
#if O_TEMPORARY == 0
  /* The following should always succeed */
  (void) my_delete(name,MYF(MY_WME));
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
                         size_t cache_size, myf cache_myflags)
{
  DBUG_ENTER("open_cached_file");
  cache->dir=	 dir ? my_strdup(key_memory_IO_CACHE,
                                 dir,MYF(cache_myflags & MY_WME)) : (char*) 0;
  cache->prefix= (prefix ? my_strdup(key_memory_IO_CACHE,
                                     prefix,MYF(cache_myflags & MY_WME)) :
		 (char*) 0);
  cache->file_name=0;
  cache->buffer=0;				/* Mark that not open */
  if (!init_io_cache(cache,-1,cache_size,WRITE_CACHE,0L,0,
		     MYF(cache_myflags | MY_NABP)))
  {
    DBUG_RETURN(0);
  }
  my_free(cache->dir);
  my_free(cache->prefix);
  DBUG_RETURN(1);
}

/* Create the temporary file */

my_bool real_open_cached_file(IO_CACHE *cache)
{
  char name_buff[FN_REFLEN];
  int error=1;
  DBUG_ENTER("real_open_cached_file");
  if ((cache->file= mysql_file_create_temp(cache->file_key, name_buff,
                      cache->dir, cache->prefix,
                      (O_RDWR | O_BINARY | O_TRUNC | O_TEMPORARY | O_SHORT_LIVED),
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
    cache->file= -1;    /* Don't flush data */
    (void) end_io_cache(cache);
    if (file >= 0)
    {
      (void) mysql_file_close(file, MYF(0));
    }
    my_free(cache->dir);
    my_free(cache->prefix);
  }
  DBUG_VOID_RETURN;
}
