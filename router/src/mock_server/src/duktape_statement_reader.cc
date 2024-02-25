/*
  Copyright (c) 2018, 2023, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <chrono>
#include <map>
#include <stdexcept>
#include <string>
#include <system_error>

#ifdef _WIN32
#include <WinSock2.h>
#include <Windows.h>
#endif

#include <mysqld_error.h>
#include <openssl/ssl.h>

#include "duk_logging.h"
#include "duk_module_shim.h"
#include "duk_node_fs.h"
#include "duktape.h"
#include "duktape_statement_reader.h"
#include "harness_assert.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/stdx/expected.h"
#include "mysqlrouter/classic_protocol.h"
#include "statement_reader.h"

IMPORT_LOG_FUNCTIONS()

namespace server_mock {

std::unique_ptr<StatementReaderBase>
DuktapeStatementReaderFactory::operator()() {
  try {
    return std::make_unique<DuktapeStatementReader>(filename_, module_prefixes_,
                                                    session_, global_scope_);
  } catch (const std::exception &ex) {
    log_warning("%s", ex.what());
    return std::make_unique<FailedStatementReader>(ex.what());
  }
}

static duk_int_t process_get_shared(duk_context *ctx);
static duk_int_t process_set_shared(duk_context *ctx);
static duk_int_t process_get_keys(duk_context *ctx);
static void check_stmts_section(duk_context *ctx);
static bool check_notices_section(duk_context *ctx);
static void check_handshake_section(duk_context *ctx);
static duk_int_t process_erase(duk_context *ctx);
static duk_int_t process_set_shared(duk_context *ctx);
duk_int_t duk_pcompile_file(duk_context *ctx, const char *path,
                            int compile_type);

/*
 * get the names of the type.
 *
 * returns a comma-separated string
 *
 * useful for debugging
 */
static std::string duk_get_type_names(duk_context *ctx, duk_idx_t ndx) {
  std::string names;
  bool is_first = true;

  std::vector<
      std::pair<std::function<bool(duk_context *, duk_idx_t)>, std::string>>
      type_checks = {
          {duk_is_array, "array"},
          {duk_is_boolean, "boolean"},
          {duk_is_buffer, "buffer"},
          {duk_is_buffer_data, "buffer_data"},
          {duk_is_c_function, "c-function"},
          {duk_is_dynamic_buffer, "dynamic-buffer"},
          {[](duk_context *_ctx, duk_idx_t _ndx) -> bool {
             return duk_is_callable(_ctx, _ndx);
           },
           "callable"},
          {[](duk_context *_ctx, duk_idx_t _ndx) -> bool {
             return duk_is_error(_ctx, _ndx);
           },
           "error"},
          {duk_is_function, "function"},
          {duk_is_ecmascript_function, "ecmascript-function"},
          {duk_is_null, "null"},
          {duk_is_number, "number"},
          {duk_is_object, "object"},
          {duk_is_pointer, "pointer"},
          {[](duk_context *_ctx, duk_idx_t _ndx) -> bool {
             return duk_is_primitive(_ctx, _ndx);
           },
           "primitive"},
          {duk_is_string, "string"},
          {duk_is_symbol, "symbol"},
          {duk_is_thread, "thread"},
          {duk_is_undefined, "undefined"},
      };

  for (auto &check : type_checks) {
    if (check.first(ctx, ndx)) {
      if (is_first) {
        is_first = false;
      } else {
        names.append(", ");
      }

      names.append(check.second);
    }
  }

  return names;
}

class DuktapeRuntimeError : public std::runtime_error {
 public:
  static std::string what_from_error(duk_context *ctx,
                                     duk_idx_t error_ndx) noexcept {
    if (duk_is_error(ctx, error_ndx)) {
      duk_get_prop_string(ctx, -1, "stack");
      std::string err_stack{duk_safe_to_string(ctx, error_ndx)};
      duk_pop(ctx);
      duk_get_prop_string(ctx, -1, "fileName");
      std::string err_filename{duk_safe_to_string(ctx, error_ndx)};
      duk_pop(ctx);
      duk_get_prop_string(ctx, -1, "lineNumber");
      std::string err_fileline{duk_safe_to_string(ctx, error_ndx)};
      duk_pop(ctx);

      duk_pop(ctx);  // error-obj

      return "at " + err_filename + ":" + err_fileline + ": " + err_stack;
    } else {
      std::string err_msg{duk_safe_to_string(ctx, error_ndx)};

      duk_pop(ctx);  // error-obj

      return err_msg;
    }
  }
  DuktapeRuntimeError(duk_context *ctx, duk_idx_t error_ndx)
      : std::runtime_error{what_from_error(ctx, error_ndx)} {}
};

