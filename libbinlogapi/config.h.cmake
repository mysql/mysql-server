/* Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.

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

#ifndef CONFIG_INCLUDED
#define CONFIG_INCLUDED
/* Headers we may use */
#cmakedefine HAVE_STDINT_H @HAVE_STDINT_H@
#cmakedefine HAVE_ENDIAN_H @HAVE_ENDIAN_H@
/* Symbols we may use */
#define IS_BIG_ENDIAN @IS_BIG_ENDIAN@
#define HAVE_LE64TOH @HAVE_LE64TOH@
#define HAVE_LE32TOH @HAVE_LE32TOH@
#define HAVE_LE16TOH @HAVE_LE16TOH@
#cmakedefine HAVE_ENDIAN_CONVERSION_MACROS @HAVE_ENDIAN_CONVERSION_MACROS@
#cmakedefine SIZEOF_LONG   @SIZEOF_LONG@
#define HAVE_LONG 1
#cmakedefine SIZEOF_INT @SIZEOF_INT@
#define HAVE_INT 1

#endif
