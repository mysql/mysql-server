/* Copyright (c) 2014, 2024, Oracle and/or its affiliates.

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

#ifndef RECOVERY_MESSAGE_INCLUDED
#define RECOVERY_MESSAGE_INCLUDED

#include <set>
#include <string>
#include <vector>

#include "my_inttypes.h"
#include "plugin/group_replication/include/gcs_plugin_messages.h"

class Recovery_message : public Plugin_gcs_message {
 public:
  enum enum_payload_item_type {
    // This type should not be used anywhere.
    PIT_UNKNOWN = 0,

    // Length of the payload item: 2 bytes
    PIT_RECOVERY_MESSAGE_TYPE = 1,

    // Length of the payload item: variable
    PIT_MEMBER_UUID = 2,

    // Length of the payload item: 8 bytes
    PIT_SENT_TIMESTAMP = 3,

    // No valid type codes can appear after this one.
    PIT_MAX = 4
  };

  /**
   The several recovery type messages.
  */
  typedef enum {
    /**This type should not be used anywhere.*/
    RECOVERY_UNKNOWN = 0,
    /**Recovery ended, member is online.*/
    RECOVERY_END_MESSAGE = 1,
    /**Donor transmitted all data (for future use)*/
    DONOR_FINISHED_MESSAGE = 2,
    /**The end of the enum.*/
    RECOVERY_MESSAGE_TYPE_END = 3
  } Recovery_message_type;

  /**
    Message constructor

    @param[in] type         the recovery message type
    @param[in] member_uuid  the origination member uuid
  */
  Recovery_message(Recovery_message_type type, const std::string &member_uuid);

  /**
    Message destructor
   */
  ~Recovery_message() override;

  /**
    Message constructor for raw data

    @param[in] buf raw data
    @param[in] len raw length
  */
  Recovery_message(const uchar *buf, size_t len);

  /** Returns this recovery message type */
  Recovery_message_type get_recovery_message_type() {
    return recovery_message_type;
  }

  /** Returns this message sender's uuid */
  const std::string &get_member_uuid() { return member_uuid; }

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
  /**
    Encodes the message contents for transmission.

    @param[out] buffer   the message buffer to be written
  */
  void encode_payload(std::vector<unsigned char> *buffer) const override;

  /**
    Message decoding method

    @param[in] buffer the received data
  */
  void decode_payload(const unsigned char *buffer,
                      const unsigned char *) override;

 private:
  /**The message type*/
  Recovery_message_type recovery_message_type;
  /**The member uuid where the message originated*/
  std::string member_uuid;
};

#endif /* RECOVERY_MESSAGE_INCLUDED */
