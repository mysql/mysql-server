/* Copyright (c) 2018, 2023, Oracle and/or its affiliates.

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

#ifndef GROUP_ACTION_MESSAGE_INCLUDED
#define GROUP_ACTION_MESSAGE_INCLUDED

#include <string>

#include "my_inttypes.h"
#include "plugin/group_replication/include/gcs_plugin_messages.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_types.h"

class Group_action_message : public Plugin_gcs_message {
 public:
  enum enum_payload_item_type {
    // This type should not be used anywhere.
    PIT_UNKNOWN = 0,
    // Length of the payload item: 2 bytes
    PIT_ACTION_TYPE = 1,
    // Length of the payload item: 2 bytes
    PIT_ACTION_PHASE = 2,
    // Length of the payload item: 4 bytes
    PIT_ACTION_RETURN_VALUE = 3,
    // The uuid field
    PIT_ACTION_PRIMARY_ELECTION_UUID = 4,
    // The GCS protocol field: 2 bytes
    PIT_ACTION_SET_COMMUNICATION_PROTOCOL_VERSION = 5,
    // The running_transactions_timeout field: 4 bytes
    PIT_ACTION_TRANSACTION_MONITOR_TIMEOUT = 6,
    // The action initiator information: 4 bytes
    PIT_ACTION_INITIATOR = 7,
    // No valid type codes can appear after this one.
    PIT_MAX = 8
  };

  /** Enum for the types of message / actions */
  enum enum_action_message_type {
    // This type should not be used
    ACTION_UNKNOWN_MESSAGE = 0,
    // Change to multi primary
    ACTION_MULTI_PRIMARY_MESSAGE = 1,
    // Elect/Change mode to primary member
    ACTION_PRIMARY_ELECTION_MESSAGE = 2,
    // Change GCS protocol version
    ACTION_SET_COMMUNICATION_PROTOCOL_MESSAGE = 3,
    // The end of the enum
    ACTION_MESSAGE_END = 4
  };
  /** Enum for the phase of the action in the message */
  enum enum_action_message_phase {
    ACTION_UNKNOWN_PHASE = 0,  // This type should not be used
    ACTION_START_PHASE = 1,    // Start a new action
    ACTION_END_PHASE = 2,      // The action was ended
    ACTION_ABORT_PHASE = 3,    // The action was aborted
    ACTION_PHASE_END = 4,      // The enum end
  };

  /** Enum to identify initiator and action. */
  enum enum_action_initiator_and_action {
    // This type should not be used
    ACTION_INITIATOR_UNKNOWN = 0,  // to match with ACTION_UNKNOWN_MESSAGE
    // Change to multi primary
    ACTION_UDF_SWITCH_TO_MULTI_PRIMARY_MODE = 1,
    // Change primary
    ACTION_UDF_SET_PRIMARY = 2,
    // Change to single primary
    ACTION_UDF_SWITCH_TO_SINGLE_PRIMARY_MODE = 3,
    // Change to single primary with UUID
    ACTION_UDF_SWITCH_TO_SINGLE_PRIMARY_MODE_UUID = 4,
    // Change GCS protocol version
    ACTION_UDF_COMMUNICATION_PROTOCOL_MESSAGE = 5,
    // The end of the enum
    ACTION_INITIATOR_END = 6
  };

  /**
    Message constructor
  */
  Group_action_message();

  /**
    Message constructor

    @param[in] type         the action message type
  */
  Group_action_message(enum_action_message_type type);

  /**
    Message constructor for ACTION_PRIMARY_ELECTION_MESSAGE action type

    @param[in] primary_uuid   the primary uuid to elect
    @param[in] transaction_monitor_timeout_arg The number of seconds to wait
    before setting the kill flag for the transactions that did not reach commit
    stage
  */
  Group_action_message(std::string &primary_uuid,
                       int32 &transaction_monitor_timeout_arg);

  /**
    Message constructor for ACTION_SET_COMMUNICATION_PROTOCOL_MESSAGE action
    type

    @param[in] gcs_protocol   the GCS protocol to change to
   */
  explicit Group_action_message(Gcs_protocol_version gcs_protocol);

  /**
    Message destructor
   */
  ~Group_action_message() override;

  /**
    Message constructor for raw data

    @param[in] buf raw data
    @param[in] len raw length
  */
  Group_action_message(const uchar *buf, size_t len);

  /** Returns this group action message type */
  enum_action_message_type get_group_action_message_type() {
    return group_action_type;
  }

  /** Returns this group action message phase */
  enum_action_message_phase get_group_action_message_phase() {
    return group_action_phase;
  }

  void set_group_action_message_phase(enum_action_message_phase phase) {
    group_action_phase = phase;
  }

  /**
    Set the return value for this message
    @param return_value_arg the value to set
  */
  void set_return_value(int return_value_arg) {
    return_value = return_value_arg;
  }

  /**
    Set the action initiator.
    @param initiator Identifier for Group action initiator
  */
  void set_action_initiator(const enum_action_initiator_and_action initiator) {
    m_action_initiator = initiator;
  }

  /**
    @return Identifier for Group action initiator
  */
  const enum_action_initiator_and_action &get_action_initiator() {
    return m_action_initiator;
  }

  /**
    @return The return value associated to this message.
  */
  int32 get_return_value() { return return_value; }

  /**
    Check what is the action that this message encodes from a buffer
    @param buf the raw data buffer
    @return If the message is a primary election action or other
  */
  static enum_action_message_type get_action_type(const uchar *buf);

  /**
    Returns this group action primary to be elected uuid.

    @return the primary to be elected uuid, which can be empty
  */
  const std::string &get_primary_to_elect_uuid();

  /**
    Returns the GCS protocol this group action wants the group to change to.

    @return the GCS protocol version
   */
  Gcs_protocol_version const &get_gcs_protocol();

  /**
    Returns the running_transactions_timeout.

    @return the running_transactions_timeout value
   */
  int32 get_transaction_monitor_timeout();

 protected:
  /**
    Encodes the message contents for transmission.

    @param[out] buffer   the message buffer to be written
  */
  void encode_payload(std::vector<unsigned char> *buffer) const override;

  /**
    Message decoding method

    @param[in] buffer the received data
    @param[in] end    the end of the buffer.
  */
  void decode_payload(const unsigned char *buffer,
                      const unsigned char *end) override;

 private:
  /** The action type for this message */
  enum_action_message_type group_action_type;

  /** If it is a start, stop or other message */
  enum_action_message_phase group_action_phase;

  /** Is there any return value associated to this action */
  int32 return_value;

  /* Option Values */

  /** The uuid for election, can be empty if not defined */
  std::string primary_election_uuid;

  /** The GCS protocol version to change to */
  Gcs_protocol_version gcs_protocol;
  /**
    The number of seconds to wait before setting the kill flag for the
    transactions that did not reach commit stage.
  */
  int32 m_transaction_monitor_timeout{-1};
  /** Group action identifier */
  enum_action_initiator_and_action m_action_initiator;
};

#endif /* GROUP_ACTION_MESSAGE_INCLUDED */
