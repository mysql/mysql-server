/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */


#include <ndb_global.h>
#include <my_sys.h>
#include <BaseString.hpp>

/*
 * Test BaseString::snprintf
 */

static
int test_snprintf(const char * fmt, int buf_sz, int result)
{
  int ret;
  char buf[100];
  ret = BaseString::snprintf(buf, buf_sz, fmt);
  
  if(ret < 0)
  {
    printf("BaseString::snprint returns %d\n", ret);
    return -1;
  }
  
  if(ret+1 == buf_sz)
  {
    printf("BaseString::snprint truncates\n");
    return -1;
  }
  
  if(ret != result)
  {
    printf("BaseString::snprint returns incorrect value: %d != %d\n",
	   ret, result);
    return -1;
  }

  for(ret = 0; ret+1 < buf_sz && ret < result; ret++)
  {
    if(buf[ret] != fmt[ret])
    {
      printf("BaseString::snprint Incorrect value in output buffer: "
	     "%d %d %d %d %d\n",
             buf_sz, result, ret, buf[ret], fmt[ret]);
      return -1;
    }
  }
  return 0;
}

int
main(void)
{
  /*
   * Test BaseString::snprintf
   */

  if(test_snprintf("test", 1, 4))
    return -1;

  if(test_snprintf("test", 0, 4))
    return -1;
  
  if(test_snprintf("test", 100, 4))
    return -1;

  /*
   * Test UintPtr
   */

  if (sizeof(UintPtr) != sizeof(Uint32*))
  {
    printf("sizeof(UintPtr)=%d != sizeof(Uint32*)=%d\n",
	   sizeof(UintPtr), sizeof(Uint32*));
    return -1;
  }

  return 0;
}
