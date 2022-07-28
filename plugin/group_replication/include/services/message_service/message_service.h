/* Copyright (c) 2019, 2022, Oracle and/or its affiliates.

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

#ifndef MESSAGE_SERVICE_H
#define MESSAGE_SERVICE_H

#include <mysql/components/service_implementation.h>
#include "plugin/group_replication/include/plugin_messages/group_service_message.h"
#include "plugin/group_replication/include/plugin_utils.h"

/**
  Register default send message service.
*/
bool register_gr_message_service_send();

/**
  Unregister default send message service.
*/
bool unregister_gr_message_service_send();

/**
  @class Message_service_handler
  Handles the deliver of recv service messages to subscribed modules.
*/
class Message_service_handler {
 public:
  /**
    Create a message service handler to deliver messages to recv subscribers.
  */
  Message_service_handler();

  virtual ~Message_service_handler();

  /**
     Initialize thread that will deliver messages.

     @return returns 0 if succeeds, error otherwise
  */
  int initialize();

  /**
     Main loop that checks message availability.
  */
  void dispatcher();

  /**
     Terminate delivering message thread.

     @return returns 0 if succeeds, error otherwise
  */
  int terminate();

  /**
     Add to queue a new message to be deliver to recv service subscribers.

     @param[in] message  message to be delivered
  */
  void add(Group_service_message *message);

  /**
    It will notify recv subscribers with a service message.

    @param[in] service_message  message to process

    @return false if message is delivered, true otherwise
  */
  bool notify_message_service_recv(Group_service_message *service_message);

 private:
  /** Thread was terminated */
  bool m_aborted;
  /** The current phase */
  my_thread_handle m_message_service_pthd;
  /** The thread lock to control access */
  mysql_mutex_t m_message_service_run_lock;
  /** The thread signal mechanism to be terminated */
  mysql_cond_t m_message_service_run_cond;
  /** The state of the thread. */
  thread_state m_message_service_thd_state;
  /** Queue with service message to be delivered */
  Abortable_synchronized_queue<Group_service_message *> *m_incoming;
};

#endif /* MESSAGE_SERVICE_H */
