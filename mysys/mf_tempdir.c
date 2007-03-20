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

#include "mysys_priv.h"
#include <m_string.h>

#if defined( __WIN__) || defined(OS2) || defined(__NETWARE__)
#define DELIM ';'
#else
#define DELIM ':'
#endif

my_bool init_tmpdir(MY_TMPDIR *tmpdir, const char *pathlist)
{
  char *end, *copy;
  char buff[FN_REFLEN];
  pthread_mutex_init(&tmpdir->mutex, MY_MUTEX_INIT_FAST);
  if (my_init_dynamic_array(&tmpdir->full_list, sizeof(char*), 1, 5))
    return TRUE;
  if (!pathlist || !pathlist[0])
  {
    /* Get default temporary directory */
    pathlist=getenv("TMPDIR");	/* Use this if possible */
#if defined( __WIN__) || defined(OS2) || defined(__NETWARE__)
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
    end=strcend(pathlist, DELIM);
    convert_dirname(buff, pathlist, end);
    if (!(copy=my_strdup(buff, MYF(MY_WME))))
      return TRUE;
    if (insert_dynamic(&tmpdir->full_list, (gptr)&copy))
      return TRUE;
    pathlist=end+1;
  }
  while (*end);
  freeze_size(&tmpdir->full_list);
  tmpdir->list=(char **)tmpdir->full_list.buffer;
  tmpdir->max=tmpdir->full_list.elements-1;
  tmpdir->cur=0;
  return FALSE;
}

char *my_tmpdir(MY_TMPDIR *tmpdir)
{
  char *dir;
  pthread_mutex_lock(&tmpdir->mutex);
  dir=tmpdir->list[tmpdir->cur];
  tmpdir->cur= (tmpdir->cur == tmpdir->max) ? 0 : tmpdir->cur+1;
  pthread_mutex_unlock(&tmpdir->mutex);
  return dir;
}

void free_tmpdir(MY_TMPDIR *tmpdir)
{
  uint i;
  for (i=0; i<=tmpdir->max; i++)
    my_free(tmpdir->list[i], MYF(0));
  delete_dynamic(&tmpdir->full_list);
  pthread_mutex_destroy(&tmpdir->mutex);
}

