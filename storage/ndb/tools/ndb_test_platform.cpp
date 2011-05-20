/*
   Copyright (c) 2004, 2010, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/


#include <ndb_global.h>
#include <BaseString.hpp>

/*
 * Test BaseString::snprintf
 */

static
int test_snprintf(const char * fmt, int buf_sz, int result)
{
  int ret;
  char buf[100];
  ret = BaseString::snprintf(buf, buf_sz, "%s", fmt);
  
  if(ret < 0)
  {
    printf("BaseString::snprint returns %d with size=%d and strlen(fmt)=%d\n",
	   ret, buf_sz, (int) strlen(fmt));
    return -1;
  }
  
  if(ret+1 == buf_sz)
  {
    printf("BaseString::snprint truncates returns %d with size=%d and strlen(fmt)=%d\n",
	   ret, buf_sz, (int) strlen(fmt));
    return -1;
  }
  
  if(ret != result)
  {
    printf("BaseString::snprint returns incorrect value: returned=%d != expected=%d\n",
	   ret, result);
    return -1;
  }

  for(ret = 0; ret+1 < buf_sz && ret < result; ret++)
  {
    if(buf[ret] != fmt[ret])
    {
      printf("BaseString::snprint Incorrect value in output buffer: "
	     "size=%d returned=expected=%d at pos=%d result=%d  != expected=%d\n",
             buf_sz, result, ret, buf[ret], fmt[ret]);
      return -1;
    }
  }
  return 0;
}

int
main(void)
{

  printf("ndb_test_platform - tests for snprintf and pointer size\n");
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
	   (int) sizeof(UintPtr), (int) sizeof(Uint32*));
    return -1;
  }

  return 0;
}
