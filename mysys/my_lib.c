/* Copyright (C) 2000 MySQL AB

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

/* TODO: check for overun of memory for names. */
/*	 Convert MSDOS-TIME to standar time_t (still needed?) */

#include	"mysys_priv.h"
#include	<m_string.h>
#include	<my_dir.h>	/* Structs used by my_dir,includes sys/types */
#include	"mysys_err.h"
#if defined(HAVE_DIRENT_H)
# include <dirent.h>
# define NAMLEN(dirent) strlen((dirent)->d_name)
#else
# define dirent direct
# define NAMLEN(dirent) (dirent)->d_namlen
# if defined(HAVE_SYS_NDIR_H)
#  include <sys/ndir.h>
# endif
# if defined(HAVE_SYS_DIR_H)
#  include <sys/dir.h>
# endif
# if defined(HAVE_NDIR_H)
#  include <ndir.h>
# endif
# if defined(__WIN__)
# include <dos.h>
# ifdef __BORLANDC__
# include <dir.h>
# endif
# endif
#endif
#ifdef VMS
#include <rms.h>
#include <iodef.h>
#include <descrip.h>
#endif

#if defined(THREAD) && defined(HAVE_READDIR_R)
#define READDIR(A,B,C) ((errno=readdir_r(A,B,&C)) != 0 || !C)
#else
#define READDIR(A,B,C) (!(C=readdir(A)))
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
    my_free((uchar*) buffer,MYF(0));
  }
  DBUG_VOID_RETURN;
} /* my_dirend */


	/* Compare in sort of filenames */

static int comp_names(struct fileinfo *a, struct fileinfo *b)
{
  return (strcmp(a->name,b->name));
} /* comp_names */


#if !defined(__WIN__)

MY_DIR	*my_dir(const char *path, myf MyFlags)
{
  char          *buffer;
  MY_DIR        *result= 0;
  FILEINFO      finfo;
  DYNAMIC_ARRAY *dir_entries_storage;
  MEM_ROOT      *names_storage;
  DIR		*dirp;
  struct dirent *dp;
  char		tmp_path[FN_REFLEN+1],*tmp_file;
#ifdef THREAD
  char	dirent_tmp[sizeof(struct dirent)+_POSIX_PATH_MAX+1];
#endif
  DBUG_ENTER("my_dir");
  DBUG_PRINT("my",("path: '%s' MyFlags: %d",path,MyFlags));

#if defined(THREAD) && !defined(HAVE_READDIR_R)
  pthread_mutex_lock(&THR_LOCK_open);
#endif

  dirp = opendir(directory_file_name(tmp_path,(char *) path));
#if defined(__amiga__)
  if ((dirp->dd_fd) < 0)			/* Directory doesn't exists */
    goto error;
#endif
  if (dirp == NULL || 
      ! (buffer= my_malloc(ALIGN_SIZE(sizeof(MY_DIR)) + 
                           ALIGN_SIZE(sizeof(DYNAMIC_ARRAY)) +
                           sizeof(MEM_ROOT), MyFlags)))
    goto error;

  dir_entries_storage= (DYNAMIC_ARRAY*)(buffer + ALIGN_SIZE(sizeof(MY_DIR))); 
  names_storage= (MEM_ROOT*)(buffer + ALIGN_SIZE(sizeof(MY_DIR)) +
                             ALIGN_SIZE(sizeof(DYNAMIC_ARRAY)));
  
  if (my_init_dynamic_array(dir_entries_storage, sizeof(FILEINFO),
                            ENTRIES_START_SIZE, ENTRIES_INCREMENT))
  {
    my_free((uchar*) buffer,MYF(0));
    goto error;
  }
  init_alloc_root(names_storage, NAMES_START_SIZE, NAMES_START_SIZE);
  
  /* MY_DIR structure is allocated and completly initialized at this point */
  result= (MY_DIR*)buffer;

  tmp_file=strend(tmp_path);

#ifdef THREAD
  dp= (struct dirent*) dirent_tmp;
#else
  dp=0;
#endif
  
  while (!(READDIR(dirp,(struct dirent*) dirent_tmp,dp)))
  {
    if (!(finfo.name= strdup_root(names_storage, dp->d_name)))
      goto error;
    
    if (MyFlags & MY_WANT_STAT)
    {
      if (!(finfo.mystat= (MY_STAT*)alloc_root(names_storage, 
                                               sizeof(MY_STAT))))
        goto error;
      
      bzero(finfo.mystat, sizeof(MY_STAT));
      VOID(strmov(tmp_file,dp->d_name));
      VOID(my_stat(tmp_path, finfo.mystat, MyFlags));
      if (!(finfo.mystat->st_mode & MY_S_IREAD))
        continue;
    }
    else
      finfo.mystat= NULL;

    if (push_dynamic(dir_entries_storage, (uchar*)&finfo))
      goto error;
  }

  (void) closedir(dirp);
#if defined(THREAD) && !defined(HAVE_READDIR_R)
  pthread_mutex_unlock(&THR_LOCK_open);
#endif
  result->dir_entry= (FILEINFO *)dir_entries_storage->buffer;
  result->number_off_files= dir_entries_storage->elements;
  
  if (!(MyFlags & MY_DONT_SORT))
    my_qsort((void *) result->dir_entry, result->number_off_files,
          sizeof(FILEINFO), (qsort_cmp) comp_names);
  DBUG_RETURN(result);

 error:
#if defined(THREAD) && !defined(HAVE_READDIR_R)
  pthread_mutex_unlock(&THR_LOCK_open);
#endif
  my_errno=errno;
  if (dirp)
    (void) closedir(dirp);
  my_dirend(result);
  if (MyFlags & (MY_FAE | MY_WME))
    my_error(EE_DIR,MYF(ME_BELL+ME_WAITTANG),path,my_errno);
  DBUG_RETURN((MY_DIR *) NULL);
} /* my_dir */


