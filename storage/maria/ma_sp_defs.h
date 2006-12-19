/* Copyright (C) 2006 MySQL AB & Ramil Kalimullin & MySQL Finland AB
   & TCX DataKonsult AB

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

#ifndef _SP_DEFS_H
#define _SP_DEFS_H

#define SPDIMS 2
#define SPTYPE HA_KEYTYPE_DOUBLE
#define SPLEN  8

#ifdef HAVE_SPATIAL

enum wkbType
{
  wkbPoint = 1,
  wkbLineString = 2,
  wkbPolygon = 3,
  wkbMultiPoint = 4,
  wkbMultiLineString = 5,
  wkbMultiPolygon = 6,
  wkbGeometryCollection = 7
};

enum wkbByteOrder
{
  wkbXDR = 0,    /* Big Endian    */
  wkbNDR = 1     /* Little Endian */
};

uint sp_make_key(register MARIA_HA *info, uint keynr, uchar *key,
                 const byte *record, my_off_t filepos);

#endif /*HAVE_SPATIAL*/
#endif /* _SP_DEFS_H */
