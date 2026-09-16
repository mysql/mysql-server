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

#include "sql/changestreams/apply/jobs/fetchable_transaction.h"

namespace mysql::csa {

Fetchable_transaction::Fetchable_transaction() {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_current_batch_it = m_event_set_batches.begin();
  m_status = Return_status::ok;
}

Fetchable_transaction::Fetchable_transaction(
    Event_set_fetchable_list &&fetch_object)
    : m_event_set_batches(std::move(fetch_object)) {
  reset_fetching(false);
}

void Fetchable_transaction::set_success() {
  std::lock_guard<std::mutex> lock(m_mutex);
  auto bit = m_event_set_batches.begin();
  while (bit != m_event_set_batches.end()) {
    bit->get()->set_success();
    ++bit;
  }
}

Fetchable_transaction::~Fetchable_transaction() {}

void Fetchable_transaction::reset_fetching(bool all) {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_current_batch_it = m_event_set_batches.begin();
  for (const auto &batch : m_event_set_batches) {
    batch->reset(all);
  }
  m_is_done = false;
  m_is_truncated.store(false, std::memory_order_release);
  m_status = Return_status::ok;
  m_error_message.assign("");
}

Format_description_log_event *Fetchable_transaction::get_fde() {
  std::lock_guard<std::mutex> lock(m_mutex);
  auto current_batch_result = get_current_event_batch_unsafe();
  if (!current_batch_result.has_value()) {
    if (m_event_set_batches.empty()) {
      return nullptr;
    }
    auto last_batch_it = --m_event_set_batches.end();
    return last_batch_it->get()->get_fde();
  }
  return current_batch_result.value()->get_fde();
}

/// @brief Returns information on whether this is actual transaction
/// (transaction starting with a GTID)
bool Fetchable_transaction::is_trx() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  auto bit = m_event_set_batches.begin();
  if (bit != m_event_set_batches.end()) {
    return bit->get()->is_trx();
  }
  return false;
}

std::optional<Managed_event> Fetchable_transaction::fetch_next() {
  while (true) {
    auto *event_batch = wait_for_current_batch();
    if (event_batch == nullptr) {
      return {};
    }

    auto fetch_result = event_batch->fetch_next();
    if (fetch_result.has_value()) {
      bool is_last_in_transaction{false};
      {
        std::lock_guard<std::mutex> lock(m_mutex);
        is_last_in_transaction = advance_finished_batch_unsafe(event_batch);
      }
      fetch_result->set_last_in_transaction(is_last_in_transaction);
      return fetch_result.value();
    }

    {
      std::lock_guard<std::mutex> lock(m_mutex);
      if (event_batch->is_error()) {
        m_status = Return_status::error;
        m_error_message = event_batch->get_error_str();
        return {};
      }
      if (advance_finished_batch_unsafe(event_batch)) {
        return {};
      }
    }
  }
}

bool Fetchable_transaction::wait_next() {
  while (true) {
    auto *event_batch = wait_for_current_batch();
    if (event_batch == nullptr) {
      return false;
    }

    if (event_batch->wait_next()) {
      return true;
    }

    {
      std::lock_guard<std::mutex> lock(m_mutex);
      if (event_batch->is_error()) {
        m_status = Return_status::error;
        m_error_message = event_batch->get_error_str();
        return false;
      }
      if (advance_finished_batch_unsafe(event_batch)) {
        return false;
      }
    }
  }
}

bool Fetchable_transaction::is_fetching_error() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_status == Return_status::error &&
         !m_is_truncated.load(std::memory_order_acquire);
}

bool Fetchable_transaction::is_fetching_done() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_is_done && m_status != Return_status::error;
}

bool Fetchable_transaction::is_truncated() const {
  return m_is_truncated.load(std::memory_order_acquire);
}

std::size_t Fetchable_transaction::get_max_event_length() const {
  return m_max_event_length.load(std::memory_order_acquire);
}

void Fetchable_transaction::set_fetching_done() {
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_is_done = true;
  }
  m_cv.notify_all();
}

void Fetchable_transaction::append_batch(Event_set_fetchable_ptr batch) {
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_event_set_batches.push_back(std::move(batch));
    if (m_current_batch_it == m_event_set_batches.end()) {
      m_current_batch_it = m_event_set_batches.begin();
    }
  }
  m_cv.notify_all();
}

void Fetchable_transaction::set_fetching_complete() {
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_is_complete = true;
    if (m_current_batch_it == m_event_set_batches.end()) {
      m_is_done = true;
    }
  }
  m_cv.notify_all();
}

void Fetchable_transaction::update_max_event_length(std::size_t event_length) {
  auto current = m_max_event_length.load(std::memory_order_relaxed);
  while (current < event_length &&
         !m_max_event_length.compare_exchange_weak(current, event_length,
                                                   std::memory_order_release,
                                                   std::memory_order_relaxed)) {
  }
}

void Fetchable_transaction::set_fetching_truncated() {
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_status = Return_status::error;
    m_is_truncated.store(true, std::memory_order_release);
  }
  m_cv.notify_all();
}

std::string Fetchable_transaction::get_fetch_error_msg() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_error_message;
}

Event_set_fetchable *Fetchable_transaction::wait_for_current_batch() {
  while (true) {
    std::unique_lock<std::mutex> lock(m_mutex);
    if ((m_is_done && m_status != Return_status::error) ||
        m_status == Return_status::error) {
      return nullptr;
    }

    auto current_batch_result = get_current_event_batch_unsafe();
    if (current_batch_result.has_value()) {
      return current_batch_result.value();
    }

    if (m_is_truncated.load(std::memory_order_acquire)) {
      return nullptr;
    }
    if (m_is_complete) {
      m_is_done = true;
      return nullptr;
    }

    m_cv.wait(lock);
  }
}

bool Fetchable_transaction::advance_finished_batch_unsafe(
    Event_set_fetchable *event_batch) {
  if (!event_batch->is_done()) {
    return false;
  }

  ++m_current_batch_it;
  if (m_current_batch_it == m_event_set_batches.end() && m_is_complete) {
    m_is_done = true;
    return true;
  }
  return false;
}

std::optional<Event_set_fetchable *>
Fetchable_transaction::get_current_event_batch_unsafe() {
  if (m_current_batch_it == m_event_set_batches.end()) {
    return {};
  }
  auto *current_batch = m_current_batch_it->get();
  if (current_batch->is_done()) {
    ++m_current_batch_it;
    return get_current_event_batch_unsafe();
  }
  return current_batch;
}

}  // namespace mysql::csa
