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

#ifndef SQL_BOOTSTRAP_H
#define SQL_BOOTSTRAP_H

#include <stddef.h>

struct MYSQL_FILE;

/**
  The maximum size of a bootstrap query.
  Increase this size if parsing a longer query during bootstrap is necessary.
  The longest query in use depends on the documentation content,
  see the file fill_help_tables.sql
*/
#define MAX_BOOTSTRAP_QUERY_SIZE 44000
/**
  The maximum size of a bootstrap query, expressed in a single line.
  Do not increase this size, use the multiline syntax instead.
*/
#define MAX_BOOTSTRAP_LINE_SIZE 44000
#define MAX_BOOTSTRAP_ERROR_LEN 256

#define READ_BOOTSTRAP_SUCCESS 0
#define READ_BOOTSTRAP_EOF 1
#define READ_BOOTSTRAP_ERROR 2
#define READ_BOOTSTRAP_QUERY_SIZE 3

#define QUERY_SOURCE_FILE 0
#define QUERY_SOURCE_COMPILED 1

typedef MYSQL_FILE *fgets_input_t;
typedef char *(*fgets_fn_t)(char *, size_t, fgets_input_t, int *error);

int read_bootstrap_query(char *query, size_t *query_length, fgets_input_t input,
                         fgets_fn_t fgets_fn, int *error);

#endif
