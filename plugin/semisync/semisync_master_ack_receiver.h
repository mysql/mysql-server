/* Copyright (c) 2014, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef SEMISYNC_MASTER_ACK_RECEIVER_DEFINED
#define SEMISYNC_MASTER_ACK_RECEIVER_DEFINED

#include <sys/types.h>
#include <vector>

#include "my_inttypes.h"
#include "my_io.h"
#include "my_thread.h"
#include "plugin/semisync/semisync.h"
#include "plugin/semisync/semisync_master.h"
#include "sql/sql_class.h"

struct Slave {
  THD *thd;
  Vio *vio;

  my_socket sock_fd() const { return vio->mysql_socket.fd; }
  uint server_id() const { return thd->server_id; }
};

typedef std::vector<Slave> Slave_vector;
typedef Slave_vector::iterator Slave_vector_it;

/**
  Ack_receiver is responsible to control ack receive thread and maintain
  slave information used by ack receive thread.

  There are mainly four operations on ack receive thread:
  start: start ack receive thread
  stop: stop ack receive thread
  add_slave: maintain a new semisync slave's information
  remove_slave: remove a semisync slave's information
 */
class Ack_receiver : public ReplSemiSyncBase {
 public:
  Ack_receiver();
  ~Ack_receiver();

  /**
     Notify ack receiver to receive acks on the dump session.

     It adds the given dump thread into the slave list and wakes
     up ack thread if it is waiting for any slave coming.

     @param[in] thd  THD of a dump thread.

     @return it return false if succeeds, otherwise true is returned.
  */
  bool add_slave(THD *thd);

  /**
    Notify ack receiver not to receive ack on the dump session.

    it removes the given dump thread from slave list.

    @param[in] thd  THD of a dump thread.
  */
  void remove_slave(THD *thd);

  /**
    Start ack receive thread

    @return it return false if succeeds, otherwise true is returned.
  */
  bool start();

  /**
     Stop ack receive thread
  */
  void stop();

  /**
     The core of ack receive thread.

     It monitors all slaves' sockets and receives acks when they come.
  */
  void run();

  void setTraceLevel(unsigned long trace_level) { trace_level_ = trace_level; }

  bool init() {
    setTraceLevel(rpl_semi_sync_master_trace_level);
    if (rpl_semi_sync_master_enabled) return start();
    return false;
  }

 private:
  enum status { ST_UP, ST_DOWN, ST_STOPPING };
  uint8 m_status;
  /*
    Protect m_status, m_slaves_changed and m_slaves. ack thread and other
    session may access the variables at the same time.
  */
  mysql_mutex_t m_mutex;
  mysql_cond_t m_cond;
  /* If slave list is updated(add or remove). */
  bool m_slaves_changed;
  Slave_vector m_slaves;
  my_thread_handle m_pid;

  /* Declare them private, so no one can copy the object. */
  Ack_receiver(const Ack_receiver &ack_receiver);
  Ack_receiver &operator=(const Ack_receiver &ack_receiver);

  void set_stage_info(const PSI_stage_info &stage);
  void wait_for_slave_connection();
};

extern Ack_receiver *ack_receiver;
#endif
