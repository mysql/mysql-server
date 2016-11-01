/* Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "task_debug.h"
#include <stdarg.h>
#include <assert.h>

/* purecov: begin deadcode */
char *mystrcat(char *dest, int *size, const char *src)
{
  int current_size= *size;
  char *ret= dest;

  int num=0;
  while (*dest)
  {
    num++;
    dest++;
  }

  /* Avoids adding chars to full buffer */
  if(current_size < STR_SIZE)
  {
    while ((*dest++= *src++))
    {
      if(++current_size > (STR_SIZE-1))
      {
        break;
      }
    }
    *size= current_size;
    ret= --dest;
  }

  return ret;
}
/* purecov: end */

char *mystrcat_sprintf(char *dest, int *size, const char *format, ...)
{
	va_list args;
	va_start(args, format);

	{
		int current_size= *size;
		
		int remaining_size= STR_SIZE - current_size;
		int ret= vsnprintf(dest, (size_t)remaining_size, format, args);
		if(ret > remaining_size){
			fprintf(stderr,
					"ERROR: mystrcat_sprintf wasn't able to add \"%s\" to "
					"destination string! Full buffer!\n", format);
			ret= remaining_size;
		}
		
		va_end(args);
		
		*size+= ret;
		
		return dest + ret;
	}
}

/* purecov: begin deadcode */
void xcom_simple_log(int l, const char *msg)
{
  char buffer[STR_SIZE+1];
  char *buf= buffer;
  int buffer_size= 0;
  buffer[0]= 0;

  buf= mystrcat(buf, &buffer_size, log_levels[l]);
  buf= mystrcat(buf, &buffer_size, msg);
  buf= mystrcat(buf, &buffer_size, NEWLINE);

  if(l < LOG_INFO)
  {
    fputs(buffer, stderr);
  }
  else
  {
    fputs(buffer, stdout);
  }
}
/* purecov: end */