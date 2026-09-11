// Copyright (c) 2026, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is designed to work with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation.  The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have either included with
// the program or referenced in the documentation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA.

#ifndef MYSQL_CSA_STORAGE_IN_MEMORY_CACHED_EVENT_MEMORY_H
#define MYSQL_CSA_STORAGE_IN_MEMORY_CACHED_EVENT_MEMORY_H

#include <cstddef>
#include <memory>
#include <vector>

#include "sql/changestreams/apply/storage/relay_log/ireader_event.h"  // IReader_event

class Format_description_log_event;
class Log_event;

namespace mysql::csa {

/// @brief An IReader_event that keeps an OWNING copy of the raw event bytes and
/// can be decoded more than once.
class Cached_event_memory : public IReader_event {
 public:
  /// @brief Take an owning copy of @p len bytes at @p buf.
  ///
  /// @param buf             The (transient) encoded event bytes; copied in.
  /// @param len             Number of bytes at @p buf.
  /// @param fde             The active Format_description_log_event (shared
  ///                        ownership) used to deserialize on every decode().
  /// @param verify_checksum Whether decode() re-verifies the event checksum.
  Cached_event_memory(const char *buf, std::size_t len,
                      std::shared_ptr<Format_description_log_event> fde,
                      bool verify_checksum);

  /// @brief Deserialize a FRESH Log_event from the owned byte copy.
  /// @return The decoded event, or an empty shared_ptr on error.
  std::shared_ptr<Log_event> decode() override;

  /// @brief No-op: the master byte copy is immutable and reused per decode().
  void reset(const Format_description_log_event *) override;

 private:
  std::vector<unsigned char> m_bytes;  ///< Owning master copy of the bytes.
  std::shared_ptr<Log_event> m_fde;    ///< FDE (owning base pointer).
  Format_description_log_event *m_fde_ptr;  ///< Non-owning typed FDE.
  bool m_verify_checksum{false};
};

}  // namespace mysql::csa

#endif  // MYSQL_CSA_STORAGE_IN_MEMORY_CACHED_EVENT_MEMORY_H
