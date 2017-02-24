/* Copyright (c) 2014, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef RECOVERY_MESSAGE_INCLUDED
#define RECOVERY_MESSAGE_INCLUDED

#include <string>
#include <set>
#include <vector>
#include "gcs_plugin_messages.h"

class Recovery_message : public Plugin_gcs_message
{
public:
  enum enum_payload_item_type
  {
    // This type should not be used anywhere.
    PIT_UNKNOWN= 0,

    // Length of the payload item: 2 bytes
    PIT_RECOVERY_MESSAGE_TYPE= 1,

    // Length of the payload item: variable
    PIT_MEMBER_UUID= 2,

    // No valid type codes can appear after this one.
    PIT_MAX= 3
  };

  /**
   The several recovery type messages.
  */
  typedef enum
  {
    /**This type should not be used anywhere.*/
    RECOVERY_UNKNOWN= 0,
    /**Recovery ended, member is online.*/
    RECOVERY_END_MESSAGE= 1,
    /**Donor transmitted all data (for future use)*/
    DONOR_FINISHED_MESSAGE= 2,
    /**The end of the enum.*/
    RECOVERY_MESSAGE_TYPE_END= 3
  } Recovery_message_type;

  /**
    Message constructor

    @param[in] type         the recovery message type
    @param[in] member_uuid  the origination member uuid
  */
  Recovery_message(Recovery_message_type type, const std::string& member_uuid);

  /**
    Message destructor
   */
  virtual ~Recovery_message();

  /**
    Message constructor for raw data

    @param[in] buf raw data
    @param[in] len raw length
  */
  Recovery_message(const uchar* buf, uint64 len);

  /** Returns this recovery message type */
  Recovery_message_type get_recovery_message_type()
  {
    return recovery_message_type;
  }

  /** Returns this message sender's uuid */
  const std::string& get_member_uuid()
  {
    return member_uuid;
  }

protected:
  /**
    Encodes the message contents for transmission.

    @param[out] buffer   the message buffer to be written
  */
  void encode_payload(std::vector<unsigned char>* buffer) const;

  /**
    Message decoding method

    @param[in] buffer the received data
    @param[in] end    the end of the buffer
  */
  void decode_payload(const unsigned char* buffer, const unsigned char* end);

private:
  /**The message type*/
  Recovery_message_type recovery_message_type;
  /**The member uuid where the message originated*/
  std::string member_uuid;
};

#endif /* RECOVERY_MESSAGE_INCLUDED */
