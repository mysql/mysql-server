/* Copyright (c) 2001, 2015, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   Without limiting anything contained in the foregoing, this file,
   which is part of C Driver for MySQL (Connector/C), is also subject to the
   Universal FOSS Exception, version 1.0, a copy of which can be found at
   http://oss.oracle.com/licenses/universal-foss-exception.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

/*
  Optimized functions for the x86 architecture (_WIN32 included).

  x86 handles misaligned reads and writes just fine, so suppress
  UBSAN warnings for these functions.
*/
static inline int16  sint2korr(const uchar *A) SUPPRESS_UBSAN;
static inline int16  sint2korr(const uchar *A) { return *((int16*) A); }

static inline int32 sint4korr(const uchar *A) SUPPRESS_UBSAN;
static inline int32 sint4korr(const uchar *A) { return *((int32*) A); }

static inline uint16 uint2korr(const uchar *A) SUPPRESS_UBSAN;
static inline uint16 uint2korr(const uchar *A) { return *((uint16*) A); }

static inline uint32 uint4korr(const uchar *A) SUPPRESS_UBSAN;
static inline uint32 uint4korr(const uchar *A) { return *((uint32*) A); }

static inline ulonglong uint8korr(const uchar *A) SUPPRESS_UBSAN;
static inline ulonglong uint8korr(const uchar *A) { return *((ulonglong*) A);}

static inline longlong  sint8korr(const uchar *A) SUPPRESS_UBSAN;
static inline longlong  sint8korr(const uchar *A) { return *((longlong*) A); }

static inline void int2store(uchar *T, uint16 A) SUPPRESS_UBSAN;
static inline void int2store(uchar *T, uint16 A)
{
  *((uint16*) T)= A;
}

static inline void int4store(uchar *T, uint32 A) SUPPRESS_UBSAN;
static inline void int4store(uchar *T, uint32 A)
{
  *((uint32*) T)= A;
}

static inline void int8store(uchar *T, ulonglong A) SUPPRESS_UBSAN;
static inline void int8store(uchar *T, ulonglong A)
{
  *((ulonglong*) T)= A;
}
