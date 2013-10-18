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

#ifndef GCS_COROSYNC_H
#define GCS_COROSYNC_H

#include <corosync/cpg.h>
#include <my_pthread.h>
#include <corosync/corotypes.h>
#include "gcs_stats.h"

namespace GCS
{

class Protocol_corosync : public Protocol
{
private:
  cpg_handle_t handle;
  pthread_t dispatcher_thd;
  Protocol_corosync(const Protocol_corosync&);

public:

  Stats& group_stats;
  Protocol_corosync(Stats& collector) :
    Protocol(), handle(0), dispatcher_thd(0), group_stats(collector) { };
  Protocol_type get_type() { return PROTO_COROSYNC; };
  ~Protocol_corosync() { /* delete from current_views; */ };
  bool open_session(Event_handlers* handlers);
  bool broadcast(const Message& msg);
  bool join(const std::string& group_name, enum_member_role role= MEMBER_ACTOR);
  bool leave(const string& group_name);

  bool close_session();
  /*
    The currently implemented Protocol session to joined groups association
    is 1 to 1.
    Therefore no attention to @c group_name arg is paid in the definition.
  */
  View& get_view() { return group_view; }
  View& get_view(const string& group_name) { return get_view(); }

  void test_me();
};

} // namespace

#endif
