/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
   
   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA */

/*
  Radixsort for pointers to fixed length strings.
  A very quick sort for not to long (< 20 char) strings.
  Neads a extra buffers of number_of_elements pointers but is
  2-3 times faster than quicksort
*/

#include "mysys_priv.h"
#include <m_string.h>

	/* Radixsort */

void radixsort_for_str_ptr(uchar **base, uint number_of_elements, size_s size_of_element, uchar **buffer)
{
  uchar **end,**ptr,**buffer_ptr;
  uint32 *count_ptr,*count_end,count[256];
  int pass;

  end=base+number_of_elements; count_end=count+256;
  for (pass=(int) size_of_element-1 ; pass >= 0 ; pass--)
  {
    bzero((gptr) count,sizeof(uint32)*256);
    for (ptr= base ; ptr < end ; ptr++)
      count[ptr[0][pass]]++;
    if (count[0] == number_of_elements)
      goto next;
    for (count_ptr=count+1 ; count_ptr < count_end ; count_ptr++)
    {
      if (*count_ptr == number_of_elements)
	goto next;
      (*count_ptr)+= *(count_ptr-1);
    }
    for (ptr= end ; ptr-- != base ;)
      buffer[--count[ptr[0][pass]]]= *ptr;
    for (ptr=base, buffer_ptr=buffer ; ptr < end ;)
      (*ptr++) = *buffer_ptr++;
  next:;
  }
}
