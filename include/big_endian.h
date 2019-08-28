/* Copyright (c) 2012, 2014, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#include <string.h>

/*
  Data in big-endian format.
*/
static inline void float4store(uchar  *T, float  A)
{ *(T)= ((uchar *) &A)[3];
  *((T)+1)=(char) ((uchar *) &A)[2];
  *((T)+2)=(char) ((uchar *) &A)[1];
  *((T)+3)=(char) ((uchar *) &A)[0]; }

static inline void float4get  (float  *V, const uchar *M)
{ float def_temp;
  ((uchar*) &def_temp)[0]=(M)[3];
  ((uchar*) &def_temp)[1]=(M)[2];
  ((uchar*) &def_temp)[2]=(M)[1];
  ((uchar*) &def_temp)[3]=(M)[0];
  (*V)=def_temp; }

static inline void float8store(uchar  *T, double V)
{ *(T)= ((uchar *) &V)[7];
  *((T)+1)=(char) ((uchar *) &V)[6];
  *((T)+2)=(char) ((uchar *) &V)[5];
  *((T)+3)=(char) ((uchar *) &V)[4];
  *((T)+4)=(char) ((uchar *) &V)[3];
  *((T)+5)=(char) ((uchar *) &V)[2];
  *((T)+6)=(char) ((uchar *) &V)[1];
  *((T)+7)=(char) ((uchar *) &V)[0]; }

static inline void float8get  (double *V, const uchar *M)
{ double def_temp;
  ((uchar*) &def_temp)[0]=(M)[7];                                 
  ((uchar*) &def_temp)[1]=(M)[6];
  ((uchar*) &def_temp)[2]=(M)[5];
  ((uchar*) &def_temp)[3]=(M)[4];
  ((uchar*) &def_temp)[4]=(M)[3];
  ((uchar*) &def_temp)[5]=(M)[2];
  ((uchar*) &def_temp)[6]=(M)[1];
  ((uchar*) &def_temp)[7]=(M)[0];
  (*V) = def_temp; }

static inline void ushortget(uint16 *V, const uchar *pM)
{ *V = (uint16) (((uint16) ((uchar) (pM)[1]))+
                 ((uint16) ((uint16) (pM)[0]) << 8)); }
static inline void shortget (int16  *V, const uchar *pM)
{ *V = (short) (((short) ((uchar) (pM)[1]))+
                ((short) ((short) (pM)[0]) << 8)); }
static inline void longget  (int32  *V, const uchar *pM)
{ int32 def_temp;
  ((uchar*) &def_temp)[0]=(pM)[0];
  ((uchar*) &def_temp)[1]=(pM)[1];
  ((uchar*) &def_temp)[2]=(pM)[2];
  ((uchar*) &def_temp)[3]=(pM)[3];
  (*V)=def_temp; }
static inline void ulongget (uint32 *V, const uchar *pM)
{ uint32 def_temp;
  ((uchar*) &def_temp)[0]=(pM)[0];
  ((uchar*) &def_temp)[1]=(pM)[1];
  ((uchar*) &def_temp)[2]=(pM)[2];
  ((uchar*) &def_temp)[3]=(pM)[3];
  (*V)=def_temp; }
static inline void shortstore(uchar *T, int16 A)
{ uint def_temp=(uint) (A) ;
  *(((char*)T)+1)=(char)(def_temp);
  *(((char*)T)+0)=(char)(def_temp >> 8); }
static inline void longstore (uchar *T, int32 A)
{ *(((char*)T)+3)=((A));
  *(((char*)T)+2)=(((A) >> 8));
  *(((char*)T)+1)=(((A) >> 16));
  *(((char*)T)+0)=(((A) >> 24)); }

static inline void floatget(float *V, const uchar *M)
{
  memcpy(V, (M), sizeof(float));
}

static inline void floatstore(uchar *T, float V)
{
  memcpy((T), (&V), sizeof(float));
}

static inline void doubleget(double *V, const uchar *M)
{
  memcpy(V, (M), sizeof(double));
}

static inline void doublestore(uchar *T, double V)
{
  memcpy((T), &V, sizeof(double));
}

static inline void longlongget(longlong *V, const uchar *M)
{
  memcpy(V, (M), sizeof(ulonglong));
}
static inline void longlongstore(uchar *T, longlong V)
{
  memcpy((T), &V, sizeof(ulonglong));
}
