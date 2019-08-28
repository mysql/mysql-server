/* Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.

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
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */

#ifndef INNODB_PRIV_INCLUDED
#define INNODB_PRIV_INCLUDED

/** @file Declaring server-internal functions that are used by InnoDB. */

#include <sql_priv.h>

class THD;

int get_quote_char_for_identifier(THD *thd, const char *name, uint length);
bool schema_table_store_record(THD *thd, TABLE *table);
void localtime_to_TIME(MYSQL_TIME *to, struct tm *from);
bool check_global_access(THD *thd, ulong want_access);
uint strconvert(CHARSET_INFO *from_cs, const char *from,
                CHARSET_INFO *to_cs, char *to, uint to_length,
                uint *errors);
void sql_print_error(const char *format, ...);



#endif /* INNODB_PRIV_INCLUDED */
