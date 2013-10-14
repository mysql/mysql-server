/* Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.

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

#ifndef SQL_THREAD_APPLIER_INCLUDE
#define SQL_THREAD_APPLIER_INCLUDE

#include "../gcs_applier.h"
#include <gcs_replication.h>
#include <applier_interfaces.h>
#include <rpl_rli.h>

class Applier_sql_thread : public EventHandler
{
public:
  Applier_sql_thread();
  int handle(PipelineEvent *ev,Continuation* cont);
  int initialize();
  int terminate();
  bool is_unique();
  Handler_role get_role();

private:

  int initialize_sql_thread();
  int terminate_sql_thread();

  Master_info *mi;
  Relay_log_info *rli;
};

#endif /* SQL_THREAD_APPLIER_INCLUDE */
