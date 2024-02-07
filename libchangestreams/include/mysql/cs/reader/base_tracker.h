/* Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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

#ifndef CS_READER_BASE_TRACKER_INCLUDED
#define CS_READER_BASE_TRACKER_INCLUDED

#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <string>

#include "libchangestreams/include/mysql/cs/reader/state.h"

namespace cs::reader {

/**
 * @brief Parses the given stream entry and updates the state.
 *
 * This class has logics to parse the stream for relevant events and
 * update the state object provided. The natural flow is for stream
 * reader to read the next packet, then feed it to this updater.
 *
 * This class does not implement any concurrency control.
 */
class Base_tracker {
 public:
  Base_tracker() = default;
  virtual ~Base_tracker() = default;

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
  virtual bool track_and_update(std::shared_ptr<State> state,
                                const std::vector<uint8_t> &buffer) = 0;
};

}  // namespace cs::reader
#endif