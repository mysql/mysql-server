#ifndef MYSQL_SERVICES_INCLUDED
/* Copyright (c) 2009, 2015, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifdef __cplusplus
extern "C" {
#endif

#include <mysql/service_my_snprintf.h>
#include <mysql/service_thd_alloc.h>
#include <mysql/service_thd_wait.h>
#include <mysql/service_thread_scheduler.h>
#include <mysql/service_my_plugin_log.h>
#include <mysql/service_mysql_string.h>
#include <mysql/service_mysql_alloc.h>
#include <mysql/service_mysql_password_policy.h>
#include <mysql/service_parser.h>
#include <mysql/service_rpl_transaction_ctx.h>
#include <mysql/service_rpl_transaction_write_set.h>
#include <mysql/service_locking.h>

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
#include <mysql/service_rules_table.h>
#endif

#define MYSQL_SERVICES_INCLUDED
#endif /* MYSQL_SERVICES_INCLUDED */
