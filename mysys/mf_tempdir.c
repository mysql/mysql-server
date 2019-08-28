/* Copyright (c) 2000, 2013, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   Without limiting anything contained in the foregoing, this file,
   which is part of C Driver for MySQL (Connector/C), is also subject to the
   Universal FOSS Exception, version 1.0, a copy of which can be found at
   http://oss.oracle.com/licenses/universal-foss-exception.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

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
      pathlist= DEFAULT_TMPDIR;
  }
  do
  {
    size_t length;
    end=strcend(pathlist, DELIM);
    strmake(buff, pathlist, (uint) (end-pathlist));
    length= cleanup_dirname(buff, buff);
    if (!(copy= my_strndup(buff, length, MYF(MY_WME))) ||
        insert_dynamic(&tmpdir->full_list, &copy))
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

