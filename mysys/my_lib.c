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
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

/* TODO: check for overun of memory for names. */

#include "mysys_priv.h"
#include "my_sys.h"
#include "m_string.h"
#include "my_dir.h"	/* Structs used by my_dir,includes sys/types */
#include "mysys_err.h"
#include "my_thread_local.h"
#if !defined(_WIN32)
# include <dirent.h>
#endif


/*
  We are assuming that directory we are reading is either has less than 
  100 files and so can be read in one initial chunk or has more than 1000
  files and so big increment are suitable.
*/
#define ENTRIES_START_SIZE (8192/sizeof(FILEINFO))
#define ENTRIES_INCREMENT  (65536/sizeof(FILEINFO))
#define NAMES_START_SIZE   32768

static int	comp_names(struct fileinfo *a,struct fileinfo *b);


	/* We need this because program don't know with malloc we used */

void my_dirend(MY_DIR *buffer)
{
  DBUG_ENTER("my_dirend");
  if (buffer)
  {
    delete_dynamic((DYNAMIC_ARRAY*)((char*)buffer + 
                                    ALIGN_SIZE(sizeof(MY_DIR))));
    free_root((MEM_ROOT*)((char*)buffer + ALIGN_SIZE(sizeof(MY_DIR)) + 
                          ALIGN_SIZE(sizeof(DYNAMIC_ARRAY))), MYF(0));
    my_free(buffer);
  }
  DBUG_VOID_RETURN;
} /* my_dirend */


	/* Compare in sort of filenames */

static int comp_names(struct fileinfo *a, struct fileinfo *b)
{
  return (strcmp(a->name,b->name));
} /* comp_names */


#if !defined(_WIN32)

static char* directory_file_name(char *dst, const char *src);

MY_DIR	*my_dir(const char *path, myf MyFlags)
{
  char          *buffer;
  MY_DIR        *result= 0;
  FILEINFO      finfo;
  DYNAMIC_ARRAY *dir_entries_storage;
  MEM_ROOT      *names_storage;
  DIR		*dirp;
  char		tmp_path[FN_REFLEN + 2], *tmp_file;
  const struct dirent *dp;

  DBUG_ENTER("my_dir");
  DBUG_PRINT("my",("path: '%s' MyFlags: %d",path,MyFlags));

  dirp = opendir(directory_file_name(tmp_path,(char *) path));
  if (dirp == NULL ||
      ! (buffer= my_malloc(key_memory_MY_DIR,
                           ALIGN_SIZE(sizeof(MY_DIR)) + 
                           ALIGN_SIZE(sizeof(DYNAMIC_ARRAY)) +
                           sizeof(MEM_ROOT), MyFlags)))
    goto error;

  dir_entries_storage= (DYNAMIC_ARRAY*)(buffer + ALIGN_SIZE(sizeof(MY_DIR)));
  names_storage= (MEM_ROOT*)(buffer + ALIGN_SIZE(sizeof(MY_DIR)) +
                             ALIGN_SIZE(sizeof(DYNAMIC_ARRAY)));

  if (my_init_dynamic_array(dir_entries_storage,
                            key_memory_MY_DIR,
                            sizeof(FILEINFO),
                            NULL,               /* init_buffer */
                            ENTRIES_START_SIZE, ENTRIES_INCREMENT))
  {
    my_free(buffer);
    goto error;
  }
  init_alloc_root(key_memory_MY_DIR, names_storage, NAMES_START_SIZE, NAMES_START_SIZE);

  /* MY_DIR structure is allocated and completly initialized at this point */
  result= (MY_DIR*)buffer;

  tmp_file=strend(tmp_path);

  for (dp= readdir(dirp) ; dp; dp= readdir(dirp))
  {
    if (!(finfo.name= strdup_root(names_storage, dp->d_name)))
      goto error;

    if (MyFlags & MY_WANT_STAT)
    {
      if (!(finfo.mystat= (MY_STAT*)alloc_root(names_storage,
                                               sizeof(MY_STAT))))
        goto error;

      memset(finfo.mystat, 0, sizeof(MY_STAT));
      (void) my_stpcpy(tmp_file,dp->d_name);
      (void) my_stat(tmp_path, finfo.mystat, MyFlags);
      if (!(finfo.mystat->st_mode & MY_S_IREAD))
        continue;
    }
    else
      finfo.mystat= NULL;

    if (insert_dynamic(dir_entries_storage, (uchar*)&finfo))
      goto error;
  }

  (void) closedir(dirp);

  result->dir_entry= (FILEINFO *)dir_entries_storage->buffer;
  result->number_off_files= dir_entries_storage->elements;

  if (!(MyFlags & MY_DONT_SORT))
    my_qsort((void *) result->dir_entry, result->number_off_files,
          sizeof(FILEINFO), (qsort_cmp) comp_names);
  DBUG_RETURN(result);

 error:
  set_my_errno(errno);
  if (dirp)
    (void) closedir(dirp);
  my_dirend(result);
  if (MyFlags & (MY_FAE | MY_WME))
  {
    char errbuf[MYSYS_STRERROR_SIZE];
    my_error(EE_DIR, MYF(0), path,
             my_errno(), my_strerror(errbuf, sizeof(errbuf), my_errno()));
  }
  DBUG_RETURN((MY_DIR *) NULL);
} /* my_dir */


