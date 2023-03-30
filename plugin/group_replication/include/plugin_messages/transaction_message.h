/* Copyright (c) 2013, 2023, Oracle and/or its affiliates.

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

#ifndef TRANSACTION_MESSAGE_INCLUDED
#define TRANSACTION_MESSAGE_INCLUDED

#include "plugin/group_replication/include/plugin_messages/transaction_message_interface.h"

/*
  @class Transaction_message
  Class to convey the serialized contents of the TCLE
 */
class Transaction_message : public Transaction_message_interface {
 public:
  enum enum_payload_item_type {
    // This type should not be used anywhere.
    PIT_UNKNOWN = 0,

    // Length of the payload item: variable
    PIT_TRANSACTION_DATA = 1,

    // No valid type codes can appear after this one.
    PIT_MAX = 2
  };

  /**
   Constructor

   @param[in] payload_capacity  The transaction data size
   */
  Transaction_message(uint64_t payload_capacity);
  ~Transaction_message() override;

  /**
     Overrides Basic_ostream::write().
     Transaction_message is a Basic_ostream, which appends
     data into the a Gcs_message_data.

     @param[in] buffer  where the data will be read
     @param[in] length  the length of the data to write

     @return returns false if succeeds, otherwise true is returned.
  */
  bool write(const unsigned char *buffer, my_off_t length) override;

  /**
     Length of the message.

     @return message length
  */
  uint64_t length() override;

  /**
     Get the Gcs_message_data object, which contains the serialized
     transaction data.
     The internal Gcs_message_data is nullified, to avoid further usage
     of this Transaction object and the caller receives a pointer to the
     previously internal Gcs_message_data, which whom it is now responsible.

     @return the serialized transaction data in a Gcs_message_data object
  */
  Gcs_message_data *get_message_data_and_reset() override;

 protected:
  /*
   Implementation of the template methods
   */
  void encode_payload(std::vector<unsigned char> *buffer) const override;
  void decode_payload(const unsigned char *buffer,
                      const unsigned char *) override;

 private:
  Gcs_message_data *m_gcs_message_data{nullptr};
};

#endif /* TRANSACTION_MESSAGE_INCLUDED */