MySQLColumnType column_type_from_string(const std::string &type) {
  int res = 0;

  try {
    res = std::stoi(type);
  } catch (const std::invalid_argument &) {
    if (type == "DECIMAL") return MySQLColumnType::DECIMAL;
    if (type == "TINY") return MySQLColumnType::TINY;
    if (type == "SHORT") return MySQLColumnType::SHORT;
    if (type == "LONG") return MySQLColumnType::LONG;
    if (type == "INT24") return MySQLColumnType::INT24;
    if (type == "LONGLONG") return MySQLColumnType::LONGLONG;
    if (type == "DECIMAL") return MySQLColumnType::DECIMAL;
    if (type == "NEWDECIMAL") return MySQLColumnType::NEWDECIMAL;
    if (type == "FLOAT") return MySQLColumnType::FLOAT;
    if (type == "DOUBLE") return MySQLColumnType::DOUBLE;
    if (type == "BIT") return MySQLColumnType::BIT;
    if (type == "TIMESTAMP") return MySQLColumnType::TIMESTAMP;
    if (type == "DATE") return MySQLColumnType::DATE;
    if (type == "TIME") return MySQLColumnType::TIME;
    if (type == "DATETIME") return MySQLColumnType::DATETIME;
    if (type == "YEAR") return MySQLColumnType::YEAR;
    if (type == "STRING") return MySQLColumnType::STRING;
    if (type == "VAR_STRING") return MySQLColumnType::VAR_STRING;
    if (type == "BLOB") return MySQLColumnType::BLOB;
    if (type == "SET") return MySQLColumnType::SET;
    if (type == "ENUM") return MySQLColumnType::ENUM;
    if (type == "GEOMETRY") return MySQLColumnType::GEOMETRY;
    if (type == "NULL") return MySQLColumnType::NULL_;
    if (type == "TINYBLOB") return MySQLColumnType::TINY_BLOB;
    if (type == "LONGBLOB") return MySQLColumnType::LONG_BLOB;
    if (type == "MEDIUMBLOB") return MySQLColumnType::MEDIUM_BLOB;

    throw std::invalid_argument("Unknown type: \"" + type + "\"");
  }

  return static_cast<MySQLColumnType>(res);
}

/**
 * memory heap of duk contexts.
 *
 * contains:
 *
 * - execution threads
 * - stacks
 * - stashes
 * - ...
 */
class DukHeap {
 public:
  DukHeap(std::vector<std::string> module_prefixes,
          std::shared_ptr<MockServerGlobalScope> shared_globals)
      : heap_{duk_create_heap(nullptr, nullptr, nullptr, nullptr,
                              [](void *, const char *msg) {
                                log_error("%s", msg);
                                abort();
                              })},
        shared_{std::move(shared_globals)} {
    duk_module_shim_init(context(), module_prefixes);
  }

  void prepare(
      std::string filename,
      std::map<std::string, std::function<std::string()>> &session_data) {
    auto ctx = context();
    duk_push_global_stash(ctx);
    if (nullptr == shared_.get()) {
      // why is the shared-ptr empty?
      throw std::logic_error(
          "expected shared global variable object to be set, but it isn't.");
    }

    // process.*
    prepare_process_object();

    // mysqld.*
    prepare_mysqld_object(session_data);

    load_script(filename);

    if (!duk_is_object(ctx, -1)) {
      throw std::runtime_error(
          filename + ": expected statement handler to return an object, got " +
          duk_get_type_names(ctx, -1));
    }

    // check if the sections have the right types
    check_stmts_section(ctx);
    check_handshake_section(ctx);
  }

  duk_context *context() { return heap_.get(); }

 private:
  class HeapDeleter {
   public:
    void operator()(duk_context *p) { duk_destroy_heap(p); }
  };

  void prepare_process_object() {
    auto ctx = context();

    duk_push_pointer(ctx, shared_.get());
    duk_put_prop_string(ctx, -2, "shared");
    duk_pop(ctx);  // stash

    duk_get_global_string(ctx, "process");
    if (duk_is_undefined(ctx, -1)) {
      // duk_module_shim_init() is expected to initialize it.
      throw std::runtime_error(
          "expected 'process' to exist, but it is undefined.");
    }
    duk_push_c_function(ctx, process_get_shared, 1);
    duk_put_prop_string(ctx, -2, "get_shared");

    duk_push_c_function(ctx, process_set_shared, 2);
    duk_put_prop_string(ctx, -2, "set_shared");

    duk_push_c_function(ctx, process_get_keys, 0);
    duk_put_prop_string(ctx, -2, "get_keys");

    duk_push_c_function(ctx, process_erase, 1);
    duk_put_prop_string(ctx, -2, "erase");

    duk_pop(ctx);
  }

  void prepare_mysqld_object(
      const std::map<std::string, std::function<std::string()>> &session_data) {
    auto ctx = context();
    // mysqld = {
    //   session: {
    //     port: 3306
    //   }
    //   global: // proxy that calls process.get_shared()/.set_shared()
    // }
    duk_push_global_object(ctx);
    duk_push_object(ctx);
    duk_push_object(ctx);

    // map of string and json-string
    for (const auto &el : session_data) {
      const std::string val = el.second();
      duk_push_lstring(ctx, val.data(), val.size());
      duk_json_decode(ctx, -1);
      duk_put_prop_lstring(ctx, -2, el.first.data(), el.first.size());
    }

    duk_put_prop_string(ctx, -2, "session");

    if (DUK_EXEC_SUCCESS !=
        duk_pcompile_string(ctx, DUK_COMPILE_FUNCTION,
                            "function () {\n"
                            "  return new Proxy({}, {\n"
                            "    ownKeys: function(target) {\n"
                            "      process.get_keys().forEach(function(el) {\n"
                            "        Object.defineProperty(\n"
                            "          target, el, {\n"
                            "            configurable: true,\n"
                            "            enumerable: true});\n"
                            "      });\n"
                            "      return Object.keys(target);\n"
                            "    },\n"
                            "    get: function(target, key, recv) {\n"
                            "      return process.get_shared(key);},\n"
                            "    set: function(target, key, val, recv) {\n"
                            "      return process.set_shared(key, val);},\n"
                            "    deleteProperty: function(target, prop) {\n"
                            "      if (process.erase(prop) > 0) {\n"
                            "        delete target[prop];\n"
                            "      }\n"
                            "    },\n"
                            "  });\n"
                            "}\n")) {
      throw DuktapeRuntimeError(ctx, -1);
    }
    if (DUK_EXEC_SUCCESS != duk_pcall(ctx, 0)) {
      throw DuktapeRuntimeError(ctx, -1);
    }

    duk_put_prop_string(ctx, -2, "global");

    duk_put_prop_string(ctx, -2, "mysqld");
  }

