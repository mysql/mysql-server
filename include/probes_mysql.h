/* Copyright (c) 2008 Sun Microsystems, Inc.
   Use is subject to license terms.

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

#ifndef PROBES_MYSQL_H

#define PROBES_MYSQL_H


#if defined(HAVE_DTRACE) && !defined(DISABLE_DTRACE)

#ifdef __linux__
/* 
  On Linux, generated probes header may include C++ header
  <limits> which conflicts with min and max macros from my_global.h .
  To fix, temporarily undefine the macros.
*/
#pragma push_macro("min")
#pragma push_macro("max")
#undef min
#undef max
#endif

#include "probes_mysql_dtrace.h"

#ifdef __linux__
#pragma pop_macro("min")
#pragma pop_macro("max")
#endif

#else  /* no dtrace */
#include "probes_mysql_nodtrace.h"
#endif
#endif /* PROBES_MYSQL_H */