/*
 * Convert from directory name to filename.
 * On VMS:
 *	 xyzzy:[mukesh.emacs] => xyzzy:[mukesh]emacs.dir.1
 *	 xyzzy:[mukesh] => xyzzy:[000000]mukesh.dir.1
 * On UNIX, it's simple: just make sure there is a terminating /

 * Returns pointer to dst;
 */

char * directory_file_name (char * dst, const char *src)
{
#ifndef VMS

  /* Process as Unix format: just remove test the final slash. */

  char * end;

  if (src[0] == 0)
    src= (char*) ".";				/* Use empty as current */
  end=strmov(dst, src);
  if (end[-1] != FN_LIBCHAR)
  {
    end[0]=FN_LIBCHAR;				/* Add last '/' */
    end[1]='\0';
  }
  return dst;

#else	/* VMS */

  long slen;
  long rlen;
  char * ptr, rptr;
  char bracket;
  struct FAB fab = cc$rms_fab;
  struct NAM nam = cc$rms_nam;
  char esa[NAM$C_MAXRSS];

  if (! src[0])
    src="[.]";					/* Empty is == current dir */

  slen = strlen (src) - 1;
  if (src[slen] == FN_C_AFTER_DIR || src[slen] == FN_C_AFTER_DIR_2 ||
      src[slen] == FN_DEVCHAR)
  {
	/* VMS style - convert [x.y.z] to [x.y]z, [x] to [000000]x */
    fab.fab$l_fna = src;
    fab.fab$b_fns = slen + 1;
    fab.fab$l_nam = &nam;
    fab.fab$l_fop = FAB$M_NAM;

    nam.nam$l_esa = esa;
    nam.nam$b_ess = sizeof esa;
    nam.nam$b_nop |= NAM$M_SYNCHK;

    /* We call SYS$PARSE to handle such things as [--] for us. */
    if (SYS$PARSE(&fab, 0, 0) == RMS$_NORMAL)
    {
      slen = nam.nam$b_esl - 1;
      if (esa[slen] == ';' && esa[slen - 1] == '.')
	slen -= 2;
      esa[slen + 1] = '\0';
      src = esa;
    }
    if (src[slen] != FN_C_AFTER_DIR && src[slen] != FN_C_AFTER_DIR_2)
    {
	/* what about when we have logical_name:???? */
      if (src[slen] == FN_DEVCHAR)
      {				/* Xlate logical name and see what we get */
	VOID(strmov(dst,src));
	dst[slen] = 0;				/* remove colon */
	if (!(src = getenv (dst)))
	  return dst;				/* Can't translate */

	/* should we jump to the beginning of this procedure?
	   Good points: allows us to use logical names that xlate
	   to Unix names,
	   Bad points: can be a problem if we just translated to a device
	   name...
	   For now, I'll punt and always expect VMS names, and hope for
	   the best! */

	slen = strlen (src) - 1;
	if (src[slen] != FN_C_AFTER_DIR && src[slen] != FN_C_AFTER_DIR_2)
	{					/* no recursion here! */
	  VOID(strmov(dst, src));
	  return(dst);
	}
      }
      else
      {						/* not a directory spec */
	VOID(strmov(dst, src));
	return(dst);
      }
    }

    bracket = src[slen];			/* End char */
    if (!(ptr = strchr (src, bracket - 2)))
    {						/* no opening bracket */
      VOID(strmov (dst, src));
      return dst;
    }
    if (!(rptr = strrchr (src, '.')))
      rptr = ptr;
    slen = rptr - src;
    VOID(strmake (dst, src, slen));

    if (*rptr == '.')
    {						/* Put bracket and add */
      dst[slen++] = bracket;			/* (rptr+1) after this */
    }
    else
    {
      /* If we have the top-level of a rooted directory (i.e. xx:[000000]),
	 then translate the device and recurse. */

      if (dst[slen - 1] == ':'
	  && dst[slen - 2] != ':' 	/* skip decnet nodes */
	  && strcmp(src + slen, "[000000]") == 0)
      {
	dst[slen - 1] = '\0';
	if ((ptr = getenv (dst))
	    && (rlen = strlen (ptr) - 1) > 0
	    && (ptr[rlen] == FN_C_AFTER_DIR || ptr[rlen] == FN_C_AFTER_DIR_2)
	    && ptr[rlen - 1] == '.')
	{
	  VOID(strmov(esa,ptr));
	  esa[rlen - 1] = FN_C_AFTER_DIR;
	  esa[rlen] = '\0';
	  return (directory_file_name (dst, esa));
	}
	else
	  dst[slen - 1] = ':';
      }
      VOID(strmov(dst+slen,"[000000]"));
      slen += 8;
    }
    VOID(strmov(strmov(dst+slen,rptr+1)-1,".DIR.1"));
    return dst;
  }
  VOID(strmov(dst, src));
  if (dst[slen] == '/' && slen > 1)
    dst[slen] = 0;
  return dst;
#endif	/* VMS */
} /* directory_file_name */

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
#ifdef __BORLANDC__
  struct ffblk       find;
