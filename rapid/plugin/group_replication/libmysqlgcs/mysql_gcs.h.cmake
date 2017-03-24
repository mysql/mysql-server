/* Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

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

#ifndef MYSQL_GCS_H_CMAKE
#define MYSQL_GCS_H_CMAKE

#include <config.h>

/*Definitions*/
#cmakedefine HAVE_STRUCT_SOCKADDR_SA_LEN 1
#cmakedefine HAVE_STRUCT_IFREQ_IFR_NAME 1

/* Headers we may use */
#cmakedefine HAVE_ENDIAN_H @HAVE_ENDIAN_H@
/* Symbols we may use */
#cmakedefine HAVE_LE64TOH @HAVE_LE64TOH@
#cmakedefine HAVE_LE32TOH @HAVE_LE32TOH@
#cmakedefine HAVE_LE16TOH @HAVE_LE16TOH@
#cmakedefine HAVE_HTOLE64 @HAVE_HTOLE64@
#cmakedefine HAVE_HTOLE32 @HAVE_HTOLE32@
#cmakedefine HAVE_HTOLE16 @HAVE_HTOLE16@
#cmakedefine HAVE_ENDIAN_CONVERSION_MACROS @HAVE_ENDIAN_CONVERSION_MACROS@

#endif

