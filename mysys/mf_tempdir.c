/* Copyright (C) 2000 MySQL AB, 2008-2009 Sun Microsystems, Inc

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

#if defined(__WIN__)
#define DELIM ';'
#else
#define DELIM ':'
#endif

my_bool init_tmpdir(MY_TMPDIR *tmpdir, const char *pathlist)
{
  char *end, *copy;
  char buff[FN_REFLEN];
  DBUG_ENTER("init_tmpdir");
  DBUG_PRINT("enter", ("pathlist: %s", pathlist ? pathlist : "NULL"));

  mysql_mutex_init(key_TMPDIR_mutex, &tmpdir->mutex, MY_MUTEX_INIT_FAST);
  if (my_init_dynamic_array(&tmpdir->full_list, sizeof(char*), 1, 5))
    goto err;
  if (!pathlist || !pathlist[0])
  {
    /* Get default temporary directory */
    pathlist=getenv("TMPDIR");	/* Use this if possible */
#if defined(__WIN__)
    if (!pathlist)
      pathlist=getenv("TEMP");
    if (!pathlist)
      pathlist=getenv("TMP");
#endif
    if (!pathlist || !pathlist[0])
      pathlist=(char*) P_tmpdir;
  }
  do
  {
    size_t length;
    end=strcend(pathlist, DELIM);
    strmake(buff, pathlist, (uint) (end-pathlist));
    length= cleanup_dirname(buff, buff);
    if (!(copy= my_strndup(buff, length, MYF(MY_WME))) ||
        insert_dynamic(&tmpdir->full_list, (uchar*) &copy))
      DBUG_RETURN(TRUE);
    pathlist=end+1;
  }
  while (*end);
  freeze_size(&tmpdir->full_list);
  tmpdir->list=(char **)tmpdir->full_list.buffer;
  tmpdir->max=tmpdir->full_list.elements-1;
  tmpdir->cur=0;
  DBUG_RETURN(FALSE);

err:
  delete_dynamic(&tmpdir->full_list);           /* Safe to free */
  mysql_mutex_destroy(&tmpdir->mutex);
  DBUG_RETURN(TRUE);
}


char *my_tmpdir(MY_TMPDIR *tmpdir)
{
  char *dir;
  if (!tmpdir->max)
    return tmpdir->list[0];
  mysql_mutex_lock(&tmpdir->mutex);
  dir=tmpdir->list[tmpdir->cur];
  tmpdir->cur= (tmpdir->cur == tmpdir->max) ? 0 : tmpdir->cur+1;
  mysql_mutex_unlock(&tmpdir->mutex);
  return dir;
}

void free_tmpdir(MY_TMPDIR *tmpdir)
{
  uint i;
  if (!tmpdir->full_list.elements)
    return;
  for (i=0; i<=tmpdir->max; i++)
    my_free(tmpdir->list[i]);
  delete_dynamic(&tmpdir->full_list);
  mysql_mutex_destroy(&tmpdir->mutex);
}