  void load_script(const std::string &filename) {
    std::lock_guard<std::mutex> lk(scripts_mtx_);

    auto ctx = context();
    if (scripts_.find(filename) != scripts_.end()) {
      // use the cached version of the script.
      const auto &buffer = scripts_[filename];

      auto *p = duk_push_fixed_buffer(ctx, buffer.size());
      std::copy(buffer.begin(), buffer.end(), static_cast<char *>(p));
    } else {
      // load the script.
      if (DUK_EXEC_SUCCESS !=
          duk_pcompile_file(ctx, filename.c_str(), DUK_COMPILE_EVAL)) {
        throw DuktapeRuntimeError(ctx, -1);
      }
// On Solaris we disable cache functionality as it causes mock server crash:
// uncaught: 'invalid bytecode'
//     Application got fatal signal: 6
//              server_mock::DukHeap::DukHeap(std::string const&,
//              std::shared_ptr<MockServerGlobalScope>)::{lambda(void*, char
//              const*)#1}::__invoke(char const, {lambda(void*, char
//              const*)#1})+0x28 [0xffffff02c0133bc8]
#ifndef __sun
      duk_dump_function(ctx);
      // store the compiled bytecode in our cache
      size_t sz;
      const auto *p = static_cast<const char *>(duk_get_buffer(ctx, -1, &sz));
      scripts_[filename] = std::string(p, p + sz);
#endif
    }

#ifndef __sun
    duk_load_function(ctx);
#endif

    duk_push_global_object(ctx);
    if (DUK_EXEC_SUCCESS != duk_pcall_method(ctx, 0)) {
      throw DuktapeRuntimeError(ctx, -1);
    }
  }

  std::unique_ptr<duk_context, HeapDeleter> heap_{};
  std::shared_ptr<MockServerGlobalScope> shared_;

  static std::mutex scripts_mtx_;
  static std::map<std::string, std::string> scripts_;
};

/*static*/ std::map<std::string, std::string> DukHeap::scripts_{};
/*static*/ std::mutex DukHeap::scripts_mtx_{};

class DukHeapPool {
 public:
  static DukHeapPool *instance() { return &instance_; }

  std::unique_ptr<DukHeap> get(
      const std::string &filename, std::vector<std::string> module_prefixes,
      std::map<std::string, std::function<std::string()>> session_data,
      std::shared_ptr<MockServerGlobalScope> shared_globals) {
    {
      std::lock_guard<std::mutex> lock(pool_mtx_);
      if (pool_.size() > 0) {
        auto result = std::move(pool_.front());
        pool_.pop_front();
        result->prepare(filename, session_data);
        return result;
      }
    }

    // there is no free context object, create new one
    auto result =
        std::make_unique<DukHeap>(std::move(module_prefixes), shared_globals);
    result->prepare(filename, session_data);

    return result;
  }

  void release(std::unique_ptr<DukHeap> heap) {
    std::lock_guard<std::mutex> lock(pool_mtx_);
    pool_.push_back(std::move(heap));
  }

 private:
  DukHeapPool() = default;
  static DukHeapPool instance_;

  std::list<std::unique_ptr<DukHeap>> pool_;
  std::mutex pool_mtx_;
};

DukHeapPool DukHeapPool::instance_;

struct DuktapeStatementReader::Pimpl {
  Pimpl(std::unique_ptr<DukHeap> heap)
      : heap_(std::move(heap)), ctx(heap_->context()) {}

  std::string get_object_string_value(duk_idx_t idx, const std::string &field,
                                      const std::string &default_val = "",
                                      bool is_required = false) {
    std::string value;

    duk_get_prop_string(ctx, idx, field.c_str());

    if (duk_is_undefined(ctx, -1)) {
      if (is_required) {
        throw std::runtime_error(
            "Wrong statements document structure: missing field \"" + field +
            "\"");
      }

      value = default_val;
    } else {
      value = duk_to_string(ctx, -1);
    }

    duk_pop(ctx);

    return value;
  }

  template <class INT_TYPE>
  typename std::enable_if<std::is_unsigned<INT_TYPE>::value, INT_TYPE>::type
  get_object_integer_value(duk_idx_t idx, const std::string &field,
                           const INT_TYPE default_val = 0,
                           bool is_required = false) {
    INT_TYPE value;

    duk_get_prop_string(ctx, idx, field.c_str());

    if (duk_is_undefined(ctx, -1)) {
      if (is_required) {
        throw std::runtime_error(
            "Wrong statements document structure: missing field \"" + field +
            "\"");
      }

      value = default_val;
    } else if (duk_is_number(ctx, -1)) {
      if (duk_get_number(ctx, -1) < std::numeric_limits<INT_TYPE>::min()) {
        throw std::runtime_error("value out-of-range for field \"" + field +
                                 "\"");
      }
      if (duk_get_number(ctx, -1) > std::numeric_limits<INT_TYPE>::max()) {
        throw std::runtime_error("value out-of-range for field \"" + field +
                                 "\"");
      }
      value = duk_to_uint(ctx, -1);
    } else {
      throw std::runtime_error("wrong type for field \"" + field +
                               "\", expected unsigned number");
    }

    duk_pop(ctx);

    return value;
  }

