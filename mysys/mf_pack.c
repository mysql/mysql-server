/* Copyright (c) 2000, 2011, Oracle and/or its affiliates.
   Copyright (c) 2012, Monty Program Ab

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
#include <m_string.h>
#ifdef HAVE_PWD_H
#include <pwd.h>
#endif

static char * expand_tilde(char **path);

	/* Pack a dirname ; Changes HOME to ~/ and current dev to ./ */
	/* from is a dirname (from dirname() ?) ending with FN_LIBCHAR */
	/* to may be == from */

void pack_dirname(char * to, const char *from)
{
  int cwd_err;
  size_t d_length,length,UNINIT_VAR(buff_length);
  char * start;
  char buff[FN_REFLEN + 1];
  DBUG_ENTER("pack_dirname");

  (void) intern_filename(to,from);		/* Change to intern name */

#ifdef FN_DEVCHAR
  if ((start=strrchr(to,FN_DEVCHAR)) != 0)	/* Skip device part */
    start++;
  else
#endif
    start=to;

  if (!(cwd_err= my_getwd(buff,FN_REFLEN,MYF(0))))
  {
    buff_length= strlen(buff);
    d_length= (size_t) (start-to);
    if ((start == to ||
	 (buff_length == d_length && !memcmp(buff,start,d_length))) &&
	*start != FN_LIBCHAR && *start)
    {						/* Put current dir before */
      bchange((uchar*) to, d_length, (uchar*) buff, buff_length, strlen(to)+1);
    }
  }

  if ((d_length= cleanup_dirname(to,to)) != 0)
  {
    length=0;
    if (home_dir)
    {
      length= strlen(home_dir);
      if (home_dir[length-1] == FN_LIBCHAR)
	length--;				/* Don't test last '/' */
    }
    if (length > 1 && length < d_length)
    {						/* test if /xx/yy -> ~/yy */
      if (memcmp(to,home_dir,length) == 0 && to[length] == FN_LIBCHAR)
      {
	to[0]=FN_HOMELIB;			/* Filename begins with ~ */
	(void) strmov_overlapp(to+1,to+length);
      }
    }
    if (! cwd_err)
    {						/* Test if cwd is ~/... */
      if (length > 1 && length < buff_length)
      {
	if (memcmp(buff,home_dir,length) == 0 && buff[length] == FN_LIBCHAR)
	{
	  buff[0]=FN_HOMELIB;
	  (void) strmov_overlapp(buff+1,buff+length);
	}
      }
      if (is_prefix(to,buff))
      {
	length= strlen(buff);
	if (to[length])
	  (void) strmov_overlapp(to,to+length);	/* Remove everything before */
	else
	{
	  to[0]= FN_CURLIB;			/* Put ./ instead of cwd */
	  to[1]= FN_LIBCHAR;
	  to[2]= '\0';
	}
      }
    }
  }
  DBUG_PRINT("exit",("to: '%s'",to));
  DBUG_VOID_RETURN;
} /* pack_dirname */


/*
  remove unwanted chars from dirname

  SYNOPSIS
     cleanup_dirname()
     to		Store result here
     from	Dirname to fix.  May be same as to

  IMPLEMENTATION
  "/../" removes prev dir
  "/~/" removes all before ~
  //" is same as "/", except on Win32 at start of a file
  "/./" is removed
  Unpacks home_dir if "~/.." used
  Unpacks current dir if if "./.." used

  RETURN
    #  length of new name   
*/

