/* Copyright (c) 2014, 2024, Oracle and/or its affiliates.

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

#include <iostream>
#include <sstream>
#include <thread>

#include "sql/changestreams/apply/commit_order_queue.h"

bool cs::apply::Commit_order_queue::Node::freeze_commit_sequence_nr(
    Commit_order_queue::sequence_type expected) {
  return this->m_commit_sequence_nr->compare_exchange_strong(
      expected, SEQUENCE_NR_FROZEN, std::memory_order_seq_cst);
}

bool cs::apply::Commit_order_queue::Node::unfreeze_commit_sequence_nr(
    Commit_order_queue::sequence_type reset_to) {
  auto to_replace{SEQUENCE_NR_FROZEN};
  return this->m_commit_sequence_nr->compare_exchange_strong(
      to_replace, reset_to, std::memory_order_seq_cst);
}

cs::apply::Commit_order_queue::sequence_type
cs::apply::Commit_order_queue::Node::reset_commit_sequence_nr() {
  for (; true;) {
    auto ticket_nr =
        this->m_commit_sequence_nr->load(std::memory_order_acquire);
    if (ticket_nr != SEQUENCE_NR_FROZEN &&
        this->m_commit_sequence_nr->compare_exchange_strong(
            ticket_nr, NO_SEQUENCE_NR, std::memory_order_release))
      return ticket_nr;
    std::this_thread::yield();
  }
  assert(false);
  return NO_SEQUENCE_NR;
}

cs::apply::Commit_order_queue::Iterator::Iterator(Commit_order_queue &queue,
                                                  index_type current)
    : m_target{&queue}, m_current{queue.m_commit_queue, current} {}

cs::apply::Commit_order_queue::Iterator::Iterator(const Iterator &) = default;

cs::apply::Commit_order_queue::Iterator::Iterator(Iterator &&rhs)
    : m_target{rhs.m_target}, m_current{rhs.m_current} {
  rhs.m_current = rhs.m_target->m_commit_queue.end();
  rhs.m_target = nullptr;
}

cs::apply::Commit_order_queue::Iterator &
cs::apply::Commit_order_queue::Iterator::operator=(const Iterator &) = default;

cs::apply::Commit_order_queue::Iterator &
cs::apply::Commit_order_queue::Iterator::operator=(Iterator &&rhs) {
  this->m_target = rhs.m_target;
  this->m_current = rhs.m_current;
  rhs.m_current = rhs.m_target->m_commit_queue.end();
  rhs.m_target = nullptr;
  return (*this);
}

cs::apply::Commit_order_queue::Iterator &
cs::apply::Commit_order_queue::Iterator::operator++() {
  ++this->m_current;
  return (*this);
}

cs::apply::Commit_order_queue::Node *
cs::apply::Commit_order_queue::Iterator::operator*() {
  auto index = *this->m_current;
  if (index == cs::apply::Commit_order_queue::NO_WORKER) return nullptr;
  return &(this->m_target->m_workers[index]);
}

cs::apply::Commit_order_queue::Iterator
cs::apply::Commit_order_queue::Iterator::operator++(int) {
  Iterator to_return{*this};
  ++(*this);
  return to_return;
}

cs::apply::Commit_order_queue::Node *
cs::apply::Commit_order_queue::Iterator::operator->() {
  auto index = *this->m_current;
  if (index == cs::apply::Commit_order_queue::NO_WORKER) return nullptr;
  return &(this->m_target->m_workers[index]);
}

bool cs::apply::Commit_order_queue::Iterator::operator==(
    Iterator const &rhs) const {
  return this->m_current == rhs.m_current;
}

bool cs::apply::Commit_order_queue::Iterator::operator!=(
    Iterator const &rhs) const {
  return !((*this) == rhs);
}

cs::apply::Commit_order_queue::Commit_order_queue(size_t n_workers)
    : m_workers{n_workers}, m_commit_queue{static_cast<size_t>(n_workers)} {
  for (size_t w = 0; w != this->m_workers.size(); ++w) {
    this->m_workers[w].m_worker_id = w;
  }
  DBUG_EXECUTE_IF("commit_order_queue_seq_wrap_around", {
    this->m_commit_sequence_generator->store(
        std::numeric_limits<unsigned long long>::max() - 2);
  });
}

cs::apply::Commit_order_queue::Node &cs::apply::Commit_order_queue::operator[](
    value_type id) {
  auto idx = static_cast<size_t>(id);
  assert(idx < this->m_workers.size());
  return this->m_workers[idx];
}

cs::apply::Commit_order_queue::queue_type::enum_queue_state
cs::apply::Commit_order_queue::get_state() {
  return this->m_commit_queue.get_state();
}

bool cs::apply::Commit_order_queue::is_empty() {
  return this->m_commit_queue.is_empty();
}

std::tuple<cs::apply::Commit_order_queue::value_type,
           cs::apply::Commit_order_queue::sequence_type>
cs::apply::Commit_order_queue::pop() {
  lock::Shared_spin_lock::Guard pop_sentry{
      this->m_push_pop_lock,
      lock::Shared_spin_lock::enum_lock_acquisition::SL_SHARED};
  value_type value_to_return{NO_WORKER};
  sequence_type sequence_to_return{Node::NO_SEQUENCE_NR};
  this->m_commit_queue >> value_to_return;
  this->m_commit_queue.clear_state();
  if (value_to_return != NO_WORKER) {
    sequence_to_return =
        this->m_workers[value_to_return].reset_commit_sequence_nr();
  }
  return std::make_tuple(value_to_return, sequence_to_return);
}

void cs::apply::Commit_order_queue::push(value_type index) {
  lock::Shared_spin_lock::Guard push_sentry{
      this->m_push_pop_lock,
      lock::Shared_spin_lock::enum_lock_acquisition::SL_SHARED};
  assert(this->m_workers[index].m_commit_sequence_nr == Node::NO_SEQUENCE_NR);
  sequence_type next{Node::NO_SEQUENCE_NR};
  do {
    next = this->m_commit_sequence_generator->fetch_add(1);
  } while (next <= Node::SEQUENCE_NR_FROZEN);
  this->m_workers[index].m_commit_sequence_nr->store(next);
  this->m_commit_queue << index;
  assert(this->m_commit_queue.get_state() !=
         Commit_order_queue::queue_type::enum_queue_state::NO_SPACE_AVAILABLE);
  this->m_commit_queue.clear_state();
}

cs::apply::Commit_order_queue::value_type
cs::apply::Commit_order_queue::front() {
  return this->m_commit_queue.front();
}

void cs::apply::Commit_order_queue::clear() { this->m_commit_queue.clear(); }

void cs::apply::Commit_order_queue::freeze() {
  this->m_push_pop_lock.acquire_exclusive();
}

void cs::apply::Commit_order_queue::unfreeze() {
  this->m_push_pop_lock.release_exclusive();
}

cs::apply::Commit_order_queue::Iterator cs::apply::Commit_order_queue::begin() {
  return cs::apply::Commit_order_queue::Iterator{*this,
                                                 this->m_commit_queue.head()};
}

cs::apply::Commit_order_queue::Iterator cs::apply::Commit_order_queue::end() {
  return cs::apply::Commit_order_queue::Iterator{*this,
                                                 this->m_commit_queue.tail()};
}

std::string cs::apply::Commit_order_queue::to_string() {
  return this->m_commit_queue.to_string();
}

cs::apply::Commit_order_queue::sequence_type
cs::apply::Commit_order_queue::get_next_sequence_nr(
    sequence_type current_seq_nr) {
  sequence_type next{current_seq_nr};
  do {
    ++next;
  } while (next <= Node::SEQUENCE_NR_FROZEN);
  return next;
}
