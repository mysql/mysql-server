/*
   Copyright (c) 2001, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef MY_GLOBAL_INCLUDED
#define MY_GLOBAL_INCLUDED

/**
  @file include/my_global.h
  This include file used to be included first in every header file.
  Do not include it in new code; instead, include what you use.
*/

#include <assert.h>
#include <errno.h>				/* Recommended by debian */
#include <fcntl.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stddef.h>
#include <sys/types.h>
#include <time.h>

#include "my_config.h"

#if !defined(_WIN32)
#include <netdb.h>
#endif
#ifdef MY_MSCRT_DEBUG
#include <crtdbg.h>
#endif

#include "my_compiler.h"
#include "my_inttypes.h"
#include "my_io.h"
#include "my_macros.h"
#include "my_sharedlib.h"
#include "my_shm_defaults.h"
#include "my_systime.h"
#include "my_table_map.h"

#endif  // MY_GLOBAL_INCLUDED
