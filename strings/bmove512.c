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

/*  File   : bmove512.c
    Author : Michael Widenius;
    Defines: bmove512()

    bmove512(dst, src, len) moves exactly "len" bytes from the source "src"
    to the destination "dst".  "src" and "dst" must be alligned on long
    boundory and len must be a mutliple of 512 byte. If len is not a
    multiple of 512 byte len/512*512+1 bytes is copyed.
    bmove512 is moustly used to copy IO_BLOCKS.  bmove512 should be the
    fastest way to move a mutiple of 512 byte.
*/

#include <global.h>
#include "m_string.h"

#ifndef bmove512

#ifdef HAVE_LONG_LONG
#define LONG ulonglong
#else
#define LONG ulonglong
#endif

void bmove512(register gptr to, register const gptr from, register uint length)
{
  reg1 LONG *f,*t;
  reg3 int len;

  f= (LONG*) from;
  t= (LONG*) to;
  len= (int) length;

#if defined(m88k) || defined(sparc) || defined(HAVE_LONG_LONG)
  do {
    t[0]=f[0];	    t[1]=f[1];	    t[2]=f[2];	    t[3]=f[3];
    t[4]=f[4];	    t[5]=f[5];	    t[6]=f[6];	    t[7]=f[7];
    t[8]=f[8];	    t[9]=f[9];	    t[10]=f[10];    t[11]=f[11];
    t[12]=f[12];    t[13]=f[13];    t[14]=f[14];    t[15]=f[15];
    t[16]=f[16];    t[17]=f[17];    t[18]=f[18];    t[19]=f[19];
    t[20]=f[20];    t[21]=f[21];    t[22]=f[22];    t[23]=f[23];
    t[24]=f[24];    t[25]=f[25];    t[26]=f[26];    t[27]=f[27];
    t[28]=f[28];    t[29]=f[29];    t[30]=f[30];    t[31]=f[31];
    t[32]=f[32];    t[33]=f[33];    t[34]=f[34];    t[35]=f[35];
    t[36]=f[36];    t[37]=f[37];    t[38]=f[38];    t[39]=f[39];
    t[40]=f[40];    t[41]=f[41];    t[42]=f[42];    t[43]=f[43];
    t[44]=f[44];    t[45]=f[45];    t[46]=f[46];    t[47]=f[47];
    t[48]=f[48];    t[49]=f[49];    t[50]=f[50];    t[51]=f[51];
    t[52]=f[52];    t[53]=f[53];    t[54]=f[54];    t[55]=f[55];
    t[56]=f[56];    t[57]=f[57];    t[58]=f[58];    t[59]=f[59];
    t[60]=f[60];    t[61]=f[61];    t[62]=f[62];    t[63]=f[63];
#ifdef HAVE_LONG_LONG
    t+=64; f+=64;
#else
    t[64]=f[64];    t[65]=f[65];    t[66]=f[66];    t[67]=f[67];
    t[68]=f[68];    t[69]=f[69];    t[70]=f[70];    t[71]=f[71];
    t[72]=f[72];    t[73]=f[73];    t[74]=f[74];    t[75]=f[75];
    t[76]=f[76];    t[77]=f[77];    t[78]=f[78];    t[79]=f[79];
    t[80]=f[80];    t[81]=f[81];    t[82]=f[82];    t[83]=f[83];
    t[84]=f[84];    t[85]=f[85];    t[86]=f[86];    t[87]=f[87];
    t[88]=f[88];    t[89]=f[89];    t[90]=f[90];    t[91]=f[91];
    t[92]=f[92];    t[93]=f[93];    t[94]=f[94];    t[95]=f[95];
    t[96]=f[96];    t[97]=f[97];    t[98]=f[98];    t[99]=f[99];
    t[100]=f[100];  t[101]=f[101];  t[102]=f[102];  t[103]=f[103];
    t[104]=f[104];  t[105]=f[105];  t[106]=f[106];  t[107]=f[107];
    t[108]=f[108];  t[109]=f[109];  t[110]=f[110];  t[111]=f[111];
    t[112]=f[112];  t[113]=f[113];  t[114]=f[114];  t[115]=f[115];
    t[116]=f[116];  t[117]=f[117];  t[118]=f[118];  t[119]=f[119];
    t[120]=f[120];  t[121]=f[121];  t[122]=f[122];  t[123]=f[123];
    t[124]=f[124];  t[125]=f[125];  t[126]=f[126];  t[127]=f[127];
    t+=128; f+=128;
#endif
  } while ((len-=512) > 0);
#else
  do {
    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;
    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;
    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;
    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;
    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;
    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;
    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;
    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;
    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;
    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;
    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;
    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;
    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;
    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;
    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;
    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;
    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;
    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;
    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;
    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;
    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;
    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;
    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;
    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;
    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;
    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;
    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;
    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;
    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;
    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;
    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;
    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;
  } while ((len-=512) > 0);
#endif
  return;
} /* bmove512 */

#endif /* bmove512 */
