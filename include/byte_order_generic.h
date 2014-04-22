/* Copyright (c) 2001, 2014, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

/*
  Endianness-independent definitions for architectures other
  than the x86 architecture.
*/
static inline int16 sint2korr(const uchar *A)
{
  return
    (int16) (((int16) (A[0])) +
             ((int16) (A[1]) << 8))
    ;
}

static inline int32 sint4korr(const uchar *A)
{
  return
    (int32) (((int32) (A[0])) +
             (((int32) (A[1]) << 8)) +
             (((int32) (A[2]) << 16)) +
             (((int32) (A[3]) << 24)))
    ;
}

static inline uint16 uint2korr(const uchar *A)
{
  return
    (uint16) (((uint16) (A[0])) +
              ((uint16) (A[1]) << 8))
    ;
}

static inline uint32 uint4korr(const uchar *A)
{
  return
    (uint32) (((uint32) (A[0])) +
              (((uint32) (A[1])) << 8) +
              (((uint32) (A[2])) << 16) +
              (((uint32) (A[3])) << 24))
    ;
}

static inline ulonglong uint8korr(const uchar *A)
{
  return
    ((ulonglong)(((uint32) (A[0])) +
                 (((uint32) (A[1])) << 8) +
                 (((uint32) (A[2])) << 16) +
                 (((uint32) (A[3])) << 24)) +
     (((ulonglong) (((uint32) (A[4])) +
                    (((uint32) (A[5])) << 8) +
                    (((uint32) (A[6])) << 16) +
                    (((uint32) (A[7])) << 24))) <<
      32))
    ;
}

static inline longlong  sint8korr(const uchar *A)
{
  return (longlong) uint8korr(A);
}

static inline void int2store(uchar *T, uint16 A)
{
  uint def_temp= A ;
  *(T)=   (uchar)(def_temp);
  *(T+1)= (uchar)(def_temp >> 8);
}

static inline void int4store(uchar *T, uint32 A)
{
  *(T)=  (uchar) (A);
  *(T+1)=(uchar) (A >> 8);
  *(T+2)=(uchar) (A >> 16);
  *(T+3)=(uchar) (A >> 24);
}

static inline void int8store(uchar *T, ulonglong A)
{
  uint def_temp= (uint) A,
       def_temp2= (uint) (A >> 32);
  int4store(T,  def_temp);
  int4store(T+4,def_temp2);
}
