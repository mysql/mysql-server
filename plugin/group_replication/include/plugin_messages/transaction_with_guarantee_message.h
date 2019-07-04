/* Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef TRANSACTION_WITH_GUARANTEE_MESSAGE_INCLUDED
#define TRANSACTION_WITH_GUARANTEE_MESSAGE_INCLUDED

#include <mysql/plugin_group_replication.h>

#include "plugin/group_replication/include/plugin_messages/transaction_message_interface.h"

/*
  @class Transaction_with_guarantee_message
  Class to convey the serialized contents of the TCLE, plus guarantees.
 */
class Transaction_with_guarantee_message
    : public Transaction_message_interface {
 public:
  enum enum_payload_item_type {
    // This type should not be used anywhere.
    PIT_UNKNOWN = 0,

    // Length of the payload item: variable
    PIT_TRANSACTION_DATA = 1,

    // Length of the payload item: 1 byte
    PIT_TRANSACTION_CONSISTENCY_LEVEL = 2,

    // No valid type codes can appear after this one.
    PIT_MAX = 3
  };

  /**
   Default constructor

   @param[in] consistency_level The transaction consistency level
   */
  Transaction_with_guarantee_message(
      enum_group_replication_consistency_level consistency_level);
  virtual ~Transaction_with_guarantee_message();

  /**
     Overrides Basic_ostream::write().
     Transaction_with_guarantee_message is a Basic_ostream. Callers can write
     data into Transaction_with_guarantee_message's data buffer though this
     method.

     @param[in] buffer  where the data will be read
     @param[in] length  the length of the data to write

     @return returns false if succeeds, otherwise true is returned.
  */
  bool write(const unsigned char *buffer, my_off_t length);

  /**
     Length of data in data vector

     @return data length
  */
  my_off_t length();

  /**
    Decode transaction consistency without unmarshal transaction data.

    @return the transaction consistency
  */
  static enum_group_replication_consistency_level
  decode_and_get_consistency_level(const unsigned char *buffer, size_t);

 protected:
  /*
   Implementation of the template methods
   */
  void encode_payload(std::vector<unsigned char> *buffer) const;
  void decode_payload(const unsigned char *buffer, const unsigned char *);

 private:
  std::vector<uchar> m_data;
  enum_group_replication_consistency_level m_consistency_level;
};

#endif /* TRANSACTION_WITH_GUARANTEE_MESSAGE_INCLUDED */
