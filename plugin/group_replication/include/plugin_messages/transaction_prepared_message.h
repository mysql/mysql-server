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

    // Length of the payload item: 8 bytes
    PIT_SENT_TIMESTAMP = 3,

    // Length of the payload item: 1-32
    // Optional item.
    PIT_TRANSACTION_PREPARED_TAG = 4,

    // No valid type codes can appear after this one.
    PIT_MAX = 5
  };

  /**
   Message constructor

   @param[in]  tsid             the prepared transaction tsid
   @param[in]  is_tsid_specified information on whether tsid is specified
   @param[in]  gno              the prepared transaction gno
  */
  Transaction_prepared_message(const gr::Gtid_tsid &tsid,
                               bool is_tsid_specified, rpl_gno gno);

  /**
   Message decode constructor

   @param[in]  buf              message buffer
   @param[in]  len              message buffer length
  */
  Transaction_prepared_message(const unsigned char *buf, size_t len);
  ~Transaction_prepared_message() override;

  rpl_gno get_gno();

  /**
    Return the time at which the message contained in the buffer was sent.
    @see Metrics_handler::get_current_time()

    @param[in] buffer            the buffer to decode from.
    @param[in] length            the buffer length

    @return the time on which the message was sent.
  */
  static uint64_t get_sent_timestamp(const unsigned char *buffer,
                                     size_t length);

  /// @brief returns information on whether TSID is specified for this trx
  /// @return information on whether TSID is specified for this trx
  bool is_tsid_specified() const { return m_tsid_specified; }

  /// @brief TSID accessor
  /// @return Const reference to transaction TSID
  const gr::Gtid_tsid &get_tsid();

  using Error_ptr = mysql::utils::Error_ptr;

  /// @brief Checks whether message encoding/decoding succeeded
  /// @return Message validity
  bool is_valid() const;

  /// @brief Gets information about decoding/encoding error
  /// @return Const reference to decoding/encoding error information
  const Error_ptr &get_error() const;

 protected:
  /*
   Implementation of the template methods
   */
  void encode_payload(std::vector<unsigned char> *buffer) const override;
  void decode_payload(const unsigned char *buffer,
                      const unsigned char *end) override;

 private:
  bool m_tsid_specified;
  rpl_gno m_gno;
  gr::Gtid_tsid m_tsid;
  /// Holds information about error that might occur during encoding/decoding
  Error_ptr m_error;
};

#endif /* TRANSACTION_PREPARED_MESSAGE_INCLUDED */
