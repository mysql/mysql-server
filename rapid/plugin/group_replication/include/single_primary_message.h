/* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef SINGLE_PRIMARY_MESSAGE_INCLUDED
#define SINGLE_PRIMARY_MESSAGE INCLUDED

#include <string>
#include <set>
#include <vector>
#include "gcs_plugin_messages.h"

class Single_primary_message : public Plugin_gcs_message
{
public:
  enum enum_payload_item_type
  {
    // This type should not be used anywhere.
    PIT_UNKNOWN= 0,

    // Length of the payload item: 2 bytes
    PIT_SINGLE_PRIMARY_MESSAGE_TYPE= 1,

    // No valid type codes can appear after this one.
    PIT_MAX= 2
  };

  /**
   The several single primary type messages.
  */
  typedef enum
  {
    /**This type should not be used anywhere.*/
    SINGLE_PRIMARY_UNKNOWN= 0,
    /**A new primary was elected.*/
    SINGLE_PRIMARY_NEW_PRIMARY_MESSAGE= 1,
    /**Primary did apply queue after election.*/
    SINGLE_PRIMARY_QUEUE_APPLIED_MESSAGE= 2,
    /**The end of the enum.*/
    SINGLE_PRIMARY_MESSAGE_TYPE_END= 3
  } Single_primary_message_type;

  /**
    Message constructor

    @param[in] type         the single primary message type
  */
  Single_primary_message(Single_primary_message_type type);

  /**
    Message destructor
   */
  virtual ~Single_primary_message();

  /**
    Message constructor for raw data

    @param[in] buf raw data
    @param[in] len raw length
  */
  Single_primary_message(const uchar* buf, uint64 len);

  /** Returns this single primary message type */
  Single_primary_message_type get_single_primary_message_type()
  {
    return single_primary_message_type;
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
    @param[in] end    the end of buffer
  */
  void decode_payload(const unsigned char* buffer,
                      const unsigned char* end);

private:
  /**The message type*/
  Single_primary_message_type single_primary_message_type;
};

#endif /* SINGLE_PRIMARY_MESSAGE_INCLUDED */
