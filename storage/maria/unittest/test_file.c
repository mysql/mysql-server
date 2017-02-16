/* Copyright (C) 2006-2008 MySQL AB

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

#include <tap.h>
#include <my_sys.h>
#include <my_dir.h>
#include "test_file.h"


/*
  Check that file contance correspond to descriptor

  SYNOPSIS
    test_file()
    file                 File to test
    file_name            Path (and name) of file which is tested
    size                 size of file
    buff_size            size of buffer which is enought to check the file
    desc                 file descriptor to check with

  RETURN
    1 file if OK
    0 error
*/

int test_file(PAGECACHE_FILE file, char *file_name,
              off_t size, size_t buff_size, struct file_desc *desc)
{
  unsigned char *buffr= my_malloc(buff_size, MYF(0));
  off_t pos= 0;
  size_t byte;
  int step= 0;
  int res= 1;                                   /* ok */

#ifdef __WIN__
  /*
    On Windows, the info returned by stat(), specifically file length
    is not necessarily current, because this is the behavior of
    underlying FindFirstFile() function.
  */
  WIN32_FILE_ATTRIBUTE_DATA file_attr;
  LARGE_INTEGER li;
  if(GetFileAttributesEx(file_name, GetFileExInfoStandard, &file_attr) == 0)
  {
    diag("Can't GetFileAttributesEx %s (errno: %d)\n", file_name,
      GetLastError());
    res= 0;
    goto err;
  }
  li.HighPart= file_attr.nFileSizeHigh;
  li.LowPart=  file_attr.nFileSizeLow;
  if(li.QuadPart !=  size)
  {
    diag("file %s size is %llu (should be %llu)\n",
      file_name, (ulonglong)size, (ulonglong)li.QuadPart);
    res= 0;                                       /* failed */
    /* continue to get more information */
  }
#else
  MY_STAT stat_buff, *stat;
  if ((stat= my_stat(file_name, &stat_buff, MYF(0))) == NULL)
  {
    diag("Can't stat() %s (errno: %d)\n", file_name, errno);
    res= 0;
    goto err;
  }
  if (stat->st_size != size)
  {
    diag("file %s size is %lu (should be %lu)\n",
         file_name, (ulong) stat->st_size, (ulong) size);
    res= 0;                                       /* failed */
    /* continue to get more information */
  }
#endif

  /* check content */
  my_seek(file.file, 0, SEEK_SET, MYF(MY_WME));
  while (desc[step].length != 0)
  {
    if (my_read(file.file, buffr, desc[step].length, MYF(0)) !=
        desc[step].length)
    {
      diag("Can't read %u bytes from %s (file: %d  errno: %d)\n",
           (uint)desc[step].length, file_name, file.file, errno);
      res= 0;
      goto err;
    }
    for (byte= 0; byte < desc[step].length; byte++)
    {
      if (buffr[byte] != desc[step].content)
      {
        diag("content of %s mismatch 0x%x in position %lu instead of 0x%x\n",
             file_name, (uint) buffr[byte], (ulong) (pos + byte),
             desc[step].content);
        res= 0;
        goto err;
      }
    }
    pos+= desc[step].length;
    step++;
  }

err:
  my_free(buffr);
  return res;
}
