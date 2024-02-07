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

#ifndef CS_READER_BINARY_MYSQL_PROTOCOL_INCLUDED
#define CS_READER_BINARY_MYSQL_PROTOCOL_INCLUDED

#include <string>
#include <vector>

#include "libchangestreams/include/mysql/cs/reader/binary/tracker.h"
#include "libchangestreams/include/mysql/cs/reader/reader.h"
#include "libchangestreams/include/mysql/cs/reader/state.h"
#include "mysql.h"
#include "mysql/binlog/event/binlog_event.h"
#include "mysql/binlog/event/control_events.h"

namespace cs::reader::binary {

/**
 * @brief Connector that relies on the MySQL protocol to connect to the
 * Change Stream source.
 *
 * This is not thread-safe.
 */
class Mysql_protocol : public cs::reader::Reader {
 public:
  constexpr static uint32_t COM_BINLOG_DUMP_FLAG_NON_BLOCKING{1 << 0};

 protected:
  bool m_is_binlog_conn_open{false};
  cs::reader::binary::Tracker m_tracker;
  std::shared_ptr<cs::reader::State> m_state;
  MYSQL *m_mysql{nullptr};
  MYSQL_RPL m_rpl_ctx;
  uint32_t m_server_id{0};
  uint32_t m_flags{0};

 private:
  void reset();
  bool setup();
  bool encode_gtid_set_to_mysql_protocol(const mysql::gtid::Gtid_set &gtid_set,
                                         std::string &output) const;

 public:
  /**
   * @brief Construct a new Mysql_protocol object.
   *
   * This constructor takes a MYSQL connection handle, a server_id and flags.
   *
   * @param mysql the MySQL connection handle.
   * @param server_id the server id that this connection will identify itself
   * with.
   * @param flags replication handshake flags.
   */
  Mysql_protocol(MYSQL *mysql, uint32_t server_id, uint32_t flags);

  /**
   * @brief Destroy this object and the properties it owns.
   *
   * Deleting this object will NOT delete the MYSQL connection handle but will
   * delete the connection state.
   */
  virtual ~Mysql_protocol() override;

  /**
   * @brief This member function closes the connection and the stream.
   *
   * This member function shall close the MYSQL connection handle (but not
   * destroy it). This member function will not destroy the stream state, thence
   * you can still get access to it after calling this member function.
   *
   * @returns true if the disconnection hit a problem, false otherwise.
   */
  bool close() override;

  /**
   * @brief This member function attaches this connector to the stream.
   *
   * Note that to attach to the stream, this connector needs to be provided a
   * MYSQL connection handle, which is already connected. This object will then
   * use the connection handle to read and update stream metadata.
   *
   * This member function takes an optional parameter, the state. If that
   * parameter is provided, the caller is relenquishing the ownership and
   * handing it over to the this object. At deletion time, this object will also
   * delete the state object provided.
   *
   * If the state object is not provided, a new one will be created, which will
   * also be deleted when this object is destroyed.
   *
   * @param state Optional parameter. If provided, it will be used as the
   * initial state while attaching to the stream.
   *
   * @returns true if there is a failure, false otherwise.
   */
  bool open(std::shared_ptr<State> state) override;

  /**
   * @brief Gets the next entry in the stream and puts it in the buffer.
   *
   * @param buffer the buffer to store the next event read from the stream.
   *
   * @returns true if there was an error while reading from the stream, false
   * otherwise.
   */
  bool read(std::vector<uint8_t> &buffer) override;

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
  std::shared_ptr<State> get_state() const override;

  /**
   * @brief Get the mysql connection handle.
   *
   * @returns the connection handle.
   */
  virtual MYSQL *get_mysql_connection() const;
};

}  // namespace cs::reader::binary

#endif