size_t cleanup_dirname(register char *to, const char *from)
{
  reg5 size_t length;
  reg2 char * pos;
  reg3 char * from_ptr;
  reg4 char * start;
  char parent[5],				/* for "FN_PARENTDIR" */
       buff[FN_REFLEN + 1],*end_parentdir;
#ifdef BACKSLASH_MBTAIL
  CHARSET_INFO *fs= fs_character_set();
#endif
  DBUG_ENTER("cleanup_dirname");
  DBUG_PRINT("enter",("from: '%s'",from));

  start=buff;
  from_ptr=(char *) from;
#ifdef FN_DEVCHAR
  if ((pos=strrchr(from_ptr,FN_DEVCHAR)) != 0)
  {						/* Skip device part */
    length=(size_t) (pos-from_ptr)+1;
    start=strnmov(buff,from_ptr,length); from_ptr+=length;
  }
#endif

  parent[0]=FN_LIBCHAR;
  length=(size_t) (strmov(parent+1,FN_PARENTDIR)-parent);
  for (pos=start ; (*pos= *from_ptr++) != 0 ; pos++)
  {
#ifdef BACKSLASH_MBTAIL
    uint l;
    if (use_mb(fs) && (l= my_ismbchar(fs, from_ptr - 1, from_ptr + 2)))
    {
      for (l-- ; l ; *++pos= *from_ptr++, l--);
      start= pos + 1; /* Don't look inside multi-byte char */
      continue;
    }
#endif
    if (*pos == '/')
      *pos = FN_LIBCHAR;
    if (*pos == FN_LIBCHAR)
    {
      if ((size_t) (pos-start) > length && memcmp(pos-length,parent,length) == 0)
      {						/* If .../../; skip prev */
	pos-=length;
	if (pos != start)
	{					 /* not /../ */
	  pos--;
	  if (*pos == FN_HOMELIB && (pos == start || pos[-1] == FN_LIBCHAR))
	  {
	    if (!home_dir)
	    {
	      pos+=length+1;			/* Don't unpack ~/.. */
	      continue;
	    }
	    pos=strmov(buff,home_dir)-1;	/* Unpacks ~/.. */
	    if (*pos == FN_LIBCHAR)
	      pos--;				/* home ended with '/' */
	  }
	  if (*pos == FN_CURLIB && (pos == start || pos[-1] == FN_LIBCHAR))
	  {
	    if (my_getwd(curr_dir,FN_REFLEN,MYF(0)))
	    {
	      pos+=length+1;			/* Don't unpack ./.. */
	      continue;
	    }
	    pos=strmov(buff,curr_dir)-1;	/* Unpacks ./.. */
	    if (*pos == FN_LIBCHAR)
	      pos--;				/* home ended with '/' */
	  }
	  end_parentdir=pos;
	  while (pos >= start && *pos != FN_LIBCHAR)	/* remove prev dir */
	    pos--;
          if (pos[1] == FN_HOMELIB ||
              (pos >= start && memcmp(pos, parent, length) == 0))
	  {					/* Don't remove ~user/ */
	    pos=strmov(end_parentdir+1,parent);
	    *pos=FN_LIBCHAR;
	    continue;
	  }
	}
      }
      else if ((size_t) (pos-start) == length-1 &&
	       !memcmp(start,parent+1,length-1))
	start=pos;				/* Starts with "../" */
      else if (pos-start > 0 && pos[-1] == FN_LIBCHAR)
      {
#ifdef FN_NETWORK_DRIVES
	if (pos-start != 1)
#endif
	  pos--;			/* Remove dupplicate '/' */
      }
      else if (pos-start > 1 && pos[-1] == FN_CURLIB && pos[-2] == FN_LIBCHAR)
	pos-=2;					/* Skip /./ */
      else if (pos > buff+1 && pos[-1] == FN_HOMELIB && pos[-2] == FN_LIBCHAR)
      {					/* Found ..../~/  */
	buff[0]=FN_HOMELIB;
	buff[1]=FN_LIBCHAR;
	start=buff; pos=buff+1;
      }
    }
  }
  (void) strmov(to,buff);
  DBUG_PRINT("exit",("to: '%s'",to));
  DBUG_RETURN((size_t) (pos-buff));
} /* cleanup_dirname */


/*
  On system where you don't have symbolic links, the following
  code will allow you to create a file: 
  directory-name.sym that should contain the real path
  to the directory.  This will be used if the directory name
  doesn't exists
*/


my_bool my_use_symdir=0;	/* Set this if you want to use symdirs */

#ifdef USE_SYMDIR
void symdirget(char *dir)
{
  char buff[FN_REFLEN + 1];
  char *pos=strend(dir);
  if (dir[0] && pos[-1] != FN_DEVCHAR && my_access(dir, F_OK))
  {
    File file;
    size_t length;
    char temp= *(--pos);            /* May be "/" or "\" */
    strmov(pos,".sym");
    file= my_open(dir, O_RDONLY, MYF(0));
    *pos++=temp; *pos=0;	  /* Restore old filename */
    if (file >= 0)
    {
      if ((length= my_read(file, buff, sizeof(buff) - 1, MYF(0))) > 0)
      {
	for (pos= buff + length ;
	     pos > buff && (iscntrl(pos[-1]) || isspace(pos[-1])) ;
	     pos --);

	/* Ensure that the symlink ends with the directory symbol */
	if (pos == buff || pos[-1] != FN_LIBCHAR)
	  *pos++=FN_LIBCHAR;

	strmake(dir,buff, (size_t) (pos-buff));
      }
      my_close(file, MYF(0));
    }
  }
}
#endif /* USE_SYMDIR */


/**
  Convert a directory name to a format which can be compared as strings

  @param to     result buffer, FN_REFLEN chars in length; may be == from
  @param from   'packed' directory name, in whatever format
  @returns      size of the normalized name

  @details
  - Ensures that last char is FN_LIBCHAR, unless it is FN_DEVCHAR
  - Uses cleanup_dirname

  It does *not* expand ~/ (although, see cleanup_dirname).  Nor does it do
  any case folding.  All case-insensitive normalization should be done by
  the caller.
*/