  OkResponse get_ok(duk_idx_t idx) {
    if (!duk_is_object(ctx, idx)) {
      throw std::runtime_error("expect an object");
    }

    return {get_object_integer_value<uint32_t>(-1, "affected_rows", 0),
            get_object_integer_value<uint32_t>(-1, "last_insert_id", 0),
            get_object_integer_value<uint16_t>(-1, "status", 0),
            get_object_integer_value<uint16_t>(-1, "warning_count", 0)};
  }

  ErrorResponse get_error(duk_idx_t idx) {
    if (!duk_is_object(ctx, idx)) {
      throw std::runtime_error("expect an object");
    }

    return {get_object_integer_value<uint16_t>(-1, "code", 0, true),
            get_object_string_value(-1, "message", "", true),
            get_object_string_value(-1, "sql_state", "HY000")};
  }

  ResultsetResponse get_result(duk_idx_t idx) {
    ResultsetResponse response;
    if (!duk_is_object(ctx, idx)) {
      throw std::runtime_error("expect an object");
    }
    duk_get_prop_string(ctx, idx, "columns");

    if (!duk_is_array(ctx, idx)) {
      throw std::runtime_error("expect an object");
    }
    // iterate over the column meta
    duk_enum(ctx, -1, DUK_ENUM_ARRAY_INDICES_ONLY);
    while (duk_next(ctx, -1, 1)) {
      // @-2 column-ndx
      // @-1 column

      if (duk_get_prop_string(ctx, -1, "repeat")) {
        throw std::runtime_error("repeat is not supported");
      }
      duk_pop(ctx);

      response.columns.emplace_back(
          get_object_string_value(-1, "catalog", "def"),
          get_object_string_value(-1, "schema"),
          get_object_string_value(-1, "table"),
          get_object_string_value(-1, "orig_table"),
          get_object_string_value(-1, "name", "", true),
          get_object_string_value(-1, "orig_name"),
          get_object_integer_value<uint16_t>(-1, "character_set", 0xff),
          get_object_integer_value<uint32_t>(-1, "length"),
          static_cast<uint8_t>(column_type_from_string(
              get_object_string_value(-1, "type", "", true))),
          get_object_integer_value<uint16_t>(-1, "flags"),
          get_object_integer_value<uint8_t>(-1, "decimals"));

      duk_pop(ctx);  // row
      duk_pop(ctx);  // row-ndx
    }
    duk_pop(ctx);  // rows-enum

    duk_pop(ctx);
    duk_get_prop_string(ctx, idx, "rows");

    // object|undefined
    if (duk_is_object(ctx, -1)) {
      // no rows

      duk_enum(ctx, -1, DUK_ENUM_ARRAY_INDICES_ONLY);
      while (duk_next(ctx, -1, 1)) {
        // @-2 row-ndx
        // @-1 row
        RowValueType row_values;

        duk_enum(ctx, -1, DUK_ENUM_ARRAY_INDICES_ONLY);
        while (duk_next(ctx, -1, 1)) {
          if (duk_is_null(ctx, -1)) {
            row_values.emplace_back(std::nullopt);
          } else {
            row_values.emplace_back(duk_to_string(ctx, -1));
          }
          duk_pop(ctx);  // field
          duk_pop(ctx);  // field-ndx
        }
        duk_pop(ctx);  // field-enum
        response.rows.push_back(row_values);

        duk_pop(ctx);  // row
        duk_pop(ctx);  // row-ndx
      }
      duk_pop(ctx);  // rows-enum
    } else if (!duk_is_undefined(ctx, -1)) {
      throw std::runtime_error("rows: expected array or undefined, get " +
                               duk_get_type_names(ctx, -1));
    }

    duk_pop(ctx);  // "rows"

    return response;
  }

  enum class HandshakeState {
    INIT,
    GREETED,
    AUTH_SWITCHED,
    AUTH_FASTED,
    DONE
  } handshake_state_{HandshakeState::INIT};

  classic_protocol::capabilities::value_type server_capabilities_;

  bool first_stmt_{true};

  std::string nonce_;
  std::unique_ptr<DukHeap> heap_;
  duk_context *ctx{nullptr};
};

duk_int_t duk_pcompile_file(duk_context *ctx, const char *path,
                            int compile_type) {
  duk_push_c_function(ctx, duk_node_fs_read_file_sync, 1);
  duk_push_string(ctx, path);
  if (duk_int_t rc = duk_pcall(ctx, 1)) {
    return rc;
  }

  duk_buffer_to_string(ctx, -1);
  duk_push_string(ctx, path);
  if (duk_int_t rc = duk_pcompile(ctx, compile_type)) {
    return rc;
  }

  return 0;
}

static duk_int_t process_get_keys(duk_context *ctx) {
  duk_push_global_stash(ctx);
  duk_get_prop_string(ctx, -1, "shared");
  auto *shared_globals =
      static_cast<MockServerGlobalScope *>(duk_get_pointer(ctx, -1));

  duk_push_array(ctx);
  size_t ndx{0};
  for (const auto &key : shared_globals->get_keys()) {
    duk_push_lstring(ctx, key.data(), key.size());
    duk_put_prop_index(ctx, -2, ndx++);
  }

  duk_remove(ctx, -2);  // 'shared' pointer
  duk_remove(ctx, -2);  // global stash

  return 1;
}

