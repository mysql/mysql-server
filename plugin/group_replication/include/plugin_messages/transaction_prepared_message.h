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

#ifndef TRANSACTION_PREPARED_MESSAGE_INCLUDED
#define TRANSACTION_PREPARED_MESSAGE_INCLUDED

#include <mysql/group_replication_priv.h>
#include <vector>

#include "my_inttypes.h"
#include "plugin/group_replication/include/gcs_plugin_messages.h"

/*
  @class Transaction_prepared_message
 */
class Transaction_prepared_message : public Plugin_gcs_message {
 public:
  enum enum_payload_item_type {
    // This type should not be used anywhere.
    PIT_UNKNOWN = 0,

    // Length of the payload item: 8 bytes
    PIT_TRANSACTION_PREPARED_GNO = 1,

    // Length of the payload item: 16 bytes.
    // Optional item.
    PIT_TRANSACTION_PREPARED_SID = 2,

    // No valid type codes can appear after this one.
    PIT_MAX = 3
  };

  /**
   Message constructor

   @param[in]  sid              the prepared transaction sid
   @param[in]  gno              the prepared transaction gno
  */
  Transaction_prepared_message(const rpl_sid *sid, rpl_gno gno);

  /**
   Message decode constructor

   @param[in]  buf              message buffer
   @param[in]  len              message buffer length
  */
  Transaction_prepared_message(const unsigned char *buf, size_t len);
  ~Transaction_prepared_message() override;

  const rpl_sid *get_sid();

  rpl_gno get_gno();

 protected:
  /*
   Implementation of the template methods
   */
  void encode_payload(std::vector<unsigned char> *buffer) const override;
  void decode_payload(const unsigned char *buffer,
                      const unsigned char *end) override;

 private:
  bool m_sid_specified;
  rpl_sid m_sid;
  rpl_gno m_gno;
};

#endif /* TRANSACTION_PREPARED_MESSAGE_INCLUDED */
