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
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef CS_DUMMY_WORKER_METRICS_H
#define CS_DUMMY_WORKER_METRICS_H

#include <cstdint>  // int64_t
#include "worker_metrics.h"

namespace cs::apply::instruments {

/// @brief Class that intends to be a dummy end point for worker metrics
class Dummy_worker_metrics : public Worker_metrics {
 public:
  Dummy_worker_metrics() = default;

  // disable copy assignment operator and the copy constructor
  Dummy_worker_metrics &operator=(const Dummy_worker_metrics &other) = delete;
  Dummy_worker_metrics(const Dummy_worker_metrics &other) = delete;

  /// @brief Resets the instruments on this instance.
  void reset() override;

  /// @brief Returns the type of the currently being processed transaction
  /// @return If the type is unknown, DML or DDL
  Worker_metrics::Transaction_type_info get_transaction_type() const override;

  /// @brief Set the type for the transaction being currently processed
  /// @param type_info what is the type: UNKONWN, DML or DDL
  void set_transaction_type(Transaction_type_info type_info) override;

  /// @brief set the full size of the ongoing transaction.
  /// @param amount new size
  void set_transaction_ongoing_full_size(int64_t amount) override;

  /// @brief Gets the full size of the ongoing transaction
  /// @return the total size of the ongoing transaction
  int64_t get_transaction_ongoing_full_size() const override;

  /// @brief increment the executed size of the ongoing transaction.
  /// @param amount the size amount to increment.
  void inc_transaction_ongoing_progress_size(int64_t amount) override;

  /// @brief Resets the the executed size of the ongoing transaction to 0
  void reset_transaction_ongoing_progress_size() override;

  /// @brief Gets the executed size of the ongoing transaction
  /// @return the exectuted size of the ongoing transaction
  int64_t get_transaction_ongoing_progress_size() const override;

  /// @brief Gets the total time waited on commit order
  /// @return the sum of the time waited on commit
  int64_t get_wait_time_on_commit_order() const override;

  /// @brief Increments the number of times waited
  void inc_waited_time_on_commit_order(unsigned long amount) override;

  /// @brief Get the number of time waiting on commit order
  /// @return the counter of waits on commit order
  int64_t get_number_of_waits_on_commit_order() const override;

  /// @brief Increments the number of times waited
  void inc_number_of_waits_on_commit_order() override;
};
}  // namespace cs::apply::instruments

#endif /* CS_DUMMY_WORKER_METRICS_H */
