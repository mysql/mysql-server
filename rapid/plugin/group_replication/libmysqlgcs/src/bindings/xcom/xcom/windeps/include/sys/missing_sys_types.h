/* Copyright (c) 2010, 2016, Oracle and/or its affiliates. All rights reserved.

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

/* Includes missing defines for Sun RPC */
#ifndef _MISSING_SYS_TYPES_H
#define _MISSING_SYS_TYPES_H 1

#if defined(WIN32) || defined(WIN64)

#include <winsock2.h>

/* Need C99 __func__ equivalent for Sun RPC */
#define __func__ __FUNCTION__

#define __const const

/* Empty macros */
#define INTDEF(func)
#define INTUSE(func) func

#define __BEGIN_DECLS
#define __END_DECLS
#define __THROW


/* Indicate to Sun RPC we have basic definitions included */
#define makedev
#define __u_char_defined

/* WIN32 still missing some types */
typedef unsigned short __u_short;
typedef unsigned int __u_int;
typedef unsigned long __u_long;

#if defined(WIN32)
typedef long long int __quad_t;
typedef unsigned long long int __u_quad_t;
#endif
#if defined(WIN64)
typedef long int __quad_t;
typedef unsigned long int __u_quad_t;
#endif
typedef struct { int __val[2];} __fsid_t;


typedef long rpc_inline_t;          /* FIXME: Won't work on 64bit compiles */

typedef unsigned long ulong_t;
typedef unsigned long long u_longlong_t;

typedef __quad_t quad_t;
typedef __u_quad_t u_quad_t;
typedef __fsid_t fsid_t;

typedef signed char int8_t;
typedef unsigned __int8 uint8_t;
typedef __int16 int16_t;
typedef unsigned __int16 uint16_t;
typedef __int32 int32_t;
typedef unsigned __int32 uint32_t;
typedef __int64 int64_t;
typedef unsigned __int64 uint64_t;

/* WIN32 missing net/if.h */
typedef int __daddr_t;
typedef char *__caddr_t;

typedef __caddr_t caddr_t;  /* normally defined in rpc.h */

/* mysql functions need uint */
typedef unsigned int uint;

/* xcom_timer uses time_t struct */
typedef long suseconds_t; /* signed number of microseconds */

#endif /* WIN32 || WIN64 */

#endif  /* missing_sys_types.h */
