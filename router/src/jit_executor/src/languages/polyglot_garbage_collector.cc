/*
 * Copyright (c) 2024, 2026, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is designed to work with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have either included with
 * the program or referenced in the documentation.
 *
 * This program is distributed in the hope that it will be useful,  but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 * the GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "languages/polyglot_garbage_collector.h"

#include "utils/polyglot_api_clean.h"

#include "utils/polyglot_error.h"

namespace shcore {
namespace polyglot {

Garbage_collector::~Garbage_collector() {
  if (m_thread) {
    stop();
  }
}

void Garbage_collector::start(Config &&config, poly_isolate isolate) {
  m_config = std::move(config);
  m_thread =
      std::make_unique<std::thread>(&Garbage_collector::run, this, isolate);
}

void Garbage_collector::stop() {
  notify(Event::TERMINATE);
  if (m_thread) {
    m_thread->join();
    m_thread.reset();
  }
}

void Garbage_collector::notify(Event event) {
  {
    std::lock_guard lock(m_state_mutex);
    if (m_state == State::ERROR) {
      throw Polyglot_generic_error(m_error);
    }
  }

  {
    std::lock_guard lock(m_mutex);
    switch (event) {
      case Event::EXECUTED_STATEMENT:
        m_statement_count++;
        break;
      case Event::TERMINATED_LANGUAGE:
        m_language_count++;
        break;
      case Event::TERMINATE:
        m_terminated = true;
        break;
    }
  }
  m_condition.notify_one();
}

void Garbage_collector::set_state(State state, const std::string &error) {
  std::lock_guard lock(m_state_mutex);
  m_state = state;
  m_error = error;
}

void Garbage_collector::run(poly_isolate isolate) {
  try {
    poly_thread thread;
    if (poly_ok != poly_attach_thread(isolate, &thread)) {
      throw Polyglot_generic_error("Error attaching garbage collector thread.");
    }

    set_state(State::STARTED);

    // Initial time for last garbage collection
    m_last_gctime = std::chrono::system_clock::now();

    while (!m_terminated) {
      std::unique_lock lock(m_mutex);
      m_condition.wait(lock, [this]() {
        if (m_terminated) {
          return true;
        }

        bool interval_ok = true;
        if (m_config.interval.has_value()) {
          auto now = std::chrono::system_clock::now();
          std::chrono::duration<double> diff = now - m_last_gctime;
          auto seconds =
              std::chrono::duration_cast<std::chrono::seconds>(diff).count();
          interval_ok = seconds >= static_cast<long>(*m_config.interval);
        }

        return interval_ok && ((m_config.statements.has_value() &&
                                m_statement_count >= *m_config.statements) ||
                               (m_config.languages.has_value() &&
                                m_language_count >= *m_config.languages));
      });

      if (poly_ok != poly_system_gc(thread)) {
        throw Polyglot_generic_error("Error executing garbage collector");
      }

      m_language_count = 0;

      m_statement_count = 0;

      m_last_gctime = std::chrono::system_clock::now();
    }

    set_state(State::TERMINATED);

    if (poly_ok != poly_detach_thread(thread)) {
      throw Polyglot_generic_error("Error detaching garbage collector thread.");
    }
  } catch (const Polyglot_generic_error &error) {
    set_state(State::ERROR, error.what());
  }
}

}  // namespace polyglot
}  // namespace shcore