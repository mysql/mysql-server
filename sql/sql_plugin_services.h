/* Copyright (c) 2009, 2011, Oracle and/or its affiliates. All rights reserved.

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

/* support for Services */
#include <service_versions.h>

struct st_service_ref {
  const char *name;
  uint version;
  void *service;
};

static struct my_snprintf_service_st my_snprintf_handler = {
  my_snprintf,
  my_vsnprintf
};

static struct thd_alloc_service_st thd_alloc_handler= {
  thd_alloc,
  thd_calloc,
  thd_strdup,
  thd_strmake,
  thd_memdup,
  thd_make_lex_string
};

static struct thd_wait_service_st thd_wait_handler= {
  thd_wait_begin,
  thd_wait_end
};

static struct my_thread_scheduler_service my_thread_scheduler_handler= {
  my_thread_scheduler_set,
  my_thread_scheduler_reset,
};

static struct my_plugin_log_service my_plugin_log_handler= {
  my_plugin_log_message
};

static struct mysql_string_service_st mysql_string_handler= {
  mysql_string_convert_to_char_ptr,
  mysql_string_get_iterator,
  mysql_string_iterator_next,
  mysql_string_iterator_isupper,
  mysql_string_iterator_islower,
  mysql_string_iterator_isdigit,
  mysql_string_to_lowercase,
  mysql_string_free,
  mysql_string_iterator_free,
};

static struct st_service_ref list_of_services[]=
{
  { "my_snprintf_service", VERSION_my_snprintf, &my_snprintf_handler },
  { "thd_alloc_service",   VERSION_thd_alloc,   &thd_alloc_handler },
  { "thd_wait_service",    VERSION_thd_wait,    &thd_wait_handler },
  { "my_thread_scheduler_service",
    VERSION_my_thread_scheduler, &my_thread_scheduler_handler },
  { "my_plugin_log_service", VERSION_my_plugin_log, &my_plugin_log_handler },
  { "mysql_string_service",
    VERSION_mysql_string, &mysql_string_handler },
};

