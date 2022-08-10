/* Copyright (c) 2020, 2022, Oracle and/or its affiliates.

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

/**
  @file
  helper macros to deal with MYSQL options
*/

#ifndef MYSQL_CLIENT_EXTENSIONS_MACROS_H_INCLUDED
#define MYSQL_CLIENT_EXTENSIONS_MACROS_H_INCLUDED

extern PSI_memory_key key_memory_mysql_options;

#define ALLOCATE_EXTENSIONS(OPTS)                                          \
  (OPTS)->extension = (struct st_mysql_options_extention *)my_malloc(      \
      key_memory_mysql_options, sizeof(struct st_mysql_options_extention), \
      MYF(MY_WME | MY_ZEROFILL))

#define ENSURE_EXTENSIONS_PRESENT(OPTS)                \
  do {                                                 \
    if (!(OPTS)->extension) ALLOCATE_EXTENSIONS(OPTS); \
  } while (0)

#define EXTENSION_SET_STRING(OPTS, X, STR)                            \
  do {                                                                \
    if ((OPTS)->extension)                                            \
      my_free((OPTS)->extension->X);                                  \
    else                                                              \
      ALLOCATE_EXTENSIONS(OPTS);                                      \
    (OPTS)->extension->X =                                            \
        ((STR) != nullptr)                                            \
            ? my_strdup(key_memory_mysql_options, (STR), MYF(MY_WME)) \
            : NULL;                                                   \
  } while (0)

#define SET_OPTION(opt_var, arg)                                            \
  do {                                                                      \
    if (mysql->options.opt_var) my_free(mysql->options.opt_var);            \
    mysql->options.opt_var =                                                \
        arg ? my_strdup(key_memory_mysql_options, arg, MYF(MY_WME)) : NULL; \
  } while (0)

#define EXTENSION_SET_SSL_STRING(OPTS, X, STR, mode)               \
  do {                                                             \
    EXTENSION_SET_STRING(OPTS, X, static_cast<const char *>(STR)); \
    if ((OPTS)->extension->X) (OPTS)->extension->ssl_mode = mode;  \
  } while (0)

#endif