size_t normalize_dirname(char *to, const char *from)
{
  size_t length;
  char buff[FN_REFLEN + 1];
  DBUG_ENTER("normalize_dirname");

  /*
    Despite the name, this actually converts the name to the system's
    format (TODO: name this properly).
  */
  (void) intern_filename(buff, from);
  length= strlen(buff);			/* Fix that '/' is last */
  if (length &&
#ifdef FN_DEVCHAR
      buff[length - 1] != FN_DEVCHAR &&
#endif
      buff[length - 1] != FN_LIBCHAR && buff[length - 1] != '/')
  {
    /* we need reserve 2 bytes for the trailing slash and the zero */
    if (length >= sizeof (buff) - 1)
      length= sizeof (buff) - 2;
    buff[length]= FN_LIBCHAR;
    buff[length + 1]= '\0';
  }

  length=cleanup_dirname(to, buff);

  DBUG_RETURN(length);
}


/**
  Fixes a directory name so that can be used by open()

  @param to     Result buffer, FN_REFLEN characters. May be == from
  @param from   'Packed' directory name (may contain ~)

  @details
  - Uses normalize_dirname()
  - Expands ~/... to home_dir/...
  - Resolves MySQL's fake "foo.sym" symbolic directory names (if USE_SYMDIR)
  - Changes a UNIX filename to system filename (replaces / with \ on windows)

  @returns
   Length of new directory name (= length of to)
*/

size_t unpack_dirname(char * to, const char *from)
{
  size_t length, h_length;
  char buff[FN_REFLEN+1+4],*suffix,*tilde_expansion;
  DBUG_ENTER("unpack_dirname");

  length= normalize_dirname(buff, from);

  if (buff[0] == FN_HOMELIB)
  {
    suffix=buff+1; tilde_expansion=expand_tilde(&suffix);
    if (tilde_expansion)
    {
      length-= (size_t) (suffix-buff)-1;
      if (length+(h_length= strlen(tilde_expansion)) <= FN_REFLEN)
      {
	if ((h_length > 0) && (tilde_expansion[h_length-1] == FN_LIBCHAR))
	  h_length--;
	if (buff+h_length < suffix)
	  bmove(buff+h_length,suffix,length);
	else
	  bmove_upp((uchar*) buff+h_length+length, (uchar*) suffix+length, length);
	bmove(buff,tilde_expansion,h_length);
      }
    }
  }
#ifdef USE_SYMDIR
  if (my_use_symdir)
    symdirget(buff);
#endif
  DBUG_RETURN(system_filename(to,buff));	/* Fix for open */
} /* unpack_dirname */


	/* Expand tilde to home or user-directory */
	/* Path is reset to point at FN_LIBCHAR after ~xxx */

static char * expand_tilde(char **path)
{
  if (path[0][0] == FN_LIBCHAR)
    return home_dir;			/* ~/ expanded to home */
#ifdef HAVE_GETPWNAM
  {
    char *str,save;
    struct passwd *user_entry;

    if (!(str=strchr(*path,FN_LIBCHAR)))
      str=strend(*path);
    save= *str; *str= '\0';
    user_entry=getpwnam(*path);
    *str=save;
    endpwent();
    if (user_entry)
    {
      *path=str;
      return user_entry->pw_dir;
    }
  }
#endif
  return (char *) 0;
}


/*
  Fix filename so it can be used by open, create

  SYNOPSIS
    unpack_filename()
    to		Store result here. Must be at least of size FN_REFLEN.
    from	Filename in unix format (with ~)

  RETURN
    # length of to

  NOTES
    to may be == from
    ~ will only be expanded if total length < FN_REFLEN
*/


size_t unpack_filename(char * to, const char *from)
{
  size_t length, n_length, buff_length;
  char buff[FN_REFLEN + 1];
  DBUG_ENTER("unpack_filename");

  length=dirname_part(buff, from, &buff_length);/* copy & convert dirname */
  n_length=unpack_dirname(buff,buff);
  if (n_length+strlen(from+length) < FN_REFLEN)
  {
    (void) strmov(buff+n_length,from+length);
    length= system_filename(to,buff);		/* Fix to usably filename */
  }
  else
    length= system_filename(to,from);		/* Fix to usably filename */
  DBUG_RETURN(length);
} /* unpack_filename */


	/* Convert filename (unix standard) to system standard */
	/* Used before system command's like open(), create() .. */
	/* Returns used length of to; total length should be FN_REFLEN */

size_t system_filename(char *to, const char *from)
{
  return (size_t) (strmake(to,from,FN_REFLEN-1)-to);
}

	/* Fix a filename to intern (UNIX format) */

char *intern_filename(char *to, const char *from)
{
  size_t length, to_length;
  char buff[FN_REFLEN + 1];
  if (from == to)
  {						/* Dirname may destroy from */
    strmov(buff,from);
    from=buff;
  }
  length= dirname_part(to, from, &to_length);	/* Copy dirname & fix chars */
  (void) strmov(to + to_length,from+length);
  return (to);
} /* intern_filename */
