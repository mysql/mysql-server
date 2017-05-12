/*
 Copyright (c) 2013, 2016, Oracle and/or its affiliates. All rights
 reserved.
 
 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; version 2 of
 the License.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 02110-1301  USA
 */


#ifndef ADAPTER_GLOBAL_H
#define ADAPTER_GLOBAL_H

#define ENABLE_WRAPPER_TYPE_CHECKS 0
#define UNIFIED_DEBUG 1

#ifdef WIN32

#include <malloc.h>

#define __func__ __FUNCTION__
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#define snprintf _snprintf
#define strtoll _strtoi64
#define strtoull _strtoui64
#define isfinite _finite
#define rint(X) floor(.5+X)

#else
#include <unistd.h>

#endif


#endif
