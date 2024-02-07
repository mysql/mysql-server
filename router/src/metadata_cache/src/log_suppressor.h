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

#ifndef METADATA_CACHE_LOG_SUPPRESSOR_INCLUDED
#define METADATA_CACHE_LOG_SUPPRESSOR_INCLUDED

#include <map>
#include <string>

#include "mysql/harness/logging/logging.h"

namespace metadata_cache {

// helper class - helps to log the message about the cluster or instance only
// when the message text (condition) changes
class LogSuppressor {
 public:
  enum class MessageId {
    /* incorrect JSON for _disconnect_existing_sessions_when_hidden from the
       last query */
    kDisconnectExistingSessionsWhenHidden,

    /* incorrect JSON for _hidden in the metadata from  the last query */
    kHidden,

    /* instance type incompatible with the Cluster type */
    kIncompatibleInstanceType,

    /* incorrect JSON for instance_type from the last query */
    kInstanceType,

    /* incorrect JSON or value for read_only_targets */
    kReadOnlyTargets,

    /* incorrect JSON or value for unreachable_quorum_allowed_traffic */
    kUnreachableQuorumAllowedTraffic,

    /* deprecated version of Cluster Metadata */
    kDeprecatedMetadataVersion
  };

  static LogSuppressor &instance() {
    static LogSuppressor instance_;
    return instance_;
  }

  void log_message(const MessageId id, const std::string &uuid,
                   const std::string &message, bool invalid_condition,
                   mysql_harness::logging::LogLevel invalid_condition_level =
                       mysql_harness::logging::LogLevel::kWarning,
                   mysql_harness::logging::LogLevel valid_condition_level =
                       mysql_harness::logging::LogLevel::kWarning,
                   const bool log_initial_valid = false);

  ~LogSuppressor();

 private:
  // the key in the map is the std::pair<uuid, message_id>
  using MessageKey = std::pair<std::string, MessageId>;
  std::map<MessageKey, std::string> messages_;

  // singleton
  LogSuppressor() = default;
  LogSuppressor(const LogSuppressor &) = delete;
  LogSuppressor &operator=(const LogSuppressor &) = delete;
};

}  // namespace metadata_cache

#endif  // METADATA_CACHE_LOG_SUPPRESSOR_INCLUDED