/*
 * Convert from directory name to filename.
 * On UNIX, it's simple: just make sure there is a terminating /

 * Returns pointer to dst;
 */

static char* directory_file_name(char *dst, const char *src)
{
  /* Process as Unix format: just remove test the final slash. */
  char *end;
  DBUG_ASSERT(strlen(src) < (FN_REFLEN + 1));

  if (src[0] == 0)
    src= (char*) ".";				/* Use empty as current */
  end= my_stpnmov(dst, src, FN_REFLEN + 1);
  if (end[-1] != FN_LIBCHAR)
  {
    end[0]=FN_LIBCHAR;				/* Add last '/' */
    end[1]='\0';
  }
  return dst;
}

#else

/*
*****************************************************************************
** Read long filename using windows rutines
*****************************************************************************
*/

MY_DIR	*my_dir(const char *path, myf MyFlags)
{
  char          *buffer;
  MY_DIR        *result= 0;
  FILEINFO      finfo;
  DYNAMIC_ARRAY *dir_entries_storage;
  MEM_ROOT      *names_storage;
  struct _finddata_t find;
  ushort	mode;
  char		tmp_path[FN_REFLEN],*tmp_file,attrib;
#ifdef _WIN64
  __int64       handle;
#else
  long		handle;
#endif
  DBUG_ENTER("my_dir");
  DBUG_PRINT("my",("path: '%s' stat: %d  MyFlags: %d",path,MyFlags));

  /* Put LIB-CHAR as last path-character if not there */
  tmp_file=tmp_path;
  if (!*path)
    *tmp_file++ ='.';				/* From current dir */
  tmp_file= my_stpnmov(tmp_file, path, FN_REFLEN-5);
  if (tmp_file[-1] == FN_DEVCHAR)
    *tmp_file++= '.';				/* From current dev-dir */
  if (tmp_file[-1] != FN_LIBCHAR)
    *tmp_file++ =FN_LIBCHAR;
  tmp_file[0]='*';				/* Windows needs this !??? */
  tmp_file[1]='.';
  tmp_file[2]='*';
  tmp_file[3]='\0';

  if (!(buffer= my_malloc(key_memory_MY_DIR,
                          ALIGN_SIZE(sizeof(MY_DIR)) + 
                          ALIGN_SIZE(sizeof(DYNAMIC_ARRAY)) +
                          sizeof(MEM_ROOT), MyFlags)))
    goto error;

  dir_entries_storage= (DYNAMIC_ARRAY*)(buffer + ALIGN_SIZE(sizeof(MY_DIR))); 
  names_storage= (MEM_ROOT*)(buffer + ALIGN_SIZE(sizeof(MY_DIR)) +
                             ALIGN_SIZE(sizeof(DYNAMIC_ARRAY)));
  
  if (my_init_dynamic_array(dir_entries_storage,
                            key_memory_MY_DIR,
                            sizeof(FILEINFO),
                            NULL,               /* init_buffer */
                            ENTRIES_START_SIZE, ENTRIES_INCREMENT))
  {
    my_free(buffer);
    goto error;
  }
  init_alloc_root(key_memory_MY_DIR, names_storage, NAMES_START_SIZE, NAMES_START_SIZE);
  
  /* MY_DIR structure is allocated and completly initialized at this point */
  result= (MY_DIR*)buffer;

  if ((handle=_findfirst(tmp_path,&find)) == -1L)
  {
    DBUG_PRINT("info", ("findfirst returned error, errno: %d", errno));
    if  (errno != EINVAL)
      goto error;
    /*
      Could not read the directory, no read access.
      Probably because by "chmod -r".
      continue and return zero files in dir
    */
  }
  else
  {

    do
    {
      attrib= find.attrib;
      /*
        Do not show hidden and system files which Windows sometimes create.
        Note. Because Borland's findfirst() is called with the third
        argument = 0 hidden/system files are excluded from the search.
      */
      if (attrib & (_A_HIDDEN | _A_SYSTEM))
        continue;
      if (!(finfo.name= strdup_root(names_storage, find.name)))
        goto error;
      if (MyFlags & MY_WANT_STAT)
      {
        if (!(finfo.mystat= (MY_STAT*)alloc_root(names_storage,
                                                 sizeof(MY_STAT))))
          goto error;

        memset(finfo.mystat, 0, sizeof(MY_STAT));
        finfo.mystat->st_size=find.size;
        mode= MY_S_IREAD;
        if (!(attrib & _A_RDONLY))
          mode|= MY_S_IWRITE;
        if (attrib & _A_SUBDIR)
          mode|= MY_S_IFDIR;
        finfo.mystat->st_mode= mode;
        finfo.mystat->st_mtime= ((uint32) find.time_write);
      }
      else
        finfo.mystat= NULL;

      if (insert_dynamic(dir_entries_storage, (uchar*)&finfo))
        goto error;
    }
    while (_findnext(handle,&find) == 0);

    _findclose(handle);
  }

  result->dir_entry= (FILEINFO *)dir_entries_storage->buffer;
  result->number_off_files= dir_entries_storage->elements;

  if (!(MyFlags & MY_DONT_SORT))
    my_qsort((void *) result->dir_entry, result->number_off_files,
          sizeof(FILEINFO), (qsort_cmp) comp_names);
  DBUG_PRINT("exit", ("found %d files", result->number_off_files));
  DBUG_RETURN(result);
error:
  set_my_errno(errno);
  if (handle != -1)
      _findclose(handle);
  my_dirend(result);
  if (MyFlags & MY_FAE+MY_WME)
  {
    char errbuf[MYSYS_STRERROR_SIZE];
    my_error(EE_DIR, MYF(0), path,
             errno, my_strerror(errbuf, sizeof(errbuf), errno));
  }
  DBUG_RETURN((MY_DIR *) NULL);
} /* my_dir */

