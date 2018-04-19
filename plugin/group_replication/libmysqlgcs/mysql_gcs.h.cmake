/* Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

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

