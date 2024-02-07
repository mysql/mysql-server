/*
  Copyright (c) 2019, 2024, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef MYSQL_ROUTER_CLASSIC_PROTOCOL_SESSION_TRACK_H_
#define MYSQL_ROUTER_CLASSIC_PROTOCOL_SESSION_TRACK_H_

// session_track as used by message::server::Ok and message::server::Eof

#include <span>
#include <string>

namespace classic_protocol {

namespace borrowable {
namespace session_track {

/**
 * Field of a session-track array.
 *
 * used in server::Ok and server::Eof
 */
template <bool Borrowed>
class Field {
 public:
  using string_type =
      std::conditional_t<Borrowed, std::string_view, std::string>;

  constexpr Field(uint8_t type, string_type data)
      : type_{type}, data_{std::move(data)} {}

  constexpr uint8_t type() const noexcept { return type_; }
  constexpr string_type data() const noexcept { return data_; }

 private:
  uint8_t type_;
  string_type data_;
};

template <bool Borrowed>
inline bool operator==(const Field<Borrowed> &a, const Field<Borrowed> &b) {
  return (a.type() == b.type()) && (a.data() == b.data());
}

/**
 * system-variable changed.
 *
 * see: session_track_system_variable
 */
template <bool Borrowed>
class SystemVariable {
 public:
  using string_type =
      std::conditional_t<Borrowed, std::string_view, std::string>;
  constexpr SystemVariable(string_type key, string_type value)
      : key_{std::move(key)}, value_{std::move(value)} {}

  constexpr string_type key() const noexcept { return key_; }
  constexpr string_type value() const noexcept { return value_; }

 private:
  string_type key_;
  string_type value_;
};

template <bool Borrowed>
inline bool operator==(const SystemVariable<Borrowed> &a,
                       const SystemVariable<Borrowed> &b) {
  return (a.key() == b.key()) && (a.value() == b.value());
}

/**
 * schema changed.
 *
 * see: session_track_schema
 */
template <bool Borrowed>
class Schema {
 public:
  using string_type =
      std::conditional_t<Borrowed, std::string_view, std::string>;

  constexpr Schema(string_type schema) : schema_{std::move(schema)} {}

  constexpr string_type schema() const noexcept { return schema_; }

 private:
  string_type schema_;
};

template <bool Borrowed>
inline bool operator==(const Schema<Borrowed> &a, const Schema<Borrowed> &b) {
  return (a.schema() == b.schema());
}

/**
 * state changed.
 *
 * see: session_track_session_state
 */
class State {
 public:
  constexpr State(int8_t state) : state_{std::move(state)} {}

  constexpr int8_t state() const noexcept { return state_; }

 private:
  int8_t state_;
};

constexpr inline bool operator==(const State &a, const State &b) {
  return (a.state() == b.state());
}

/**
 * gtid changed.
 *
 * - FixedInt<1> spec
 * - gtid-string
 * -
 *
 * see: session_track_gtid
 */
template <bool Borrowed>
class Gtid {
 public:
  using string_type =
      std::conditional_t<Borrowed, std::string_view, std::string>;
  constexpr Gtid(uint8_t spec, string_type gtid)
      : spec_{spec}, gtid_{std::move(gtid)} {}

  constexpr uint8_t spec() const noexcept { return spec_; }
  constexpr string_type gtid() const { return gtid_; }

 private:
  uint8_t spec_;
  string_type gtid_;
};

template <bool Borrowed>
inline bool operator==(const Gtid<Borrowed> &a, const Gtid<Borrowed> &b) {
  return (a.spec() == b.spec()) && (a.gtid() == b.gtid());
}

/**
 * TransactionState changed.
 *
 * - trx_type: Explicit|Implicit|none
 * - read_unsafe: one_or_more|none
 * - read_trx: one_or_more|none
 * - write_unsafe: one_or_more|none
 * - write_trx: one_or_more|none
 * - stmt_unsafe: one_or_more|none
 * - resultset: one_or_more|none
 * - locked_tables: one_or_more|none
 *
 * implicit transaction: no autocommit, stmt against transactionable table
 * without START TRANSACTION
 * explicit transaction: START TRANSACTION
 *
 * read_unsafe: read-operation against non-transactionable table
 * read_trx: read-operation against transactionable table
 * write_unsafe: write-operation against non-transactionable table
 * write_trx: write-operation against transactionable table
 * stmt_unsafe: an unusafe statement was executed like RAND()
 * resultset: some resultset was sent
 * locked_tables: some tables got locked explicitly
 *
 * 'resultset' may be triggered without 'read_trx' and 'read_unsafe' if a
 * 'SELECT' was executed against 'dual' or without table.
 *
 * see: session_track_transaction_info
 */