static duk_int_t process_get_shared(duk_context *ctx) {
  const char *key = duk_require_string(ctx, 0);

  duk_push_global_stash(ctx);
  duk_get_prop_string(ctx, -1, "shared");
  auto *shared_globals =
      static_cast<MockServerGlobalScope *>(duk_get_pointer(ctx, -1));

  auto v = shared_globals->get_all();

  auto it = v.find(key);
  if (it == v.end()) {
    duk_push_undefined(ctx);
  } else {
    auto value = (*it).second;
    duk_push_lstring(ctx, value.c_str(), value.size());
    duk_json_decode(ctx, -1);
  }

  duk_remove(ctx, -2);  // 'shared' pointer
  duk_remove(ctx, -2);  // global stash

  return 1;
}

static duk_int_t process_erase(duk_context *ctx) {
  const char *key = duk_require_string(ctx, 0);

  duk_push_global_stash(ctx);
  duk_get_prop_string(ctx, -1, "shared");
  auto *shared_globals =
      static_cast<MockServerGlobalScope *>(duk_get_pointer(ctx, -1));

  duk_push_int(ctx, shared_globals->erase(key));

  duk_remove(ctx, -2);  // 'shared' pointer
  duk_remove(ctx, -2);  // global stash

  return 1;
}

static duk_int_t process_set_shared(duk_context *ctx) {
  const char *key = duk_require_string(ctx, 0);
  duk_require_valid_index(ctx, 1);

  duk_push_global_stash(ctx);
  duk_get_prop_string(ctx, -1, "shared");
  auto *shared_globals =
      static_cast<MockServerGlobalScope *>(duk_get_pointer(ctx, -1));

  if (nullptr == shared_globals) {
    return duk_generic_error(ctx, "shared is null");
  }

  duk_dup(ctx, 1);
  shared_globals->set(key, duk_json_encode(ctx, -1));

  duk_pop(ctx);  // the dup
  duk_pop(ctx);  // 'shared' pointer
  duk_pop(ctx);  // global

  return 0;
}

static void check_stmts_section(duk_context *ctx) {
  duk_get_prop_string(ctx, -1, "stmts");
  if (!(duk_is_callable(ctx, -1) || duk_is_thread(ctx, -1) ||
        duk_is_array(ctx, -1))) {
    throw std::runtime_error(
        "expected 'stmts' to be one of callable, thread or array, "
        "got " +
        duk_get_type_names(ctx, -1));
  }
  duk_pop(ctx);
}

static bool check_notices_section(duk_context *ctx) {
  duk_get_prop_string(ctx, -1, "notices");
  bool has_notices = !duk_is_null_or_undefined(ctx, -1);
  if (has_notices && (!(duk_is_callable(ctx, -1) || duk_is_thread(ctx, -1) ||
                        duk_is_array(ctx, -1)))) {
    throw std::runtime_error(
        "expected 'notices' to be one of callable, thread or array, "
        "got " +
        duk_get_type_names(ctx, -1));
  }
  duk_pop(ctx);

  return has_notices;
}

static void check_handshake_section(duk_context *ctx) {
  duk_get_prop_string(ctx, -1, "handshake");
  if (!duk_is_undefined(ctx, -1)) {
    if (!duk_is_object(ctx, -1)) {
      throw std::runtime_error("handshake must be an object, if set. Is " +
                               duk_get_type_names(ctx, -1));
    }
    duk_get_prop_string(ctx, -1, "greeting");
    if (!duk_is_undefined(ctx, -1)) {
      if (!duk_is_object(ctx, -1)) {
        throw std::runtime_error(
            "handshake.greeting must be an object, if set. Is " +
            duk_get_type_names(ctx, -1));
      }
      duk_get_prop_string(ctx, -1, "exec_time");
      if (!duk_is_undefined(ctx, -1)) {
        if (!duk_is_number(ctx, -1)) {
          throw std::runtime_error("exec_time must be a number, if set. Is " +
                                   duk_get_type_names(ctx, -1));
        }
      }
      duk_pop(ctx);
    }
    duk_pop(ctx);
  }
  duk_pop(ctx);
}

DuktapeStatementReader::DuktapeStatementReader(
    std::string filename, std::vector<std::string> module_prefixes,
    std::map<std::string, std::function<std::string()>> session_data,
    std::shared_ptr<MockServerGlobalScope> shared_globals)
    : pimpl_{std::make_unique<Pimpl>(DukHeapPool::instance()->get(
          std::move(filename), std::move(module_prefixes), session_data,
          shared_globals))} {
  auto ctx = pimpl_->ctx;
  has_notices_ = check_notices_section(ctx);
}

// must be declared here as Pimpl an incomplete type in the header

DuktapeStatementReader::DuktapeStatementReader(DuktapeStatementReader &&) =
    default;

DuktapeStatementReader &DuktapeStatementReader::operator=(
    DuktapeStatementReader &&) = default;

DuktapeStatementReader::~DuktapeStatementReader() {
  DukHeapPool::instance()->release(std::move(pimpl_->heap_));
}

