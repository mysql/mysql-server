/* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02111-1307  USA */

#include <mysql/components/component_implementation.h>
#include <mysql/components/service.h>
#include <mysql/components/service_implementation.h>
#include <mysql/components/services/mysql_cond_service.h>
#include <mysql/psi/mysql_cond.h>

void
impl_mysql_cond_register(const char *category, PSI_cond_info *info, int count)
{
  mysql_cond_register(category, info, count);
}

int
impl_mysql_cond_init(
  PSI_cond_key key,
  mysql_cond_t *that,
  const char *src_file,
  unsigned int src_line)
{
  return mysql_cond_init_with_src(key, that, src_file, src_line);
}

int
impl_mysql_cond_destroy(
  mysql_cond_t *that,
  const char *src_file,
  unsigned int src_line)
{
  return mysql_cond_destroy_with_src(that, src_file, src_line);
}

int
impl_mysql_cond_wait(
  mysql_cond_t *cond,
  mysql_mutex_t *mutex,
  const char *src_file,
  unsigned int src_line)
{
  return mysql_cond_wait_with_src(cond, mutex, src_file, src_line);
}

int
impl_mysql_cond_timedwait(
  mysql_cond_t *cond,
  mysql_mutex_t *mutex,
  const struct timespec *abstime,
  const char *src_file,
  unsigned int src_line)
{
  return mysql_cond_timedwait_with_src(cond, mutex, abstime, src_file, src_line);
}

int
impl_mysql_cond_signal(
  mysql_cond_t *that,
  const char *src_file,
  unsigned int src_line)
{
  return mysql_cond_signal_with_src(that, src_file, src_line);
}

int
impl_mysql_cond_broadcast(
  mysql_cond_t *that,
  const char *src_file,
  unsigned int src_line)
{
  return mysql_cond_broadcast_with_src(that, src_file, src_line);
}

extern SERVICE_TYPE(mysql_cond_v1) SERVICE_IMPLEMENTATION(mysql_server, mysql_cond_v1);

SERVICE_TYPE(mysql_cond_v1) SERVICE_IMPLEMENTATION(mysql_server, mysql_cond_v1) =
{
  impl_mysql_cond_register,
  impl_mysql_cond_init,
  impl_mysql_cond_destroy,
  impl_mysql_cond_wait,
  impl_mysql_cond_timedwait,
  impl_mysql_cond_signal,
  impl_mysql_cond_broadcast
};

