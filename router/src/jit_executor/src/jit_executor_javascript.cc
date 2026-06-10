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

#include "jit_executor_javascript.h"

#include <array>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

#include "include/my_thread.h"
#include "languages/polyglot_javascript.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/scoped_callback.h"
#include "mysqlrouter/jit_executor_common.h"
#include "mysqlrouter/jit_executor_db_interface.h"
#include "mysqlrouter/polyglot_file_system.h"
#include "objects/polyglot_date.h"
#include "objects/polyglot_session.h"
#include "utils/polyglot_error.h"
#include "utils/polyglot_utils.h"
#include "utils/utils_string.h"

namespace jit_executor {
using shcore::polyglot::Polyglot_generic_error;

IMPORT_LOG_FUNCTIONS()

using shcore::polyglot::Date;
using Value_type = shcore::Value_type;
using Scoped_global = shcore::polyglot::Scoped_global;

void JavaScript::create_result(const Value &result, ResultState state) {
  // If it is a native object, it could well be a wrapper for an language
  // exception, so it is thrown, processed and handled as such
  if (result.get_type() == Value_type::Object) {
    auto object = result.as_object();
    if (object->is_exception()) {
      try {
        object->throw_exception();
      } catch (const Polyglot_error &error) {
        return create_result(error);
      }
    }
  }

  if (m_result_type == ResultType::Json) {
    shcore::JSON_dumper dumper;
    dumper.start_object();
    dumper.append_string("status");
    if (state == ResultState::Ok) {
      dumper.append_string("ok");
    } else {
      dumper.append_string("error");
    }
    dumper.append_value("result", result);
    dumper.end_object();

    m_result.push({state, dumper.str()});
  } else {
    m_result.push({state, result.descr(true)});
  }
}

void JavaScript::create_result(const Polyglot_error &error) {
  std::string result;

  // Any result created from a polyglot exception, sends an OK response
  auto state = ResultState::Ok;
  if (error.is_resource_exhausted()) {
    state = ResultState::ResourceExhausted;
  }
  if (m_result_type == ResultType::Json) {
    shcore::JSON_dumper dumper;
    dumper.start_object();
    dumper.append_string("status");
    dumper.append_string("error");

    dumper.append_string("message");
    dumper.append_string(error.message());

    if (error.type().has_value()) {
      dumper.append_string("type");
      dumper.append_string(*error.type());
    }

    if (error.code().has_value()) {
      dumper.append_string("code");
      dumper.append_int64(*error.code());
    }

    if (error.line().has_value()) {
      dumper.append_string("line");
      dumper.append_int64(*error.line());
    }

    if (error.column().has_value()) {
      dumper.append_string("column");
      dumper.append_int64(*error.column());
    }

    if (!error.backtrace().empty()) {
      dumper.append_string("backtrace");
      dumper.start_array();
      for (const auto &frame : error.backtrace()) {
        dumper.append_string(frame);
      }
      dumper.end_array();
    }

    dumper.end_object();

    if (state == ResultState::ResourceExhausted) {
      stop_run_thread();
    }
    m_result.push({state, dumper.str()});
  } else {
    if (state == ResultState::ResourceExhausted) {
      stop_run_thread();
    }
    m_result.push({state, error.format(true)});
  }
}

bool JavaScript::start(size_t id, const std::shared_ptr<IFile_system> &fs,
                       const Dictionary_t &predefined_globals) {
  m_id = id;
  m_file_system = fs;
  m_predefined_globals = predefined_globals;
  m_execution_thread = std::make_unique<std::thread>(&JavaScript::run, this);

  std::unique_lock<std::mutex> lock(m_processing_state_mutex);
  m_processing_state_condition.wait(
      lock, [this]() { return m_processing_state.has_value(); });

  if (*m_processing_state == ProcessingState::Finished) {
    m_execution_thread->join();
    m_execution_thread.reset();
    return false;
  }

  return true;
}

void JavaScript::stop_run_thread() {
  // Pushes an empty code to indicate the running thread it should exit
  m_code.push({});
}

void JavaScript::stop() {
  if (m_execution_thread) {
    stop_run_thread();
    m_execution_thread->join();
    m_execution_thread.reset();
  }
}

int64_t JavaScript::eval(poly_reference source, poly_value *result) const {
  return poly_context_eval_source(thread(), context(), source, result);
}

poly_value JavaScript::create_source(const std::string &source,
                                     const std::string &code_str) const {
  poly_value builder;
  shcore::polyglot::throw_if_error(poly_create_source_builder, thread(),
                                   get_language_id(), source.c_str(),
                                   code_str.c_str(), &builder);

  shcore::polyglot::throw_if_error(poly_source_builder_set_mime_type, thread(),
                                   builder, "application/javascript+module");

  poly_value poly_source;
  shcore::polyglot::throw_if_error(poly_source_builder_build, thread(), builder,
                                   &poly_source);

  return poly_source;
}

void JavaScript::run() {
  my_thread_self_setname("Jit-Run");
  bool initialized = false;
  try {
    initialize(m_file_system);
    initialized = true;

    if (m_predefined_globals) {
      for (const auto &it : (*m_predefined_globals)) {
        set_global(it.first, it.second);
      }
    }

    set_global_function(
        "synch_return",
        shcore::polyglot::polyglot_handler_fixed_args<JavaScript, Synch_return>,
        this);

    set_global_function(
        "synch_error",
        shcore::polyglot::polyglot_handler_fixed_args<JavaScript, Synch_error>,
        this);

    set_global_function(
        "getSession",
        shcore::polyglot::native_handler_variable_args<JavaScript, Get_session>,
        this);

    set_global_function(
        "getCurrentMrsUserId",
        shcore::polyglot::polyglot_handler_no_args<JavaScript,
                                                   Get_current_mrs_user_id>,
        this);

    set_global_function(
        "getContentSetPath",
        shcore::polyglot::native_handler_fixed_args<JavaScript,
                                                    Get_content_set_path>,
        this);

    if (const auto rc = Java_script_interface::eval(
            "(internal)::resolver",
            R"(new Function ("prom", "prom.then(value => synch_return(value)).catch(error => synch_error(error));");)",
            &m_promise_resolver);
        rc != poly_ok) {
      throw Polyglot_error(thread(), rc);
    }
    set_processing_state(ProcessingState::Idle);
  } catch (const Polyglot_generic_error &err) {
    log_error("Error initializing JavaScript context (%zu): %s", m_id,
              err.what());
    set_processing_state(ProcessingState::Finished);
  }

  mysql_harness::ScopedCallback terminate([this, &initialized]() {
    if (initialized) {
      try {
        finalize();
      } catch (const std::exception &error) {
        log_error("Error finalizing JavaScript context: %s", error.what());
      }
    }
  });

  while (*m_processing_state != ProcessingState::Finished) {
    // The processing thread should not do anything until it is back to idle
    // state, including the consumption of the results produced in the last
    // processing round.
    wait_for_idle();

    auto entry = m_code.pop();

    if (!std::holds_alternative<Code>(entry)) {
      set_processing_state(ProcessingState::Finished);
      break;
    }

    set_processing_state(ProcessingState::Processing);

    const auto &code = std::get<Code>(entry);
    m_result_type = code.result_type;

    poly_value result = nullptr;

    // Creates a scope to ensure all the handles created on the execution are
    // wiped out right after finished
    shcore::polyglot::Polyglot_scope this_code_scope(thread());

    try {
      if (const auto rc =
              Java_script_interface::eval("(internal)", code.source, &result);
          rc != poly_ok) {
        throw Polyglot_error(thread(), rc);
      }

      // If it is a Promise, it needs to be resolved and the value will come
      // from synch_return
      if (std::string class_name;
          result && is_object(result, &class_name) && class_name == "Promise") {
        resolve_promise(result);
      } else {
        create_result(convert(result));
      }
    } catch (const Polyglot_error &error) {
      create_result(error);
    } catch (const std::exception &error) {
      create_result(Value(error.what()), ResultState::Error);
    }
  }
}

namespace {
[[maybe_unused]] void log_state(size_t id, ProcessingState state,
                                const std::string &other = "") {
  std::string st;
  switch (state) {
    case ProcessingState::Idle:
      st = "idle";
      break;
    case ProcessingState::Processing:
      st = "processing";
      break;
    case ProcessingState::Finished:
      st = "finished";
      break;
    case ProcessingState::HoldingResult:
      st = "holding_result";
      break;
  }

  if (!other.empty()) {
    st.append(", ").append(other);
  }

  log_error("JavaScript %zu: %s", id, st.c_str());
}
}  // namespace

void JavaScript::set_processing_state(ProcessingState state) {
  {
    std::scoped_lock lock{m_processing_state_mutex};

    m_processing_state = state;

    // log_state(m_id, state);
  }

  m_processing_state_condition.notify_one();
}

bool JavaScript::wait_for_idle() {
  bool idle = false;
  bool holding_results = false;
  {
    std::unique_lock lock{m_processing_state_mutex};

    // log_state(m_id, m_processing_state, "wait start");

    if (m_processing_state_condition.wait_for(
            lock, std::chrono::seconds(5), [this]() {
              return *m_processing_state != ProcessingState::Processing;
            })) {
      idle = *m_processing_state == ProcessingState::Idle;
      holding_results = *m_processing_state == ProcessingState::HoldingResult;
      if (m_processing_state == ProcessingState::HoldingResult) {
        Result to_discard;
        while (m_result.try_pop(to_discard)) {
          log_error("Releasing stalled result... %s",
                    to_discard.data.value_or("-").c_str());
        };
      }
    }
  }

  if (holding_results) {
    idle = true;
    set_processing_state(ProcessingState::Idle);
  }

  // log_state(m_id, m_processing_state, "wait end");

  return idle;
}

bool JavaScript::is_idle() {
  return *m_processing_state == ProcessingState::Idle;
}

Value JavaScript::native_array(poly_value object) {
  int64_t array_size{0};
  if (const auto rc = poly_value_get_array_size(thread(), object, &array_size);
      rc != poly_ok) {
    throw Polyglot_error(thread(), rc);
  }

  auto narray = std::make_shared<Value::Array_type>();
  narray->resize(array_size);

  for (int32_t c = array_size, i = 0; i < c; i++) {
    poly_value item;
    if (const auto rc =
            poly_value_get_array_element(thread(), object, i, &item);
        rc != poly_ok) {
      throw Polyglot_error(thread(), rc);
    }

    (*narray)[i] = convert(item);
  }

  return Value(std::move(narray));
}

Value JavaScript::native_object(poly_value object) {
  auto keys = shcore::polyglot::get_member_keys(thread(), context(), object);

  auto dict = shcore::make_dict();
  for (const auto &key : keys) {
    poly_value value;
    if (auto rc = poly_value_get_member(thread(), object, key.c_str(), &value);
        rc != poly_ok) {
      throw Polyglot_error(thread(), rc);
    }

    dict->set(key, convert(value));
  }

  return Value(std::move(dict));
}

Value JavaScript::to_native_object(poly_value object,
                                   const std::string &class_name) {
  if (class_name == "Array") {
    return native_array(object);
  } else if (class_name == "Object") {
    return native_object(object);
    // } else if (class_name == "Function") {
    //   return native_function(object);
  } else if (class_name == "Error") {
    poly_value poly_cause;
    if (auto rc = poly_value_get_member(thread(), object, "cause", &poly_cause);
        rc != poly_ok) {
      throw Polyglot_error(thread(), rc);
    }

    Value cause = convert(poly_cause);

    if (!cause.is_null() && cause.get_type() != Value_type::Map) {
      poly_value poly_message;
      if (const auto rc =
              poly_value_get_member(thread(), object, "message", &poly_message);
          rc != poly_ok) {
        throw Polyglot_error(thread(), rc);
      }
      cause = convert(poly_message);
    }

    return cause;
  }

  return Java_script_interface::to_native_object(object, class_name);
}

void JavaScript::output_handler(const char *bytes, size_t length) {
  log_info("%.*s", static_cast<int>(length), bytes);
}

void JavaScript::error_handler(const char *bytes, size_t length) {
  log_error("%.*s", static_cast<int>(length), bytes);
}

poly_value JavaScript::from_native_object(const Object_bridge_t &object) const {
  poly_value result = nullptr;
  if (object && object->class_name() == "Date") {
    std::shared_ptr<Date> date = std::static_pointer_cast<Date>(object);

    // 0 date values can come from MySQL but they're not supported by the JS
    // Date object, so we convert them to null
    if (date->has_date() && date->get_year() == 0 && date->get_month() == 0 &&
        date->get_day() == 0) {
      shcore::polyglot::throw_if_error(poly_create_null, thread(), context(),
                                       &result);
    } else if (!date->has_date()) {
      // there's no Time object in JS and we can't use Date to represent time
      // only
      std::string t;
      result = poly_string(date->append_descr(t));
    } else {
      auto source = shcore::str_format(
          "new Date(%d, %d, %d, %d, %d, %d, %d)", date->get_year(),
          date->get_month() - 1, date->get_day(), date->get_hour(),
          date->get_min(), date->get_sec(), date->get_usec() / 1000);

      Scoped_global builder(this);
      result = builder.execute(source);
    }
  }

  return result;
}

std::string JavaScript::get_parameter_string(
    const std::vector<Value> &parameters) const {
  std::string parameter_string;

  for (const auto &param : parameters) {
    if (!parameter_string.empty()) {
      parameter_string += ",";
    }

    switch (param.get_type()) {
      case Value_type::Undefined:
        parameter_string += "undefined";
        break;
      case Value_type::Null:
        parameter_string += "null";
        break;
        // TODO(rennox): Binary conversion...
      case Value_type::String:
        parameter_string += shcore::quote_string(param.descr(), '`');
        break;
      default:
        parameter_string += param.descr();
    }
  }

  return parameter_string;
}

std::string JavaScript::execute(const std::string &code, int timeout,
                                ResultType result_type,
                                const GlobalCallbacks &global_callbacks) {
  ProcessingState next_state = ProcessingState::Idle;
  mysql_harness::ScopedCallback set_state(
      [this, &next_state]() { set_processing_state(next_state); });

  clear_is_terminating();

  auto ms_timeout = std::chrono::milliseconds{timeout};

  m_global_callbacks = &global_callbacks;
  m_code.push(Code{code, result_type});

  mysql_harness::ScopedCallback clean_resources([this]() {
    if (m_session) {
      m_session->reset();
    }
    m_session.reset();
    m_global_callbacks = nullptr;
  });

  Result result;
  if (!m_debug_port.empty()) {
    // We don't want timeouts when debugging...
    result = m_result.pop();
  } else {
    m_result.try_pop(result, ms_timeout);
  }

  if (result.state.has_value()) {
    switch (*result.state) {
      case ResultState::Ok:
        return *result.data;
      case ResultState::Error:
        throw std::runtime_error(*result.data);
      case ResultState::ResourceExhausted:
        throw MemoryError(*result.data);
    }
  }

  // At this point, the expected result was not consumed and will arrive to the
  // queue, so we should handle it
  next_state = ProcessingState::HoldingResult;

  // This logic is reached if a Timeout occurred, the time out handling
  // includes:
  // - Calling the interrupt callback which may release resources being used
  // during the code execution.
  // - calling terminate() which will tell the JavaScript context to terminate
  // any code in execution.

  // NOTE that at this point, there's still a result pending and the actions
  // listed above will make that result to come up, most probably with an error,
  // which will be simply ignored as we will raise Time-Out error for the
  // client.
  if (global_callbacks.interrupt) {
    global_callbacks.interrupt();
  }

  throw TimeoutError("Timeout");
}

poly_value JavaScript::synch_return(const std::vector<poly_value> &args) {
  std::string class_name;

  // If we got a promise (i.e. chained one) we wait for it to be resolved too
  if (args[0] && is_object(args[0], &class_name) && class_name == "Promise") {
    resolve_promise(args[0]);
  } else {
    try {
      // If we got a module, it is automatically resolved as a Polyglot_object
      // i.e. import('<module-path>')
      if (class_name == "[object Module]") {
        create_result(to_native_object(args[0], class_name));
      } else {
        create_result(convert(args[0]));
      }
    } catch (const Polyglot_error &error) {
      create_result(error);
    } catch (const std::exception &error) {
      create_result(Value(error.what()), ResultState::Error);
    }
  }

  return nullptr;
}

poly_value JavaScript::synch_error(const std::vector<poly_value> &args) {
  try {
    create_result(convert(args[0]), ResultState::Error);
  } catch (const Polyglot_error &error) {
    create_result(error);
  } catch (const std::exception &error) {
    create_result(Value(error.what()), ResultState::Error);
  }

  return nullptr;
}

void JavaScript::resolve_promise(poly_value promise) {
  std::array<poly_value, 1> args{promise};

  if (const auto rc = poly_value_execute(thread(), m_promise_resolver,
                                         args.data(), args.size(), nullptr);
      rc != poly_ok) {
    throw Polyglot_error(thread(), rc);
  }
}

shcore::Value JavaScript::get_session(const std::vector<shcore::Value> &args) {
  if (!m_global_callbacks) {
    throw std::runtime_error("Missing callbacks...");
  }

  bool read_only = true;
  if (args.size() > 1) {
    throw std::runtime_error(
        shcore::str_format("getSession(bool readOnly) takes up to 1 argument"));
  } else if (!args.empty()) {
    read_only = args[0].as_bool();
  }

  return shcore::Value(std::make_shared<shcore::polyglot::Session>(
      m_global_callbacks->get_session(read_only)));
}

poly_value JavaScript::get_current_mrs_user_id() {
  if (!m_global_callbacks) {
    throw std::runtime_error("Missing callbacks...");
  }

  std::optional<std::string> user_id;
  user_id = m_global_callbacks->get_current_mrs_user_id();

  if (user_id.has_value()) {
    return convert(Value(*user_id));
  } else {
    return undefined();
  }
}

shcore::Value JavaScript::get_content_set_path(
    const std::vector<shcore::Value> &args) {
  if (!m_global_callbacks) {
    throw std::runtime_error("Missing callbacks...");
  }

  return shcore::Value(
      m_global_callbacks->get_content_set_path(args[0].as_string()));
}

}  // namespace jit_executor