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

#ifndef GROUP_VALIDATION_MESSAGE_INCLUDED
#define GROUP_VALIDATION_MESSAGE_INCLUDED

#include "my_inttypes.h"
#include "plugin/group_replication/include/gcs_plugin_messages.h"

/** The base message for group validation messages */
class Group_validation_message : public Plugin_gcs_message {
 public:
  /** Enum for message payload */
  enum enum_payload_item_type {
    PIT_UNKNOWN = 0,             // Not used
    PIT_VALIDATION_TYPE = 1,     // The validation type, length: 2 bytes
    PIT_VALIDATION_CHANNEL = 2,  // The member has channel flag, length: 1 bytes
    PIT_MEMBER_WEIGHT = 3,       // The member weight, length: 2 bytes
    PIT_SENT_TIMESTAMP = 4,      // Length: 8 bytes
    PIT_MAX  // No valid type codes can appear after this one
  };

  /** Enum for the types of validation action **/
  enum enum_validation_message_type {
    GROUP_VALIDATION_UNKNOWN_MESSAGE = 0,  // Reserved type
    ELECTION_VALIDATION_MESSAGE = 1,       // Member info for elections
    GROUP_VALIDATION_MESSAGE_END = 2       // Enum end value
  };

  /**
    Class constructor
    @param has_channels This member has running slave channels?
    @param member_weight_arg The election weight of this member
  */
  Group_validation_message(bool has_channels, uint member_weight_arg);

  /**
    Message constructor for raw data

    @param[in] buf raw data
    @param[in] len raw length
  */
  Group_validation_message(const uchar *buf, size_t len);

  /** Class destructor */
  ~Group_validation_message() override;

  /**
    Does the member has running channels
    @return true if yes, false otherwise
  */
  bool has_slave_channels() const;

  /**
    The election weight of this member
    @return The member weight
  */
  uint get_member_weight() const;

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
  enum_validation_message_type group_validation_message_type;

  /** Does the member has channels? */
  bool has_channels;

  /** The member election weight */
  uint member_weight;
};

#endif /* GROUP_VALIDATION_MESSAGE_INCLUDED */