#else
  struct _finddata_t find;
#endif
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
  tmp_file= strnmov(tmp_file, path, FN_REFLEN-5);
  if (tmp_file[-1] == FN_DEVCHAR)
    *tmp_file++= '.';				/* From current dev-dir */
  if (tmp_file[-1] != FN_LIBCHAR)
    *tmp_file++ =FN_LIBCHAR;
  tmp_file[0]='*';				/* Windows needs this !??? */
  tmp_file[1]='.';
  tmp_file[2]='*';
  tmp_file[3]='\0';

  if (!(buffer= my_malloc(ALIGN_SIZE(sizeof(MY_DIR)) + 
                          ALIGN_SIZE(sizeof(DYNAMIC_ARRAY)) +
                          sizeof(MEM_ROOT), MyFlags)))
    goto error;

  dir_entries_storage= (DYNAMIC_ARRAY*)(buffer + ALIGN_SIZE(sizeof(MY_DIR))); 
  names_storage= (MEM_ROOT*)(buffer + ALIGN_SIZE(sizeof(MY_DIR)) +
                             ALIGN_SIZE(sizeof(DYNAMIC_ARRAY)));
  
  if (my_init_dynamic_array(dir_entries_storage, sizeof(FILEINFO),
                            ENTRIES_START_SIZE, ENTRIES_INCREMENT))
  {
    my_free((uchar*) buffer,MYF(0));
    goto error;
  }
  init_alloc_root(names_storage, NAMES_START_SIZE, NAMES_START_SIZE);
  
  /* MY_DIR structure is allocated and completly initialized at this point */
  result= (MY_DIR*)buffer;

#ifdef __BORLANDC__
  if ((handle= findfirst(tmp_path,&find,0)) == -1L)
#else
  if ((handle=_findfirst(tmp_path,&find)) == -1L)
