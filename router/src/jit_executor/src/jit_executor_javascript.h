/*
  Copyright (c) 2024, 2026, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_JIT_EXECUTOR_SRC_JIT_EXECUTOR_JAVASCRIPT_H_
#define ROUTER_SRC_JIT_EXECUTOR_SRC_JIT_EXECUTOR_JAVASCRIPT_H_

#include <condition_variable>
#include <memory>  // shared_ptr
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <variant>
#include <vector>

#include "languages/polyglot_javascript.h"
#include "mysql/harness/mpsc_queue.h"
#include "mysqlrouter/jit_executor_callbacks.h"
#include "mysqlrouter/jit_executor_common.h"
#include "mysqlrouter/jit_executor_db_interface.h"
#include "mysqlrouter/jit_executor_value.h"
#include "mysqlrouter/polyglot_file_system.h"
#include "native_wrappers/polyglot_object_bridge.h"
#include "objects/polyglot_session.h"

namespace jit_executor {

using Value = shcore::Value;
using Dictionary_t = shcore::Dictionary_t;
using Polyglot_error = shcore::polyglot::Polyglot_error;
using IFile_system = shcore::polyglot::IFile_system;
using shcore::polyglot::Object_bridge_t;

// To be used to determine the actual state of the produced result
enum class ResultState { Ok, Error, ResourceExhausted };

// To be used to determine the processing state
enum class ProcessingState { Idle, Processing, HoldingResult, Finished };

struct Result {
  std::optional<ResultState> state;
  std::optional<std::string> data;

  void reset() {
    state.reset();
    data.reset();
  }
};

struct Code {
  std::string source;
  ResultType result_type;
};

/**
 * MRS JavaScript Implementation
 *
 * Starts the JavaScript engine in a thread for execution of code from the
 * MRS end points. A threaded version is needed to support the JavaScript
 * Promise resolution to get the final result.
 *
 * To achieve these two global functions are exposed: synch_return and
 * synch_error, such function would be used on the promise resolution by
 * executing:
 *   promise.then(value => synch_return(value), error=>synch_error(err0r))
 */
class JavaScript : public shcore::polyglot::Java_script_interface {
 public:
  using Java_script_interface::Java_script_interface;
  ~JavaScript() override = default;

  bool start(size_t id, const std::shared_ptr<IFile_system> &fs = {},
             const Dictionary_t &predefined_globals = {});
  void stop();

  std::string execute(const std::string &code, int timeout,
                      ResultType result_type, const GlobalCallbacks &callbacks);

  std::string get_parameter_string(const std::vector<Value> &parameters) const;

  /**
   * Wraps a call to poly_context_eval
   */
  int64_t eval(poly_reference source, poly_value *result) const;

  /**
   * Creates a Source object
   */
  poly_value create_source(const std::string &source,
                           const std::string &code_str) const;

  bool is_idle();
  bool wait_for_idle();

  size_t id() { return m_id; }

 private:
  void run();
  void stop_run_thread();

  Value native_object(poly_value object);
  Value native_array(poly_value object);
  Value to_native_object(poly_value object,
                         const std::string &class_name) override;
  void output_handler(const char *bytes, size_t length) override;
  void error_handler(const char *bytes, size_t length) override;
  poly_value from_native_object(const Object_bridge_t &object) const override;

  void create_result(const Value &result, ResultState state = ResultState::Ok);
  void create_result(const shcore::polyglot::Polyglot_error &error);

  // Every global function exposed to JavaScript requires:
  // - The function implementation
  // - The function metadata
  poly_value synch_return(const std::vector<poly_value> &args);
  struct Synch_return {
    static const constexpr char *name = "synch_return";
    static const constexpr std::size_t argc = 1;
    static const constexpr auto callback = &JavaScript::synch_return;
  };

  poly_value synch_error(const std::vector<poly_value> &args);
  struct Synch_error {
    static const constexpr char *name = "synch_error";
    static const constexpr std::size_t argc = 1;
    static const constexpr auto callback = &JavaScript::synch_error;
  };

  void resolve_promise(poly_value promise);
  shcore::Value get_session(const std::vector<shcore::Value> &args);

  struct Get_session {
    static const constexpr char *name = "getSession";
    static const constexpr std::size_t argc = 1;
    static const constexpr auto callback = &JavaScript::get_session;
  };

  poly_value get_current_mrs_user_id();
  struct Get_current_mrs_user_id {
    static const constexpr char *name = "getCurrentMrsUserId";
    static const constexpr auto callback = &JavaScript::get_current_mrs_user_id;
  };

  shcore::Value get_content_set_path(const std::vector<shcore::Value> &args);
  struct Get_content_set_path {
    static const constexpr char *name = "getContentSetPath";
    static const constexpr std::size_t argc = 1;
    static const constexpr auto callback = &JavaScript::get_content_set_path;
  };

  void set_processing_state(ProcessingState state);

  // To control the statement execution, the execution thread will be in
  // wait state until a statement arrives
  std::unique_ptr<std::thread> m_execution_thread;

  Dictionary_t m_predefined_globals;

  mysql_harness::WaitingMPSCQueue<std::variant<std::monostate, Code>> m_code;
  mysql_harness::WaitingMPSCQueue<Result> m_result;

  ResultType m_result_type;
  poly_value m_promise_resolver;

  const GlobalCallbacks *m_global_callbacks = nullptr;
  std::shared_ptr<shcore::polyglot::Session> m_session;

  std::optional<ProcessingState> m_processing_state;
  std::condition_variable m_processing_state_condition;
  std::mutex m_processing_state_mutex;
  size_t m_id = 0;
};

}  // namespace jit_executor

#endif  // ROUTER_SRC_JIT_EXECUTOR_SRC_JIT_EXECUTOR_JAVASCRIPT_H_
