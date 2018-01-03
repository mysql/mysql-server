/* Copyright (c) 2010, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef _welcome_copyright_notice_h_
#define _welcome_copyright_notice_h_

/**
  @file include/welcome_copyright_notice.h
*/

#include <string.h>

#define COPYRIGHT_NOTICE_CURRENT_YEAR "2018"

/*
  This define specifies copyright notice which is displayed by every MySQL
  program on start, or on help screen.
*/
#define ORACLE_WELCOME_COPYRIGHT_NOTICE(first_year) \
  (strcmp(first_year, COPYRIGHT_NOTICE_CURRENT_YEAR) ? \
   "Copyright (c) " first_year ", " COPYRIGHT_NOTICE_CURRENT_YEAR ", " \
   "Oracle and/or its affiliates. All rights reserved.\n\nOracle is a " \
   "registered trademark of Oracle Corporation and/or its\naffiliates. " \
   "Other names may be trademarks of their respective\nowners.\n" : \
   "Copyright (c) " first_year ", Oracle and/or its affiliates. " \
   "All rights reserved.\n\nOracle is a registered trademark of " \
   "Oracle Corporation and/or its\naffiliates. Other names may be " \
   "trademarks of their respective\nowners.\n")

#endif /* _welcome_copyright_notice_h_ */
