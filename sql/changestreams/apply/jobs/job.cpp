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

#include "sql/changestreams/apply/jobs/job.h"
#include "sql/changestreams/apply/jobs/job_applier.h"
#include "sql/changestreams/apply/psi/psi.h"
#include "sql/changestreams/apply/service/csa_service.h"

namespace mysql::csa {

namespace {

std::atomic<bool> &default_applier_stop() {
  static std::atomic<bool> stop_requested{false};
  return stop_requested;
}

}  // namespace

std::atomic<std::size_t> Job::next_id = 0;

Job::Job(unsigned int max_retries)
    : m_is_error(false),
      m_is_done(false),
      m_max_retries(max_retries),
      m_id(next_id.fetch_add(1)),
      m_applier_stop(default_applier_stop()) {}

Job::~Job() {}

unsigned int Job::get_instance_id() const { return 0; }

bool Job::is_trx() const { return false; }

bool Job::restart() {
  m_is_done = false;
  m_is_error = false;
  m_is_fatal_error = false;
  return false;
}

Job::Thread_id Job::get_attach_id() const { return get_id(); }

std::size_t Job::get_id() const { return m_id; }

void Job::inc_retries() { ++m_trx_retries; }

bool Job::can_be_retried() {
  return m_trx_retries < m_max_retries && !is_fatal_error();
}

unsigned int Job::get_retries() const { return m_trx_retries; }

bool Job::is_error() { return m_is_error || m_is_fatal_error; }

bool Job::is_stopped() const {
  return m_applier_stop.get().load(std::memory_order_relaxed);
}

bool Job::is_fatal_error() { return m_is_fatal_error; }

void Job::set_applier_stop(std::atomic<bool> &applier_stop) {
  m_applier_stop = applier_stop;
}

void Job::set_success() {}

void Job::set_failure() {}

void Job::set_fatal_error() {
  m_is_error = true;
  m_is_done = true;
  m_is_fatal_error = true;
}

void Job::set_error() {
  m_is_error = true;
  m_is_done = true;
}

void Job::set_done() { m_is_done = true; }

bool Job::is_done() { return m_is_done; }

std::string Job::to_string() { return ""; }

}  // namespace mysql::csa
