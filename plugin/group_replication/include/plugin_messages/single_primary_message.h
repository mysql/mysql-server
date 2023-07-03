/* Copyright (c) 2016, 2023, Oracle and/or its affiliates.

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

#ifndef SINGLE_PRIMARY_MESSAGE_INCLUDED
#define SINGLE_PRIMARY_MESSAGE_INCLUDED

#include <set>
#include <string>
#include <vector>

#include "my_inttypes.h"
#include "plugin/group_replication/include/gcs_plugin_messages.h"
#include "plugin/group_replication/include/plugin_handlers/primary_election_include.h"

class Single_primary_message : public Plugin_gcs_message {
 public:
  enum enum_payload_item_type {
    // This type should not be used anywhere.
    PIT_UNKNOWN = 0,

    // Length of the payload item: 2 bytes
    PIT_SINGLE_PRIMARY_MESSAGE_TYPE = 1,

    // Uuid to elect: variable
    PIT_SINGLE_PRIMARY_SERVER_UUID = 2,

    // The election mode: 2 bytes
    PIT_SINGLE_PRIMARY_ELECTION_MODE = 3,

    // No valid type codes can appear after this one.
    PIT_MAX = 4
  };

  /**
   The several single primary type messages.
  */
  typedef enum {
    /**This type should not be used anywhere.*/
    SINGLE_PRIMARY_UNKNOWN = 0,
    /**A new primary was elected.*/
    SINGLE_PRIMARY_NEW_PRIMARY_MESSAGE = 1,
    /**Primary did apply queue after election.*/
    SINGLE_PRIMARY_QUEUE_APPLIED_MESSAGE = 2,
    /**No more forbidden transactions on multi master remain*/
    SINGLE_PRIMARY_NO_RESTRICTED_TRANSACTIONS = 3,
    /**Invoke a new election*/
    SINGLE_PRIMARY_PRIMARY_ELECTION = 4,
    /**The primary is now ready, processed all the backlog*/
    SINGLE_PRIMARY_PRIMARY_READY = 5,
    /**The member as set the read on the election context*/
    SINGLE_PRIMARY_READ_MODE_SET = 6,
    /**The end of the enum.*/
    SINGLE_PRIMARY_MESSAGE_TYPE_END = 7
  } Single_primary_message_type;

  /**
    Message constructor

    @param[in] type         the single primary message type
  */
  Single_primary_message(Single_primary_message_type type);

  /**
    Message constructor for primary election invocations

    @note this message will use type SINGLE_PRIMARY_PRIMARY_ELECTION

    @param primary_uuid  The primary uuid to elect
    @param election_mode The election mode to perform
  */
  Single_primary_message(std::string &primary_uuid,
                         enum_primary_election_mode election_mode);

  /**
    Message destructor
   */
  ~Single_primary_message() override;

  /**
    Message constructor for raw data

    @param[in] buf raw data
    @param[in] len raw length
  */
  Single_primary_message(const uchar *buf, size_t len);

  /** Returns this single primary message type */
  Single_primary_message_type get_single_primary_message_type() const {
    return single_primary_message_type;
  }

  /**
    @note This method should only be used with type
    SINGLE_PRIMARY_PRIMARY_ELECTION
    @return the primary uuid associated to this message
  */
  std::string &get_primary_uuid();

  /**
    @note This method should only be used with type
    SINGLE_PRIMARY_PRIMARY_ELECTION
    @return the election mode associated to this message
  */
  enum_primary_election_mode get_election_mode();

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
  /**The message type*/
  Single_primary_message_type single_primary_message_type;

  /** The uuid for the primary member */
  std::string primary_uuid;
  /** The mode for election requests */
  enum_primary_election_mode election_mode;
};

#endif /* SINGLE_PRIMARY_MESSAGE_INCLUDED */
