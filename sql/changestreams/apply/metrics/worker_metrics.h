/*
   Copyright (c) 2024, Oracle and/or its affiliates.

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
   but WITHOUT ANY WARRANTY; witho`ut even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef CS_WORKER_METRICS_H
#define CS_WORKER_METRICS_H

#include <cstdint>  // int64_t

namespace cs::apply::instruments {

/// @brief Abstract class for classes that contain metrics related to
/// transaction execution in applier workers
class Worker_metrics {
 public:
  /// @brief This class helps signaling a transactions as DDL or DML
  enum class Transaction_type_info {
    UNKNOWN,  // The transaction type is not yet known
    DML,      // It is a DML transaction
    DDL       // It is a DDL transaction
  };

  virtual ~Worker_metrics() = default;

  /// @brief Resets the instruments on this instance.
  virtual void reset() = 0;

  /// @brief Returns the type of the currently being processed transaction
  /// @return If the type is unknown, DML or DDL
  virtual Transaction_type_info get_transaction_type() const = 0;

  /// @brief Set the type for the transaction being currently processed
  /// @param type_info what is the type: UNKONWN, DML or DDL
  virtual void set_transaction_type(Transaction_type_info type_info) = 0;

  /// @brief set the full size of the ongoing transaction.
  /// @param amount new size
  virtual void set_transaction_ongoing_full_size(int64_t amount) = 0;

  /// @brief Gets the full size of the ongoing transaction
  /// @return the total size of the ongoing transaction
  virtual int64_t get_transaction_ongoing_full_size() const = 0;

  /// @brief increment the executed size of the ongoing transaction.
  /// @param amount the size amount to increment.
  virtual void inc_transaction_ongoing_progress_size(int64_t amount) = 0;

  /// @brief Resets the the executed size of the ongoing transaction to 0
  virtual void reset_transaction_ongoing_progress_size() = 0;

  /// @brief Gets the executed size of the ongoing transaction
  /// @return the exectuted size of the ongoing transaction
  virtual int64_t get_transaction_ongoing_progress_size() const = 0;

  /// @brief Gets the total time waited on commit order
  /// @return the sum of the time waited on commit
  virtual int64_t get_wait_time_on_commit_order() const = 0;

  /// @brief Increments the number of times waited
  virtual void inc_waited_time_on_commit_order(unsigned long amount) = 0;

  /// @brief Get the number of time waiting on commit order
  /// @return the counter of waits on commit order
  virtual int64_t get_number_of_waits_on_commit_order() const = 0;

  /// @brief Increments the number of times waited
  virtual void inc_number_of_waits_on_commit_order() = 0;
};
}  // namespace cs::apply::instruments

#endif /* CS_WORKER_METRICS_H */
