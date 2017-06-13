/* Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef XCOM_COMMON_H
#define XCOM_COMMON_H

#include <limits.h>
#include <stdint.h>
#include <xcom/xcom.h>
#include "my_compiler.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint16_t xcom_port;
#define number_is_valid_port(n) ((n) > 0 && (n) <= (int) UINT16_MAX)

typedef void *gpointer;
typedef char	gchar;
typedef int	gboolean;
typedef struct timeval GTimeVal;

#ifndef MAX
#define MAX(x,y) ((x) > (y) ? (x) : (y))
#endif
#ifndef MIN
#define MIN(x,y) ((x) < (y) ? (x) : (y))
#endif

#define idx_check_ret(x,limit, ret) if(x < 0 || x >= limit){g_critical("index out of range " #x " < 0  || " #x " >= " #limit" %s:%d", __FILE__, __LINE__); return ret; }else
#define idx_check_fail(x,limit) if(x < 0 || x >= limit){g_critical("index out of range " #x " < 0  || " #x " >= " #limit" %s:%d", __FILE__, __LINE__);abort();}else

#define BSD_COMP


#ifdef __cplusplus
}
#endif

#endif

