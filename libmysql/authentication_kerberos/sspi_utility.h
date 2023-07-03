/* Copyright (c) 2021, 2022, Oracle and/or its affiliates.

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

#ifndef AUTH_GSSAPI_UTILITY_H_
#define AUTH_GSSAPI_UTILITY_H_

#include <stdlib.h>
#include <string.h>

/*
  Note: Added below header files with one extra lines.
  Automatically code formatting changes the order of header file.
  We need to maintain the order of header file, otherwise build will fail.
*/

#include <windows.h>

#include <sspi.h>

#include <SecExt.h>

#include <stdarg.h>

#include <stdio.h>
#include "log_client.h"

/* Maximum length of principal name. */
constexpr int PRINCIPAL_NAME_MAX{256};

void log_client_sspi_error(SECURITY_STATUS err, const char *msg);

bool succeeded(SECURITY_STATUS err);
#endif  // AUTH_GSSAPI_UTILITY_H_
