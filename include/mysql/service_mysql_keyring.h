/* Copyright (c) 2014, 2016 Oracle and/or its affiliates. All rights reserved.

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

#ifndef MYSQL_SERVICE_MYSQL_PLUGIN_KEYRING_INCLUDED
#define MYSQL_SERVICE_MYSQL_PLUGIN_KEYRING_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

extern struct mysql_keyring_service_st
{
  int (*my_key_store_func)(const char *, const char *, const char *,
                           const void *, size_t);
  int (*my_key_fetch_func)(const char *, char **, const char *, void **,
                           size_t *);
  int (*my_key_remove_func)(const char *, const char *);
  int (*my_key_generate_func)(const char *, const char *, const char *,
                              size_t);
} *mysql_keyring_service;

#ifdef MYSQL_DYNAMIC_PLUGIN

#define my_key_store(key_id, key_type, user_id, key, key_len) \
  mysql_keyring_service->my_key_store_func(key_id, key_type, user_id, key, \
                                           key_len)
#define my_key_fetch(key_id, key_type, user_id, key, key_len) \
  mysql_keyring_service->my_key_fetch_func(key_id, key_type, user_id, key, \
                                           key_len)
#define my_key_remove(key_id, user_id) \
  mysql_keyring_service->my_key_remove_func(key_id, user_id)
#define my_key_generate(key_id, key_type, user_id, key_len) \
  mysql_keyring_service->my_key_generate_func(key_id, key_type, user_id, \
                                              key_len)
#else

int my_key_store(const char *, const char *, const char *, const void *, size_t);
int my_key_fetch(const char *, char **, const char *, void **,
                 size_t *);
int my_key_remove(const char *, const char *);
int my_key_generate(const char *, const char *, const char *, size_t);

#endif

#ifdef __cplusplus
}
#endif

#endif //MYSQL_SERVICE_MYSQL_PLUGIN_KEYRING_INCLUDED

