/*
  Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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

#include <sstream>
#include <system_error>

#include "mysql/harness/net_ts/buffer.h"
#include "mysql/harness/stdx/expected.h"
#include "mysqlrouter/classic_protocol_codec_session_track.h"
#include "mysqlrouter/classic_protocol_constants.h"
#include "mysqlrouter/classic_protocol_session_track.h"

template <class T>
static constexpr uint8_t type_byte() {
  return classic_protocol::Codec<T>::type_byte();
}

stdx::expected<std::vector<std::pair<std::string, std::string>>,
               std::error_code>
session_trackers_to_string(net::const_buffer session_trackers,
                           classic_protocol::capabilities::value_type caps) {
  std::vector<std::pair<std::string, std::string>> attributes;

  enum class Type {
    SystemVariable =
        type_byte<classic_protocol::session_track::SystemVariable>(),
    Schema = type_byte<classic_protocol::session_track::Schema>(),
    State = type_byte<classic_protocol::session_track::State>(),
    Gtid = type_byte<classic_protocol::session_track::Gtid>(),
    TransactionState =
        type_byte<classic_protocol::session_track::TransactionState>(),
    TransactionCharacteristics = type_byte<
        classic_protocol::session_track::TransactionCharacteristics>(),
  };

  do {
    auto decode_session_res = classic_protocol::decode<
        classic_protocol::borrowed::session_track::Field>(session_trackers,
                                                          caps);
    if (!decode_session_res) {
      return stdx::unexpected(decode_session_res.error());
    }

    const auto decoded_size = decode_session_res->first;

    if (decoded_size == 0) {
      return stdx::unexpected(make_error_code(std::errc::bad_message));
    }

    switch (Type{decode_session_res->second.type()}) {
      case Type::SystemVariable: {
        auto decode_value_res = classic_protocol::decode<
            classic_protocol::borrowed::session_track::SystemVariable>(
            net::buffer(decode_session_res->second.data()), caps);
        if (!decode_value_res) {
          // ignore errors?
        } else {
          const auto kv = decode_value_res->second;

          attributes.emplace_back("@@SESSION." + std::string(kv.key()),
                                  std::string(kv.value()));
        }
      } break;
      case Type::Schema: {
        auto decode_value_res = classic_protocol::decode<
            classic_protocol::borrowed::session_track::Schema>(
            net::buffer(decode_session_res->second.data()), caps);
        if (!decode_value_res) {
          // ignore errors?
        } else {
          attributes.emplace_back(
              "schema", std::string(decode_value_res->second.schema()));
        }
      } break;
      case Type::State: {
        auto decode_value_res = classic_protocol::decode<
            classic_protocol::borrowed::session_track::State>(
            net::buffer(decode_session_res->second.data()), caps);
        if (!decode_value_res) {
          // ignore errors?
        } else {
          // .state() is always '1'
          attributes.emplace_back(
              "state_changed",
              std::to_string(decode_value_res->second.state()));
        }
      } break;
      case Type::Gtid: {
        auto decode_value_res = classic_protocol::decode<
            classic_protocol::borrowed::session_track::Gtid>(
            net::buffer(decode_session_res->second.data()), caps);
        if (!decode_value_res) {
          // ignore errors?
        } else {
          const auto gtid = decode_value_res->second;

          attributes.emplace_back("gtid", gtid.gtid());
        }
      } break;
      case Type::TransactionState: {
        auto decode_value_res = classic_protocol::decode<
            classic_protocol::borrowed::session_track::TransactionState>(
            net::buffer(decode_session_res->second.data()), caps);
        if (!decode_value_res) {
          // ignore errors?
        } else {
          const auto trx_state = decode_value_res->second;

          // remember the last transaction-state
          std::ostringstream oss;

          switch (trx_state.trx_type()) {
            case '_':
              oss << "no trx";
              break;
            case 'T':
              oss << "explicit trx";
              break;
            case 'I':
              oss << "implicit trx";
              break;
            default:
              oss << "(unknown trx-type)";
              break;
          }

          switch (trx_state.read_trx()) {
            case '_':
              break;
            case 'R':
              oss << ", read trx";
              break;
            default:
              oss << ", (unknown read-trx-type)";
              break;
          }

          switch (trx_state.read_unsafe()) {
            case '_':
              break;
            case 'r':
              oss << ", read trx (non-transactional)";
              break;
            default:
              oss << ", (unknown read-unsafe-type)";
              break;
          }

          switch (trx_state.write_trx()) {
            case '_':
              break;
            case 'W':
              oss << ", write trx";
              break;
            default:
              oss << ", (unknown write-trx-type)";
              break;
          }

          switch (trx_state.write_unsafe()) {
            case '_':
              break;
            case 'w':
              oss << ", write trx (non-transactional)";
              break;
            default:
              oss << ", (unknown write-unsafe-type)";
              break;
          }

          switch (trx_state.stmt_unsafe()) {
            case '_':
              break;
            case 's':
              oss << ", stmt unsafe (UUID(), RAND(), ...)";
              break;
            default:
              oss << ", (unknown stmt-unsafe-type)";
              break;
          }

          switch (trx_state.resultset()) {
            case '_':
              break;
            case 'S':
              oss << ", resultset sent";
              break;
            default:
              oss << ", (unknown resultset-type)";
              break;
          }

          switch (trx_state.locked_tables()) {
            case '_':
              break;
            case 'L':
              oss << ", LOCK TABLES";
              break;
            default:
              oss << ", (unknown locked-tables-type)";
              break;
          }

          attributes.emplace_back("transaction_state", oss.str());
        }
      } break;
      case Type::TransactionCharacteristics: {
        auto decode_value_res =
            classic_protocol::decode<classic_protocol::borrowed::session_track::
                                         TransactionCharacteristics>(
                net::buffer(decode_session_res->second.data()), caps);
        if (!decode_value_res) {
          // ignore errors?
        } else {
          const auto trx_characteristics = decode_value_res->second;

          attributes.emplace_back(
              "transaction_characteristics",
              std::string(trx_characteristics.characteristics()));
        }
      } break;
    }

    // go to the next field.
    session_trackers += decoded_size;
  } while (session_trackers.size() > 0);

  return attributes;
}