class TransactionState {
 public:
  constexpr TransactionState(char trx_type, char read_unsafe, char read_trx,
                             char write_unsafe, char write_trx,
                             char stmt_unsafe, char resultset,
                             char locked_tables)
      : trx_type_{trx_type},
        read_unsafe_{read_unsafe},
        read_trx_{read_trx},
        write_unsafe_{write_unsafe},
        write_trx_{write_trx},
        stmt_unsafe_{stmt_unsafe},
        resultset_{resultset},
        locked_tables_{locked_tables} {}

  constexpr TransactionState(std::span<char, 8> val)
      : trx_type_{val[0]},
        read_unsafe_{val[1]},
        read_trx_{val[2]},
        write_unsafe_{val[3]},
        write_trx_{val[4]},
        stmt_unsafe_{val[5]},
        resultset_{val[6]},
        locked_tables_{val[7]} {}

  constexpr char trx_type() const noexcept { return trx_type_; }
  constexpr char read_unsafe() const noexcept { return read_unsafe_; }
  constexpr char read_trx() const noexcept { return read_trx_; }
  constexpr char write_unsafe() const noexcept { return write_unsafe_; }
  constexpr char write_trx() const noexcept { return write_trx_; }
  constexpr char stmt_unsafe() const noexcept { return stmt_unsafe_; }
  constexpr char resultset() const noexcept { return resultset_; }
  constexpr char locked_tables() const noexcept { return locked_tables_; }

 private:
  char trx_type_;       // T|I|_
  char read_unsafe_;    // r|_
  char read_trx_;       // R|_
  char write_unsafe_;   // w|_
  char write_trx_;      // W|_
  char stmt_unsafe_;    // s|_
  char resultset_;      // S|_
  char locked_tables_;  // L|_
};

inline bool operator==(const TransactionState &a, const TransactionState &b) {
  return (a.trx_type() == b.trx_type()) &&
         (a.read_unsafe() == b.read_unsafe()) &&
         (a.read_trx() == b.read_trx()) &&
         (a.write_unsafe() == b.write_unsafe()) &&
         (a.write_trx() == b.write_trx()) &&
         (a.stmt_unsafe() == b.stmt_unsafe()) &&
         (a.resultset() == b.resultset()) &&
         (a.locked_tables() == b.locked_tables());
}

/**
 * TransactionCharacteristics changed.
 *
 * resembles the SQL-text which started the transaction.
 *
 * see: session_track_transaction_info
 */
template <bool Borrowed>
class TransactionCharacteristics {
 public:
  using string_type =
      std::conditional_t<Borrowed, std::string_view, std::string>;

  constexpr TransactionCharacteristics(string_type characteristics)
      : characteristics_{std::move(characteristics)} {}

  constexpr string_type characteristics() const { return characteristics_; }

 private:
  string_type characteristics_;
};

template <bool Borrowed>
inline bool operator==(const TransactionCharacteristics<Borrowed> &a,
                       const TransactionCharacteristics<Borrowed> &b) {
  return (a.characteristics() == b.characteristics());
}
}  // namespace session_track
}  // namespace borrowable

namespace borrowed {
namespace session_track {
using Field = borrowable::session_track::Field<true>;
using TransactionCharacteristics =
    borrowable::session_track::TransactionCharacteristics<true>;
using TransactionState = borrowable::session_track::TransactionState;
using SystemVariable = borrowable::session_track::SystemVariable<true>;
using Schema = borrowable::session_track::Schema<true>;
using State = borrowable::session_track::State;
using Gtid = borrowable::session_track::Gtid<true>;
}  // namespace session_track
}  // namespace borrowed

namespace session_track {
using Field = borrowable::session_track::Field<false>;
using TransactionCharacteristics =
    borrowable::session_track::TransactionCharacteristics<false>;
using TransactionState = borrowable::session_track::TransactionState;
using SystemVariable = borrowable::session_track::SystemVariable<false>;
using Schema = borrowable::session_track::Schema<false>;
using State = borrowable::session_track::State;
using Gtid = borrowable::session_track::Gtid<false>;
}  // namespace session_track

}  // namespace classic_protocol

#endif
