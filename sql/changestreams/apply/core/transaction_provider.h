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

#ifndef MYSQL_CSA_CORE_TRANSACTION_PROVIDER_H
#define MYSQL_CSA_CORE_TRANSACTION_PROVIDER_H

#include <memory>
#include "sql/changestreams/apply/jobs/job.h"
#include "sql/changestreams/apply/storage/relay_log/relay_log_adaptive_reader.h"

namespace mysql::csa {

class Transaction_provider;
using Transaction_provider_sptr = std::shared_ptr<Transaction_provider>;

/// Interface for all transaction providers.
/// Transaction provider shall implement the following methods:
/// - start - start provider in case this provider is asynchronous
/// - stop - stops provider and wakes blocking reads
/// - finish - completes provider shutdown from the provider owner thread
/// - is_stopped - checks if stop has been requested internally or externally
/// - next - blocks until next transaction is fetched or provider reaches
///          stop conditions (is_stopped will return true) or temporarily
///          wakes up thread requesting the next transaction
class Transaction_provider {
 public:
  // using Common_reader_type = Reader_relaylog;
  using Common_reader_type = Relay_log_adaptive_reader;
  /// Get the next job
  /// @return Job smart pointer
  virtual Job_ptr next() = 0;
  /// Checks whether provider has been stopped externally or internally
  /// @brief True if stopped, false otherwise
  virtual bool is_stopped() const = 0;
  /// Start provider (if asynchronous)
  virtual void start() = 0;
  /// Stop provider and wake blocked reader calls.
  virtual void stop() = 0;
  /// Complete provider shutdown from the owner thread (transaction receiver).
  /// Requires stop to have been called first; otherwise it is a no-op.
  virtual void finish() = 0;
  /// @brief Check if provider has an error
  /// @return True in case an error occurred, false otherwise
  virtual bool is_error() const = 0;
  /// Destructor
  virtual ~Transaction_provider() = default;
};

}  // namespace mysql::csa

#endif  // MYSQL_CSA_CORE_TRANSACTION_PROVIDER_H
