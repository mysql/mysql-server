/*
   Copyright (c) 2014, 2017, Oracle and/or its affiliates. All rights reserved.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef NDB_BINLOG_THREAD_H
#define NDB_BINLOG_THREAD_H

#include "sql/ndb_component.h"

class Ndb_binlog_thread : public Ndb_component
{
public:
  Ndb_binlog_thread();
  virtual ~Ndb_binlog_thread();
private:
  virtual int do_init() { return 0;}
  virtual void do_run();
  virtual int do_deinit() { return 0;}
  // Wake up for stop
  virtual void do_wakeup();

  /*
     The Ndb_binlog_thread is supposed to make a continuous recording
     of the activity in the cluster to the mysqlds binlog. When this
     recording is interrupted an incident event(aka. GAP event) is
     written to the binlog thus allowing consumers of the binlog to
     notice that the recording is most likely not continuous.
  */
  enum Reconnect_type {
    // Incident occured because the mysqld was stopped and
    // is now starting up again
    MYSQLD_STARTUP,
    // Incident occured because the mysqld was disconnected
    // from the cluster
    CLUSTER_DISCONNECT
  };
  bool check_reconnect_incident(THD* thd, class injector* inj,
                                Reconnect_type incident_id) const;

};

#endif
