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

#ifndef MYSQLSHDK_SCRIPTING_POLYGLOT_LANGUAGES_POLYGLOT_GARBAGE_COLLECTOR_H_
#define MYSQLSHDK_SCRIPTING_POLYGLOT_LANGUAGES_POLYGLOT_GARBAGE_COLLECTOR_H_
#include "utils/polyglot_api_clean.h"

#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>

namespace shcore {
namespace polyglot {

/**
 * The Garbage Collection logic is determined by the occurrence of events such
 * as:
 *
 * - Statements being executed
 * - Language instances being disposed
 * - Completion of predefined time lapse
 *
 * The trigger for GC operations is defined by the following optional
 * configurations:
 *
 * - Minimal interval (seconds): if defined, the GC will NOT be executed in
 *   periods shorter than this interval.
 * - Minimal executed statements: if defined, the GC will not be executed if
 *   less than the defined number of statements has been executed.
 * - Minimal language instances: if defined, the GC will not be executed if less
 *   than the defined number of language instances have been terminated.
 *
 * Additionally, when the garbage collector may be stopped (i.e. when
 * terminating the application), in such case, a GC operation will be triggered
 * despite the state of the configurable options.
 *
 * This class keeps the Garbage Collection configuration and executes GC in a
 * separate thread based on the configuration and reported events.
 */
class Garbage_collector final {
  enum class State { INITIAL, STARTED, TERMINATED, ERROR };

 public:
  using Time_point = std::chrono::time_point<std::chrono::system_clock>;
  Garbage_collector() = default;
  ~Garbage_collector();

  Garbage_collector(const Garbage_collector &other) = delete;
  Garbage_collector(Garbage_collector &&other) = delete;

  Garbage_collector &operator=(const Garbage_collector &other) = delete;
  Garbage_collector &operator=(Garbage_collector &&other) = delete;

  enum class Event { EXECUTED_STATEMENT, TERMINATED_LANGUAGE, TERMINATE };

  struct Config {
    std::optional<size_t> interval;
    std::optional<size_t> statements;
    std::optional<size_t> languages;
  };

  void notify(Event event);
  void start(Config &&config, poly_isolate isolate);
  void stop();

 private:
  void run(poly_isolate isolate);
  void set_state(State state, const std::string &error = "");
  std::mutex m_mutex;
  std::mutex m_state_mutex;
  std::condition_variable m_condition;
  std::unique_ptr<std::thread> m_thread;

  State m_state = State::INITIAL;
  std::string m_error;
  Config m_config;
  bool m_terminated = false;
  size_t m_statement_count = 0;
  size_t m_language_count = 0;

  Time_point m_last_gctime;
};

}  // namespace polyglot
}  // namespace shcore

#endif  // MYSQLSHDK_SCRIPTING_POLYGLOT_LANGUAGES_POLYGLOT_GARBAGE_COLLECTOR_H_