#endif /* _WIN32 */

/****************************************************************************
** File status
** Note that MY_STAT is assumed to be same as struct stat
****************************************************************************/ 


int my_fstat(File Filedes, MY_STAT *stat_area,
             myf MyFlags MY_ATTRIBUTE((unused)))
{
  DBUG_ENTER("my_fstat");
  DBUG_PRINT("my",("fd: %d  MyFlags: %d", Filedes, MyFlags));
#ifdef _WIN32
  DBUG_RETURN(my_win_fstat(Filedes, stat_area));
#else
  DBUG_RETURN(fstat(Filedes, (struct stat *) stat_area));
#endif
}


MY_STAT *my_stat(const char *path, MY_STAT *stat_area, myf my_flags)
{
  const int m_used= (stat_area == NULL);
  DBUG_ENTER("my_stat");
  DBUG_PRINT("my", ("path: '%s'  stat_area: 0x%lx  MyFlags: %d", path,
                    (long) stat_area, my_flags));

  if (m_used)
    if (!(stat_area= (MY_STAT *) my_malloc(key_memory_MY_STAT,
                                           sizeof(MY_STAT), my_flags)))
      goto error;
#ifndef _WIN32
    if (! stat((char *) path, (struct stat *) stat_area) )
      DBUG_RETURN(stat_area);
#else
    if (! my_win_stat(path, stat_area) )
      DBUG_RETURN(stat_area);
#endif
  DBUG_PRINT("error",("Got errno: %d from stat", errno));
  set_my_errno(errno);
  if (m_used)					/* Free if new area */
    my_free(stat_area);

error:
  if (my_flags & (MY_FAE+MY_WME))
  {
    char errbuf[MYSYS_STRERROR_SIZE];
    my_error(EE_STAT, MYF(0), path,
             my_errno(), my_strerror(errbuf, sizeof(errbuf), my_errno()));
    DBUG_RETURN((MY_STAT *) NULL);
  }
  DBUG_RETURN((MY_STAT *) NULL);
} /* my_stat */
