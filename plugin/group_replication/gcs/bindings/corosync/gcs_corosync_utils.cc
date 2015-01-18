/* Copyright (c) 2014, 2015, Oracle and/or its affiliates. All rights reserved.

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

#include "gcs_corosync_utils.h"


Gcs_corosync_utils::~Gcs_corosync_utils()
{
}

Gcs_member_identifier*
Gcs_corosync_utils::build_corosync_member_id(uint32 local_node_id,
                                             int pid)
{
  ostringstream builder;

  builder << local_node_id << ":" << pid;

  string member_id_str(builder.str());

  return new Gcs_member_identifier(member_id_str);
}

Gcs_corosync_view_change_control::Gcs_corosync_view_change_control()
{
  view_changing= false;

  pthread_cond_init(&wait_for_view_cond, NULL);
  pthread_mutex_init(&wait_for_view_mutex, NULL);
}
Gcs_corosync_view_change_control::~Gcs_corosync_view_change_control()
{
  pthread_mutex_destroy(&wait_for_view_mutex);
  pthread_cond_destroy(&wait_for_view_cond);
}

void
Gcs_corosync_view_change_control::start_view_exchange()
{
  pthread_mutex_lock(&wait_for_view_mutex);
  view_changing= true;
  pthread_mutex_unlock(&wait_for_view_mutex);
}

void
Gcs_corosync_view_change_control::end_view_exchange()
{
  pthread_mutex_lock(&wait_for_view_mutex);
  view_changing= false;
  pthread_cond_broadcast(&wait_for_view_cond);
  pthread_mutex_unlock(&wait_for_view_mutex);
}

void
Gcs_corosync_view_change_control::wait_for_view_change_end()
{
  pthread_mutex_lock(&wait_for_view_mutex);
  while (view_changing)
    pthread_cond_wait(&wait_for_view_cond, &wait_for_view_mutex);
  pthread_mutex_unlock(&wait_for_view_mutex);
}