stdx::expected<classic_protocol::message::server::Greeting, std::error_code>
DuktapeStatementReader::server_greeting(bool with_tls) {
  auto *ctx = pimpl_->ctx;

  // defaults
  std::string server_version = "8.0.23-mock";
  uint32_t connection_id = 0;
  classic_protocol::capabilities::value_type server_capabilities =
      classic_protocol::capabilities::long_password |
      classic_protocol::capabilities::found_rows |
      classic_protocol::capabilities::long_flag |
      classic_protocol::capabilities::connect_with_schema |
      classic_protocol::capabilities::no_schema |
      // compress (not yet)
      classic_protocol::capabilities::odbc |
      classic_protocol::capabilities::local_files |
      // ignore_space (client only)
      classic_protocol::capabilities::protocol_41 |
      // interactive (client-only)
      // ssl (below)
      // ignore sigpipe (client-only)
      classic_protocol::capabilities::transactions |
      classic_protocol::capabilities::secure_connection |
      // multi_statements (not yet)
      classic_protocol::capabilities::multi_results |
      classic_protocol::capabilities::ps_multi_results |
      classic_protocol::capabilities::plugin_auth |
      classic_protocol::capabilities::connect_attributes |
      classic_protocol::capabilities::client_auth_method_data_varint |
      classic_protocol::capabilities::expired_passwords |
      classic_protocol::capabilities::session_track |
      classic_protocol::capabilities::text_result_with_session_tracking
      // optional_resultset_metadata (not yet)
      // compress_zstd (not yet)
      ;

  if (with_tls) {
    server_capabilities |= classic_protocol::capabilities::ssl;
  }

  uint16_t status_flags = 0;
  uint8_t character_set = 0;
  std::string auth_method = MySQLNativePassword::name;
  std::string nonce = "01234567890123456789";

  duk_get_prop_string(ctx, -1, "handshake");
  if (!duk_is_undefined(ctx, -1)) {
    if (!duk_is_object(ctx, -1)) {
      throw std::runtime_error("handshake must be an object, if set. Is " +
                               duk_get_type_names(ctx, -1));
    }
    duk_get_prop_string(ctx, -1, "greeting");
    if (!duk_is_undefined(ctx, -1)) {
      if (!duk_is_object(ctx, -1)) {
        throw std::runtime_error(
            "handshake.greeting must be an object, if set. Is " +
            duk_get_type_names(ctx, -1));
      }

      server_version =
          pimpl_->get_object_string_value(-1, "server_version", server_version);
      connection_id = pimpl_->get_object_integer_value<uint32_t>(
          -1, "connection_id", connection_id);
      status_flags = pimpl_->get_object_integer_value<uint16_t>(
          -1, "status_flags", status_flags);
      character_set = pimpl_->get_object_integer_value<uint8_t>(
          -1, "character_set", character_set);
      server_capabilities = pimpl_->get_object_integer_value<uint32_t>(
          -1, "capabilities", server_capabilities.to_ulong());
      auth_method =
          pimpl_->get_object_string_value(-1, "auth_method", auth_method);
      nonce = pimpl_->get_object_string_value(-1, "nonce", nonce);
    }
    duk_pop(ctx);
  }
  duk_pop(ctx);

  return {std::in_place,
          0x0a,
          server_version,
          connection_id,
          nonce + std::string(1, '\0'),
          server_capabilities,
          character_set,
          status_flags,
          auth_method};
}

std::chrono::microseconds DuktapeStatementReader::server_greeting_exec_time() {
  std::chrono::microseconds exec_time{};

  auto *ctx = pimpl_->ctx;

  duk_get_prop_string(ctx, -1, "handshake");
  if (!duk_is_undefined(ctx, -1)) {
    if (!duk_is_object(ctx, -1)) {
      throw std::runtime_error("handshake must be an object, if set. Is " +
                               duk_get_type_names(ctx, -1));
    }
    duk_get_prop_string(ctx, -1, "greeting");
    if (!duk_is_undefined(ctx, -1)) {
      if (!duk_is_object(ctx, -1)) {
        throw std::runtime_error(
            "handshake.greeting must be an object, if set. Is " +
            duk_get_type_names(ctx, -1));
      }

      duk_get_prop_string(ctx, -1, "exec_time");
      if (!duk_is_undefined(ctx, -1)) {
        if (!duk_is_number(ctx, -1)) {
          throw std::runtime_error("exec_time must be a number, if set. Is " +
                                   duk_get_type_names(ctx, -1));
        }
        if (duk_get_number(ctx, -1) < 0) {
          throw std::out_of_range("exec_time must be a non-negative number");
        }

        // exec_time is written in the tracefile as microseconds
        exec_time = std::chrono::microseconds(
            static_cast<long>(duk_get_number(ctx, -1) * 1000));
      }
      duk_pop(ctx);
    }
    duk_pop(ctx);
  }
  duk_pop(ctx);

  return exec_time;
}

