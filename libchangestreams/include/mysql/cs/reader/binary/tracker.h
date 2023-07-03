/* Copyright (c) 2021, 2023, Oracle and/or its affiliates.

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

#ifndef CS_READER_BINARY_TRACKER_INCLUDED
#define CS_READER_BINARY_TRACKER_INCLUDED

#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include "libbinlogevents/include/trx_boundary_parser.h"
#include "libchangestreams/include/mysql/cs/reader/base_tracker.h"
#include "libchangestreams/include/mysql/cs/reader/state.h"

namespace cs::reader::binary {

/**
 * @brief Parses the given stream entry and updates the state.
 *
 * This class has logics to parse the stream for relevant events and
 * update the state object provided. The natural flow is for stream
 * reader to read the next packet, then feed it to this updater.
 *
 * This class does not implement any concurrency control.
 */
class Tracker : public cs::reader::Base_tracker {
 protected:
  std::unique_ptr<binary_log::Format_description_event> m_fde{};
  std::string m_current_gtid_event_buffer;
  Transaction_boundary_parser m_trx_boundary_parser{
      Transaction_boundary_parser::TRX_BOUNDARY_PARSER_RECEIVER};

 private:
  Tracker &operator=(const Tracker &) = delete;
  Tracker(Tracker &) = delete;

 public:
  Tracker();
  virtual ~Tracker() override = default;

  /**
   * @brief Updates the state given the contents of the buffer, if needed.
   *
   * This member function will check transaction boundaries if a transaction
   * is terminated it will add its identifier to the list of transactions
   * in the state object.
   *
   * @param state The state to be updated
   * @param buffer The raw bytes read from the stream.
   * @return true if there was an error.
   * @return false if there was no error.
   */
  bool track_and_update(std::shared_ptr<State> state,
                        const std::vector<uint8_t> &buffer) override;
};

}  // namespace cs::reader::binary
#endif