#endif
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
#ifdef __BORLANDC__
      attrib= find.ff_attrib;
#else
      attrib= find.attrib;
      /*
        Do not show hidden and system files which Windows sometimes create.
        Note. Because Borland's findfirst() is called with the third
        argument = 0 hidden/system files are excluded from the search.
      */
      if (attrib & (_A_HIDDEN | _A_SYSTEM))
        continue;
#endif
#ifdef __BORLANDC__
      if (!(finfo.name= strdup_root(names_storage, find.ff_name)))
        goto error;
#else
      if (!(finfo.name= strdup_root(names_storage, find.name)))
        goto error;
#endif
      if (MyFlags & MY_WANT_STAT)
      {
        if (!(finfo.mystat= (MY_STAT*)alloc_root(names_storage,
                                                 sizeof(MY_STAT))))
          goto error;

        bzero(finfo.mystat, sizeof(MY_STAT));
#ifdef __BORLANDC__
        finfo.mystat->st_size=find.ff_fsize;
#else
        finfo.mystat->st_size=find.size;
#endif
        mode= MY_S_IREAD;
        if (!(attrib & _A_RDONLY))
          mode|= MY_S_IWRITE;
        if (attrib & _A_SUBDIR)
          mode|= MY_S_IFDIR;
        finfo.mystat->st_mode= mode;
#ifdef __BORLANDC__
        finfo.mystat->st_mtime= ((uint32) find.ff_ftime);
#else
        finfo.mystat->st_mtime= ((uint32) find.time_write);
#endif
      }
      else
        finfo.mystat= NULL;

      if (push_dynamic(dir_entries_storage, (uchar*)&finfo))
        goto error;
    }
#ifdef __BORLANDC__
    while (findnext(&find) == 0);
#else
    while (_findnext(handle,&find) == 0);

    _findclose(handle);
#endif
  }

  result->dir_entry= (FILEINFO *)dir_entries_storage->buffer;
  result->number_off_files= dir_entries_storage->elements;

  if (!(MyFlags & MY_DONT_SORT))
    my_qsort((void *) result->dir_entry, result->number_off_files,
          sizeof(FILEINFO), (qsort_cmp) comp_names);
  DBUG_PRINT("exit", ("found %d files", result->number_off_files));
  DBUG_RETURN(result);
error:
  my_errno=errno;
#ifndef __BORLANDC__
  if (handle != -1)
      _findclose(handle);
#endif
  my_dirend(result);
  if (MyFlags & MY_FAE+MY_WME)
    my_error(EE_DIR,MYF(ME_BELL+ME_WAITTANG),path,errno);
  DBUG_RETURN((MY_DIR *) NULL);
} /* my_dir */

#endif /* __WIN__ */

/****************************************************************************
** File status
** Note that MY_STAT is assumed to be same as struct stat
****************************************************************************/ 

int my_fstat(int Filedes, MY_STAT *stat_area,
             myf MyFlags __attribute__((unused)))
{
  DBUG_ENTER("my_fstat");
  DBUG_PRINT("my",("fd: %d  MyFlags: %d", Filedes, MyFlags));
  DBUG_RETURN(fstat(Filedes, (struct stat *) stat_area));
}


MY_STAT *my_stat(const char *path, MY_STAT *stat_area, myf my_flags)
{
  int m_used;
  DBUG_ENTER("my_stat");
  DBUG_PRINT("my", ("path: '%s'  stat_area: 0x%lx  MyFlags: %d", path,
                    (long) stat_area, my_flags));

  if ((m_used= (stat_area == NULL)))
    if (!(stat_area = (MY_STAT *) my_malloc(sizeof(MY_STAT), my_flags)))
      goto error;
  if (! stat((char *) path, (struct stat *) stat_area) )
    DBUG_RETURN(stat_area);

  DBUG_PRINT("error",("Got errno: %d from stat", errno));
  my_errno= errno;
  if (m_used)					/* Free if new area */
    my_free((uchar*) stat_area,MYF(0));

error:
  if (my_flags & (MY_FAE+MY_WME))
  {
    my_error(EE_STAT, MYF(ME_BELL+ME_WAITTANG),path,my_errno);
    DBUG_RETURN((MY_STAT *) NULL);
  }
  DBUG_RETURN((MY_STAT *) NULL);
} /* my_stat */