stdx::expected<DuktapeStatementReader::handshake_data, ErrorResponse>
DuktapeStatementReader::handshake() {
  auto *ctx = pimpl_->ctx;

  std::optional<ErrorResponse> error;

  std::optional<std::string> username;
  std::optional<std::string> password;
  bool cert_required{false};
  std::optional<std::string> cert_issuer;
  std::optional<std::string> cert_subject;

  std::error_code ec{};

  duk_get_prop_string(ctx, -1, "handshake");
  if (duk_is_object(ctx, -1)) {
    duk_get_prop_string(ctx, -1, "error");
    if (!duk_is_undefined(ctx, -1)) {
      error = pimpl_->get_error(-1);
    }
    duk_pop(ctx);

    duk_get_prop_string(ctx, -1, "auth");
    if (duk_is_object(ctx, -1)) {
      duk_get_prop_literal(ctx, -1, "username");
      if (duk_is_string(ctx, -1)) {
        username = std::string(duk_to_string(ctx, -1));
      } else if (!duk_is_undefined(ctx, -1)) {
        ec = make_error_code(std::errc::invalid_argument);
      }
      duk_pop(ctx);

      duk_get_prop_literal(ctx, -1, "password");
      if (duk_is_string(ctx, -1)) {
        password = std::string(duk_to_string(ctx, -1));
      } else if (!duk_is_undefined(ctx, -1)) {
        ec = make_error_code(std::errc::invalid_argument);
      }
      duk_pop(ctx);

      duk_get_prop_literal(ctx, -1, "certificate");
      if (duk_is_object(ctx, -1)) {
        cert_required = true;

        duk_get_prop_literal(ctx, -1, "issuer");
        if (duk_is_string(ctx, -1)) {
          cert_issuer = std::string(duk_to_string(ctx, -1));
        } else if (!duk_is_undefined(ctx, -1)) {
          ec = make_error_code(std::errc::invalid_argument);
        }
        duk_pop(ctx);

        duk_get_prop_literal(ctx, -1, "subject");
        if (duk_is_string(ctx, -1)) {
          cert_subject = std::string(duk_to_string(ctx, -1));
        } else if (!duk_is_undefined(ctx, -1)) {
          ec = make_error_code(std::errc::invalid_argument);
        }
        duk_pop(ctx);
      }
      duk_pop(ctx);
    } else if (!duk_is_undefined(ctx, -1)) {
      ec = make_error_code(std::errc::invalid_argument);
    }
    duk_pop(ctx);
  } else if (!duk_is_undefined(ctx, -1)) {
    ec = make_error_code(std::errc::invalid_argument);
  }
  duk_pop(ctx);

  if (ec) {
    return stdx::make_unexpected(ErrorResponse{2013, "hmm", "HY000"});
  }

  return handshake_data{error,         username,     password,
                        cert_required, cert_subject, cert_issuer};
}

// @pre on the stack is an object
void DuktapeStatementReader::handle_statement(const std::string &statement,
                                              ProtocolBase *protocol) {
  auto *ctx = pimpl_->ctx;
  bool is_enumable = false;

  // setup the stack for the next rounds
  if (pimpl_->first_stmt_) {
    duk_get_prop_string(ctx, -1, "stmts");
    // type is already checked in the constructor

    if (duk_is_array(ctx, -1)) {
      duk_enum(ctx, -1, DUK_ENUM_ARRAY_INDICES_ONLY);
    }

    pimpl_->first_stmt_ = false;
  }

  if (duk_is_thread(ctx, -1)) {
    if (DUK_EXEC_SUCCESS !=
        duk_pcompile_string(ctx, DUK_COMPILE_FUNCTION,
                            "function (t, stmt) { return "
                            "Duktape.Thread.resume(t, stmt); }")) {
      throw DuktapeRuntimeError(ctx, -1);
    }
    duk_dup(ctx, -2);  // the thread
    duk_push_lstring(ctx, statement.c_str(), statement.size());

    if (DUK_EXEC_SUCCESS != duk_pcall(ctx, 2)) {
      throw DuktapeRuntimeError(ctx, -1);
    }
    // @-1 result of resume
  } else if (duk_is_callable(ctx, -1)) {
    duk_dup(ctx,
            -1);  // copy the function to keep it on the stack for the
                  // next run
    duk_push_lstring(ctx, statement.c_str(), statement.size());

    if (DUK_EXEC_SUCCESS != duk_pcall(ctx, 1)) {
      throw DuktapeRuntimeError(ctx, -1);
    }
  } else {
    if (!duk_is_object(ctx, -1)) {  // enumarator is an object
      throw std::runtime_error(
          "expected 'stmts' enumerator to be an object, got " +
          duk_get_type_names(ctx, -1));
    }

    // @-1 is an enumarator
    if (0 == duk_next(ctx, -1, true)) {
      duk_pop(ctx);

      // startement received, but no matching statement in the iterator.
      protocol->encode_error(
          {1064, "Unknown statement. (end of stmts)", "HY000"});
      return;
    }
    // @-3 is an enumarator
    // @-2 is key
    // @-1 is value
    is_enumable = true;
  }

  // value must be an object
  if (!duk_is_object(ctx, -1)) {
    throw std::runtime_error("expected 'stmts' to return an 'object', got " +
                             duk_get_type_names(ctx, -1));
  }

  std::chrono::microseconds exec_time{};
  duk_get_prop_string(ctx, -1, "exec_time");
  if (!duk_is_undefined(ctx, -1)) {
    if (!duk_is_number(ctx, -1)) {
      throw std::runtime_error("exec_time must be a number, if set, got " +
                               duk_get_type_names(ctx, -1));
    }
    if (duk_get_number(ctx, -1) < 0) {
      throw std::out_of_range("exec_time must be a non-negative number");
    }

    // exec_time is written in the tracefile as microseconds
    exec_time = std::chrono::microseconds(
        static_cast<long>(duk_get_number(ctx, -1) * 1000));
  }
  duk_pop(ctx);

  bool response_sent{false};
  duk_get_prop_string(ctx, -1, "result");
  if (!duk_is_undefined(ctx, -1)) {
    protocol->exec_timer().expires_after(exec_time);
    protocol->encode_resultset(pimpl_->get_result(-1));
    response_sent = true;
  } else {
    duk_pop(ctx);  // result
    duk_get_prop_string(ctx, -1, "error");
    if (!duk_is_undefined(ctx, -1)) {
      protocol->encode_error(pimpl_->get_error(-1));
      response_sent = true;
    } else {
      duk_pop(ctx);  // error
      duk_get_prop_string(ctx, -1, "ok");
      if (!duk_is_undefined(ctx, -1)) {
        protocol->encode_ok(pimpl_->get_ok(-1));
        response_sent = true;
      } else {
        throw std::runtime_error("expected 'error', 'ok' or 'result'");
      }
    }
  }
  duk_pop(ctx);  // last prop

  duk_pop(ctx);  // value
  if (is_enumable) {
    duk_pop(ctx);  // key
  }

  if (!response_sent) {
    protocol->encode_error({1064, "Unsupported command", "HY000"});
  }
}

