/**
 Copyright (c) 2014, Oracle and/or its affiliates. All rights reserved.

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

#ifndef BAPI_CONFIG_INCLUDED
#define BAPI_CONFIG_INCLUDED
/* Headers we may use */
#define HAVE_STDINT_H 1
#define HAVE_ENDIAN_H 1
/* Symbols we may use */
/* #undef IS_BIG_ENDIAN */
#define HAVE_LE64TOH 1
#define HAVE_LE32TOH 1
#define HAVE_LE16TOH 1
#define HAVE_STRNDUP 1
#define HAVE_ENDIAN_CONVERSION_MACROS 1
#define SIZEOF_LONG_LONG   8
#define HAVE_LONG_LONG 1
#define SIZEOF_LONG 4
#define HAVE_LONG 1
#define SIZEOF_INT 4
#define HAVE_INT 1

#endif
