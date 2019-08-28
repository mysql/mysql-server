/* Copyright (c) 2001, 2017, Oracle and/or its affiliates. All rights reserved.

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
  Optimized function-like macros for the x86 architecture (_WIN32 included).
*/
#define sint2korr(A)	(int16) (*((int16 *) (A)))
#define sint3korr(A)	((int32) ((((uchar) (A)[2]) & 128) ? \
				  (((uint32) 255L << 24) | \
				   (((uint32) (uchar) (A)[2]) << 16) |\
				   (((uint32) (uchar) (A)[1]) << 8) | \
				   ((uint32) (uchar) (A)[0])) : \
				  (((uint32) (uchar) (A)[2]) << 16) |\
				  (((uint32) (uchar) (A)[1]) << 8) | \
				  ((uint32) (uchar) (A)[0])))
#define sint4korr(A)	(int32)  (*((int32 *) (A)))
#define uint2korr(A)	(uint16) (*((uint16 *) (A)))
#define uint3korr(A)	(uint32) (((uint32) ((uchar) (A)[0])) +\
				  (((uint32) ((uchar) (A)[1])) << 8) +\
				  (((uint32) ((uchar) (A)[2])) << 16))
#define uint4korr(A)	(uint32) (*((uint32 *) (A)))
#define uint5korr(A)	((ulonglong)(((uint32) ((uchar) (A)[0])) +\
				    (((uint32) ((uchar) (A)[1])) << 8) +\
				    (((uint32) ((uchar) (A)[2])) << 16) +\
				    (((uint32) ((uchar) (A)[3])) << 24)) +\
				    (((ulonglong) ((uchar) (A)[4])) << 32))
#define uint6korr(A)	((ulonglong)(((uint32)    ((uchar) (A)[0]))          + \
                                     (((uint32)    ((uchar) (A)[1])) << 8)   + \
                                     (((uint32)    ((uchar) (A)[2])) << 16)  + \
                                     (((uint32)    ((uchar) (A)[3])) << 24)) + \
                         (((ulonglong) ((uchar) (A)[4])) << 32) +       \
                         (((ulonglong) ((uchar) (A)[5])) << 40))
#define uint8korr(A)	(ulonglong) (*((ulonglong *) (A)))
#define sint8korr(A)	(longlong) (*((longlong *) (A)))

#define int2store(T,A)	do { uchar *pT= (uchar*)(T);\
                             *((uint16*)(pT))= (uint16) (A);\
                        } while (0)
  
#define int3store(T,A)  do { *(T)=  (uchar) ((A));\
                            *(T+1)=(uchar) (((uint) (A) >> 8));\
                            *(T+2)=(uchar) (((A) >> 16));\
                        } while (0)
#define int4store(T,A)	do { uchar *pT= (uchar*)(T);\
                             *((uint32 *) (pT))= (uint32) (A); \
                        } while (0)

#define int5store(T,A)  do { *(T)= (uchar)((A));\
                             *((T)+1)=(uchar) (((A) >> 8));\
                             *((T)+2)=(uchar) (((A) >> 16));\
                             *((T)+3)=(uchar) (((A) >> 24));\
                             *((T)+4)=(uchar) (((A) >> 32));\
                        } while(0)
#define int6store(T,A)  do { *(T)=    (uchar)((A));          \
                             *((T)+1)=(uchar) (((A) >> 8));  \
                             *((T)+2)=(uchar) (((A) >> 16)); \
                             *((T)+3)=(uchar) (((A) >> 24)); \
                             *((T)+4)=(uchar) (((A) >> 32)); \
                             *((T)+5)=(uchar) (((A) >> 40)); \
                        } while(0)
#define int8store(T,A)	do { uchar *pT= (uchar*)(T);\
                             *((ulonglong *) (pT))= (ulonglong) (A);\
                        } while(0)