std::chrono::microseconds DuktapeStatementReader::get_default_exec_time() {
  return std::chrono::microseconds{0};
}

std::vector<AsyncNotice> DuktapeStatementReader::get_async_notices() {
  std::vector<AsyncNotice> result;

  if (!has_notices_) return result;

  auto *ctx = pimpl_->ctx;
  duk_get_prop_string(ctx, -1, "notices");

  if (!duk_is_array(ctx, -1)) {
    duk_pop(ctx);
    throw std::runtime_error("notices has to be an array!");
  }
  duk_enum(ctx, -1, DUK_ENUM_ARRAY_INDICES_ONLY);

  if (!duk_is_object(ctx, -1)) {  // enumarator is an object
    duk_pop_n(ctx, 2);
    throw std::runtime_error(
        "expected 'notices' enumerator to be an object, got " +
        duk_get_type_names(ctx, -1));
  }

  while (0 != duk_next(ctx, -1, true)) {
    if (!duk_is_object(ctx, -1)) {
      duk_pop_n(ctx, 4);
      throw std::runtime_error("expected 'notice' to return an 'object', got " +
                               duk_get_type_names(ctx, -1));
    }

    AsyncNotice notice;

    duk_get_prop_string(ctx, -1, "send_offset");
    if (!duk_is_undefined(ctx, -1)) {
      if (!duk_is_number(ctx, -1)) {
        throw std::runtime_error("send_offset must be a number, if set, got " +
                                 duk_get_type_names(ctx, -1));
      }

      if (duk_get_number(ctx, -1) < 0) {
        duk_pop(ctx);
        throw std::out_of_range("send_offset must be a non-negative number");
      }
      // send_offset is written in the tracefile as milliseconds
      notice.send_offset_ms =
          std::chrono::milliseconds(static_cast<long>(duk_get_number(ctx, -1)));
    }
    duk_pop(ctx);

    duk_get_prop_string(ctx, -1, "type");
    if (!duk_is_undefined(ctx, -1)) {
      if (!duk_is_number(ctx, -1)) {
        throw std::runtime_error("type must be a number, if set, got " +
                                 duk_get_type_names(ctx, -1));
      }

      if (duk_get_number(ctx, -1) < 0) {
        duk_pop(ctx);
        throw std::out_of_range("id must be a non-negative number");
      }
      notice.type = static_cast<unsigned>(duk_get_number(ctx, -1));
    }
    duk_pop(ctx);

    duk_get_prop_string(ctx, -1, "scope");
    if (!duk_is_undefined(ctx, -1)) {
      if (!duk_is_string(ctx, -1)) {
        throw std::runtime_error("scope must be a string, if set, got " +
                                 duk_get_type_names(ctx, -1));
      }

      const std::string scope = duk_get_string(ctx, -1);
      if (scope == "LOCAL" || scope == "") {
        notice.is_local = true;
      } else if (scope == "GLOBAL") {
        notice.is_local = false;
      } else {
        throw std::runtime_error("scope must be LOCAL or GLOBAL was: '" +
                                 scope + "'");
      }
    }
    duk_pop(ctx);

    duk_get_prop_string(ctx, -1, "payload");
    if (!duk_is_undefined(ctx, -1)) {
      if (!duk_is_object(ctx, -1)) {
        throw std::runtime_error("payload must be an object, if set, got " +
                                 duk_get_type_names(ctx, -1));
      }

      notice.payload = duk_json_encode(ctx, -1);
    }
    duk_pop_n(ctx, 3);
    result.push_back(notice);
  }

  duk_pop_n(ctx, 2);

  return result;
}

void DuktapeStatementReader::set_session_ssl_info(const SSL *ssl) {
  auto *ctx = pimpl_->ctx;

  duk_push_global_object(ctx);
  duk_get_prop_string(ctx, -1, "mysqld");
  duk_get_prop_string(ctx, -1, "session");

  duk_push_string(ctx, SSL_get_cipher_name(ssl));
  duk_put_prop_string(ctx, -2, "ssl_cipher");

  duk_push_string(ctx, SSL_get_cipher_name(ssl));
  duk_put_prop_string(ctx, -2, "mysqlx_ssl_cipher");

  duk_pop(ctx);
  duk_pop(ctx);
  duk_pop(ctx);
}

}  // namespace server_mock
