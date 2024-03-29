/* Copyright (c) 2015, 2023, Oracle and/or its affiliates.

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

#include <assert.h>

#include "mysql/plugin.h"
#include "mysql/service_security_context.h"

my_svc_bool thd_get_security_context(MYSQL_THD, MYSQL_SECURITY_CONTEXT *) {
  assert(0);
  return 0;
}

my_svc_bool thd_set_security_context(MYSQL_THD, MYSQL_SECURITY_CONTEXT) {
  assert(0);
  return 0;
}

my_svc_bool security_context_create(MYSQL_SECURITY_CONTEXT *) {
  assert(0);
  return 0;
}

my_svc_bool security_context_destroy(MYSQL_SECURITY_CONTEXT) {
  assert(0);
  return 0;
}

my_svc_bool security_context_copy(MYSQL_SECURITY_CONTEXT,
                                  MYSQL_SECURITY_CONTEXT *) {
  assert(0);
  return 0;
}

my_svc_bool security_context_lookup(MYSQL_SECURITY_CONTEXT, const char *,
                                    const char *, const char *, const char *) {
  assert(0);
  return 0;
}

my_svc_bool security_context_get_option(MYSQL_SECURITY_CONTEXT, const char *,
                                        void *) {
  assert(0);
  return 0;
}

my_svc_bool security_context_set_option(MYSQL_SECURITY_CONTEXT, const char *,
                                        void *) {
  assert(0);
  return 0;
}
