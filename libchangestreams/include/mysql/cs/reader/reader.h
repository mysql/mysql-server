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

#ifndef CS_READER_INCLUDED
#define CS_READER_INCLUDED

#include <memory>
#include <vector>
#include "libchangestreams/include/mysql/cs/reader/state.h"

namespace cs::reader {

class Reader {
 public:
  Reader() = default;
  virtual ~Reader() = default;

  /**
   * @brief Opens the stream.
   *
   * If a pointer of the state is given, that context is used to set the stream
   * up. The state will continue to be updated.
   *
   * @param state The state to be used in the connection.
   * @return true if there was an error during the connection.
   * @return false if the connection was successful.
   */
  virtual bool open(std::shared_ptr<State> state) = 0;

  /**
   * @brief Closes the stream.
   *
   * @return true if there was an error while disconnecting.
   * @return false if the disconnection was successful.
   */
  virtual bool close() = 0;

  /**
   * Reads the next entry in the stream into the vector provided.
   *
   * @param next the buffer to put the next event contents in.
   * @return true if there was an error.
   * @return false if the getting the next event was successful.
   */
  virtual bool read(std::vector<uint8_t> &next) = 0;

  /**
   * @brief Get the state object.
   *
   * The state object is accessible until the stream is destroyed. I.e., it
   * outlives the connection itself. This gives the caller the opportunity
   * to copy the object and use it the state of a new stream connection when
   * this one is already disconnected.
   *
   * @return the state of the stream at this point in time. Returns nullptr if
   * the stream has not been opened yet.
   */
  virtual std::shared_ptr<State> get_state() const = 0;
};

}  // namespace cs::reader

#endif
