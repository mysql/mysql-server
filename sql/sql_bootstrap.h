/* Copyright (c) 2010, 2012, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */


#ifndef SQL_BOOTSTRAP_H
#define SQL_BOOTSTRAP_H

/**
  The maximum size of a bootstrap query.
  Increase this size if parsing a longer query during bootstrap is necessary.
  The longest query in use depends on the documentation content,
  see the file fill_help_tables.sql
*/
#define MAX_BOOTSTRAP_QUERY_SIZE 20000
/**
  The maximum size of a bootstrap query, expressed in a single line.
  Do not increase this size, use the multiline syntax instead.
*/
#define MAX_BOOTSTRAP_LINE_SIZE 20000
#define MAX_BOOTSTRAP_ERROR_LEN 256

#define READ_BOOTSTRAP_SUCCESS     0
#define READ_BOOTSTRAP_EOF         1
#define READ_BOOTSTRAP_ERROR       2
#define READ_BOOTSTRAP_QUERY_SIZE  3

typedef void *fgets_input_t;
typedef char * (*fgets_fn_t)(char *, size_t, fgets_input_t, int *error);

int read_bootstrap_query(char *query, int *query_length,
                         fgets_input_t input, fgets_fn_t fgets_fn, int *error);

#endif


