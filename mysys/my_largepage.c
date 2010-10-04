/* Copyright (C) 2004 MySQL AB, 2008-2009 Sun Microsystems, Inc

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include "mysys_priv.h"

#ifdef HAVE_LARGE_PAGES

#ifdef HAVE_SYS_IPC_H
#include <sys/ipc.h>
#endif

#ifdef HAVE_SYS_SHM_H
#include <sys/shm.h>
#endif

static uint my_get_large_page_size_int(void);
static uchar* my_large_malloc_int(size_t size, myf my_flags);
static my_bool my_large_free_int(uchar* ptr);

/* Gets the size of large pages from the OS */

uint my_get_large_page_size(void)
{
  uint size;
  DBUG_ENTER("my_get_large_page_size");
  
  if (!(size = my_get_large_page_size_int()))
    fprintf(stderr, "Warning: Failed to determine large page size\n");

  DBUG_RETURN(size);
}

/*
  General large pages allocator.
  Tries to allocate memory from large pages pool and falls back to
  my_malloc_lock() in case of failure
*/

uchar* my_large_malloc(size_t size, myf my_flags)
{
  uchar* ptr;
  DBUG_ENTER("my_large_malloc");
  
  if (my_use_large_pages && my_large_page_size)
  {
    if ((ptr = my_large_malloc_int(size, my_flags)) != NULL)
        DBUG_RETURN(ptr);
    if (my_flags & MY_WME)
      fprintf(stderr, "Warning: Using conventional memory pool\n");
  }
      
  DBUG_RETURN(my_malloc_lock(size, my_flags));
}

/*
  General large pages deallocator.
  Tries to deallocate memory as if it was from large pages pool and falls back
  to my_free_lock() in case of failure
 */

void my_large_free(uchar* ptr)
{
  DBUG_ENTER("my_large_free");
  
  /*
    my_large_free_int() can only fail if ptr was not allocated with
    my_large_malloc_int(), i.e. my_malloc_lock() was used so we should free it
    with my_free_lock()
  */
  if (!my_use_large_pages || !my_large_page_size || !my_large_free_int(ptr))
    my_free_lock(ptr);

  DBUG_VOID_RETURN;
}

#ifdef HUGETLB_USE_PROC_MEMINFO
/* Linux-specific function to determine the size of large pages */

uint my_get_large_page_size_int(void)
{
  MYSQL_FILE *f;
  uint size = 0;
  char buf[256];
  DBUG_ENTER("my_get_large_page_size_int");

  if (!(f= mysql_file_fopen(key_file_proc_meminfo, "/proc/meminfo",
                            O_RDONLY, MYF(MY_WME))))
    goto finish;

  while (mysql_file_fgets(buf, sizeof(buf), f))
    if (sscanf(buf, "Hugepagesize: %u kB", &size))
      break;

  mysql_file_fclose(f, MYF(MY_WME));
  
finish:
  DBUG_RETURN(size * 1024);
}
#endif /* HUGETLB_USE_PROC_MEMINFO */

#if HAVE_DECL_SHM_HUGETLB
/* Linux-specific large pages allocator  */
    
uchar* my_large_malloc_int(size_t size, myf my_flags)
{
  int shmid;
  uchar* ptr;
  struct shmid_ds buf;
  DBUG_ENTER("my_large_malloc_int");

  /* Align block size to my_large_page_size */
  size= MY_ALIGN(size, (size_t) my_large_page_size);
  
  shmid = shmget(IPC_PRIVATE, size, SHM_HUGETLB | SHM_R | SHM_W);
  if (shmid < 0)
  {
    if (my_flags & MY_WME)
      fprintf(stderr,
              "Warning: Failed to allocate %lu bytes from HugeTLB memory."
              " errno %d\n", (ulong) size, errno);

    DBUG_RETURN(NULL);
  }

  ptr = (uchar*) shmat(shmid, NULL, 0);
  if (ptr == (uchar *) -1)
  {
    if (my_flags& MY_WME)
      fprintf(stderr, "Warning: Failed to attach shared memory segment,"
              " errno %d\n", errno);
    shmctl(shmid, IPC_RMID, &buf);

    DBUG_RETURN(NULL);
  }

  /*
    Remove the shared memory segment so that it will be automatically freed
    after memory is detached or process exits
  */
  shmctl(shmid, IPC_RMID, &buf);

  DBUG_RETURN(ptr);
}

/* Linux-specific large pages deallocator */

my_bool my_large_free_int(uchar *ptr)
{
  DBUG_ENTER("my_large_free_int");
  DBUG_RETURN(shmdt(ptr) == 0);
}
#endif /* HAVE_DECL_SHM_HUGETLB */

#endif /* HAVE_LARGE_PAGES */
