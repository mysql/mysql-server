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

#define IFACTOR 4

  void
dcopy(char *chardest, char *charsrc, int size)
{
  register int *src, *dest, intcount ;
  int startcharcpy, intoffset, numints2cpy, i ;

  numints2cpy = size >> 2 ;
  startcharcpy = numints2cpy << 2 ;
  intcount = numints2cpy & ~(IFACTOR-1) ;
  intoffset = numints2cpy - intcount ;

  src = (int *)(((int) charsrc) + intcount*sizeof(int*)) ;
  dest = (int *)(((int) chardest) + intcount*sizeof(int*)) ;

  /* copy the ints */
  switch(intoffset)
    do
    {
    case 0: dest[3] = src[3] ;
    case 3: dest[2] = src[2] ;
    case 2: dest[1] = src[1] ;
    case 1: dest[0] = src[0] ;
      intcount -= IFACTOR ;
      dest -= IFACTOR ;
      src -= IFACTOR ;
    } while (intcount >= 0) ;

  /* copy the chars left over by the int copy at the end */
  for(i=startcharcpy ; i<size ; i++)
    chardest[i] = charsrc[i] ;
}
