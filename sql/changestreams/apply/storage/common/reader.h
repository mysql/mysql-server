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

#ifndef MYSQL_CSA_READER_H
#define MYSQL_CSA_READER_H

#include <memory>
#include "sql/changestreams/apply/jobs/job.h"

namespace mysql::csa {

/// @brief Abstract interface for reading jobs from a storage source.
///
/// This class defines methods for reading Job objects and checking the status
/// of the Change Stream Applier (CSA).
class Reader;
/// Shared reader type
using Reader_sptr = std::shared_ptr<Reader>;

class Reader {
 public:
  /// @brief Reads and returns a unique pointer to a Job object.
  ///
  /// This method is responsible for reading data and creating a Job.
  /// @return A unique pointer to the Job read from the storage.
  virtual Job_ptr read() = 0;
  /// @brief Checks whether the Change Stream Applier (CSA) is stopped.
  ///
  /// @return true if a stop was requested, false otherwise.
  virtual bool is_stopped() const = 0;
  /// @brief Checks whether the Change Stream Applier (CSA) has encountered an
  /// error.
  ///
  /// @return true if an error occurred, false otherwise.
  virtual bool is_error() const = 0;
  /// Destructor
  virtual ~Reader() = default;
  /// Stops the reader
  virtual void stop() = 0;
};

}  // namespace mysql::csa

#endif  // MYSQL_CSA_READER_H
