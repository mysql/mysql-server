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

/**
 * @file ndb_types.h
 */

#ifndef NDB_TYPES_H
#define NDB_TYPES_H

typedef   signed char  Int8;
typedef unsigned char  Uint8;
typedef   signed short Int16;
typedef unsigned short Uint16;
typedef   signed int   Int32;
typedef unsigned int   Uint32;

typedef unsigned int UintR;

#ifdef __SIZE_TYPE__
typedef __SIZE_TYPE__ UintPtr;
#else
#include <ndb_global.h>
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#ifdef HAVE_INTTYPES_H
#include <inttypes.h>
#endif
#if defined(WIN32) || defined(NDB_WIN32)
typedef Uint32 UintPtr;
#else
typedef uintptr_t UintPtr;
#endif
#endif

#if defined(WIN32) || defined(NDB_WIN32)
typedef unsigned __int64 Uint64;
typedef   signed __int64 Int64;
typedef UintPtr ssize_t;
#else
typedef unsigned long long Uint64;
typedef   signed long long Int64;
#endif


#endif
