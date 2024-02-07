/* Copyright (c) 2018, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef SYNC_BEFORE_EXECUTION_MESSAGE_INCLUDED
#define SYNC_BEFORE_EXECUTION_MESSAGE_INCLUDED

#include <mysql/group_replication_priv.h>
#include <vector>

#include "my_inttypes.h"
#include "plugin/group_replication/include/gcs_plugin_messages.h"

/*
  @class Sync_before_execution_message
 */
class Sync_before_execution_message : public Plugin_gcs_message {
 public:
  enum enum_payload_item_type {
    // This type should not be used anywhere.
    PIT_UNKNOWN = 0,

    // Length of the payload item: 4 bytes
    PIT_MY_THREAD_ID = 1,

    // Length of the payload item: 8 bytes
    PIT_SENT_TIMESTAMP = 2,

    // No valid type codes can appear after this one.
    PIT_MAX = 3
  };

  /**
   Message constructor

   @param[in]  thread_id        the thread that did the request
  */
  Sync_before_execution_message(my_thread_id thread_id);

  /**
   Message decode constructor

   @param[in]  buf              message buffer
   @param[in]  len              message buffer length
  */
  Sync_before_execution_message(const unsigned char *buf, size_t len);
  ~Sync_before_execution_message() override;

  my_thread_id get_thread_id();

  /**
    Return the time at which the message contained in the buffer was sent.
    @see Metrics_handler::get_current_time()

    @param[in] buffer            the buffer to decode from.
    @param[in] length            the buffer length

    @return the time on which the message was sent.
  */
  static uint64_t get_sent_timestamp(const unsigned char *buffer,
                                     size_t length);

 protected:
  /*
   Implementation of the template methods
  */
  void encode_payload(std::vector<unsigned char> *buffer) const override;
  void decode_payload(const unsigned char *buffer,
                      const unsigned char *end) override;

 private:
  my_thread_id m_thread_id;
};

#endif /* SYNC_BEFORE_EXECUTION_MESSAGE_INCLUDED */
