/*
  Copyright (c) 2018, 2020, Oracle and/or its affiliates. All rights reserved.

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
#include <functional>
#include <map>
#include <stdexcept>
#include <string>

#include <mysqld_error.h>
#include <system_error>

#include "authentication.h"
#include "duk_logging.h"
#include "duk_module_shim.h"
#include "duk_node_fs.h"
#include "duktape.h"
#include "duktape_statement_reader.h"
#include "harness_assert.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/stdx/expected.h"
#include "mysqlrouter/classic_protocol.h"

IMPORT_LOG_FUNCTIONS()

namespace server_mock {
/*
 * get the names of the type.
 *
 * returns a comma-seperated string
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

struct DuktapeStatementReader::Pimpl {
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

    return {get_object_integer_value<uint16_t>(-1, "last_insert_id", 0),
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

      column_info_type column_info{
          get_object_string_value(-1, "name", "", true),
          column_type_from_string(
              get_object_string_value(-1, "type", "", true)),
          get_object_string_value(-1, "orig_name"),
          get_object_string_value(-1, "table"),
          get_object_string_value(-1, "orig_table"),
          get_object_string_value(-1, "schema"),
          get_object_string_value(-1, "catalog", "def"),
          get_object_integer_value<uint16_t>(-1, "flags"),
          get_object_integer_value<uint8_t>(-1, "decimals"),
          get_object_integer_value<uint32_t>(-1, "length"),
          get_object_integer_value<uint16_t>(-1, "character_set", 63),
          1  // repeat
      };

      if (duk_get_prop_string(ctx, -1, "repeat")) {
        throw std::runtime_error("repeat is not supported");
      }
      duk_pop(ctx);

      response.columns.push_back(column_info);

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
            row_values.emplace_back(stdx::make_unexpected());
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
  duk_context *ctx{nullptr};

  bool authenticate(const std::string &auth_username,
                    const std::vector<uint8_t> &auth_response) {
    // std::optional would be neat
    std::string username;
    bool username_set{false};
    std::string password;
    bool password_set{false};

    duk_get_prop_string(ctx, -1, "handshake");
    if (duk_is_object(ctx, -1)) {
      duk_get_prop_string(ctx, -1, "auth");
      if (duk_is_object(ctx, -1)) {
        duk_get_prop_literal(ctx, -1, "username");
        if (duk_is_string(ctx, -1)) {
          username = duk_to_string(ctx, -1);
          username_set = true;
        }
        duk_pop(ctx);

        duk_get_prop_literal(ctx, -1, "password");
        if (duk_is_string(ctx, -1)) {
          password = duk_to_string(ctx, -1);
          password_set = true;
        }
        duk_pop(ctx);
      }
      duk_pop(ctx);
    }
    duk_pop(ctx);

    if (username_set && username != auth_username) {
      return false;
    }

    if (password_set) {
      if (auth_method_ == CachingSha2Password::name) {
        auto scramble_res = CachingSha2Password::scramble(nonce_, password);
        return scramble_res && (scramble_res.value() == auth_response);
      } else if (auth_method_ == MySQLNativePassword::name) {
        auto scramble_res = MySQLNativePassword::scramble(nonce_, password);
        return scramble_res && (scramble_res.value() == auth_response);
      } else if (auth_method_ == ClearTextPassword::name) {
        auto scramble_res = ClearTextPassword::scramble(nonce_, password);
        return scramble_res && (scramble_res.value() == auth_response);
      } else {
        // there is also
        // - old_password (3.23, 4.0)
        // - sha256_password (5.6, ...)
        // - windows_authentication (5.6, ...)
        return false;
      }
    }

    return true;
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
  std::string auth_method_;
  std::string username_;
};

duk_int_t duk_peval_file(duk_context *ctx, const char *path) {
  duk_push_c_function(ctx, duk_node_fs_read_file_sync, 1);
  duk_push_string(ctx, path);
  if (duk_int_t rc = duk_pcall(ctx, 1)) {
    return rc;
  }

  duk_buffer_to_string(ctx, -1);
  duk_push_string(ctx, path);
  if (duk_int_t rc = duk_pcompile(ctx, DUK_COMPILE_EVAL)) {
    return rc;
  }
  duk_push_global_object(ctx);
  return duk_pcall_method(ctx, 0);
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

/**
 * dismissable scope guard.
 *
 * used with RAII to call cleanup function if not dismissed
 *
 * allows to release resources in case exceptions are thrown
 */
class ScopeGuard {
 public:
  template <class Callable>
  ScopeGuard(Callable &&undo_func)
      : undo_func_{std::forward<Callable>(undo_func)} {}

  void dismiss() { undo_func_ = nullptr; }
  ~ScopeGuard() {
    if (undo_func_) undo_func_();
  }

 private:
  std::function<void()> undo_func_;
};

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
    const std::string &filename, const std::string &module_prefix,
    std::map<std::string, std::string> session_data,
    std::shared_ptr<MockServerGlobalScope> shared_globals)
    : pimpl_{new Pimpl()}, shared_{std::move(shared_globals)} {
  auto *ctx = duk_create_heap_default();

  // free the duk_context if an exception gets thrown as
  // DuktapeStatementReaders's destructor will not be called in that case.
  ScopeGuard duk_guard{[&ctx]() { duk_destroy_heap(ctx); }};

  // init module-loader
  duk_module_shim_init(ctx, module_prefix.c_str());

  duk_push_global_stash(ctx);
  if (nullptr == shared_.get()) {
    // why is the shared-ptr empty?
    throw std::logic_error(
        "expected shared global variable object to be set, but it isn't.");
  }

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
  for (auto &el : session_data) {
    duk_push_lstring(ctx, el.second.data(), el.second.size());
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

  if (DUK_EXEC_SUCCESS != duk_peval_file(ctx, filename.c_str())) {
    throw DuktapeRuntimeError(ctx, -1);
  }

  if (!duk_is_object(ctx, -1)) {
    throw std::runtime_error(
        filename + ": expected statement handler to return an object, got " +
        duk_get_type_names(ctx, -1));
  }

  // check if the sections have the right types
  check_stmts_section(ctx);
  has_notices_ = check_notices_section(ctx);
  check_handshake_section(ctx);

  // we are still alive, dismiss the guard
  pimpl_->ctx = ctx;
  duk_guard.dismiss();
}

DuktapeStatementReader::~DuktapeStatementReader() {
  // duk_pop(pimpl_->ctx);

  if (pimpl_->ctx) duk_destroy_heap(pimpl_->ctx);
}

/*
 * @pre on the stack is an object
 */
HandshakeResponse DuktapeStatementReader::handle_handshake_init(
    const std::vector<uint8_t> &, HandshakeState &next_state) {
  HandshakeResponse response;

  response.exec_time = get_default_exec_time();

  auto *ctx = pimpl_->ctx;

  // defaults
  std::string server_version = "8.0.5-mock";
  uint32_t connection_id = 0;
  classic_protocol::capabilities::value_type server_capabilities =
      classic_protocol::capabilities::protocol_41 |
      classic_protocol::capabilities::plugin_auth |
      classic_protocol::capabilities::secure_connection;
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
        response.exec_time = std::chrono::microseconds(
            static_cast<long>(duk_get_number(ctx, -1) * 1000));
      }
      duk_pop(ctx);

      server_version =
          pimpl_->get_object_string_value(-1, "server_version", server_version);
      connection_id = pimpl_->get_object_integer_value<uint32_t>(
          -1, "connection_id", connection_id);
      status_flags = pimpl_->get_object_integer_value<uint16_t>(
          -1, "status_flags", status_flags);
      character_set = pimpl_->get_object_integer_value<uint8_t>(
          -1, "character_set", character_set);
      auth_method =
          pimpl_->get_object_string_value(-1, "auth_method", auth_method);
      nonce = pimpl_->get_object_string_value(-1, "nonce", nonce);
    }
    duk_pop(ctx);
  }
  duk_pop(ctx);

  response.response_type = HandshakeResponse::ResponseType::GREETING;
  response.response = std::make_unique<Greeting>(
      server_version, connection_id, server_capabilities, status_flags,
      character_set, auth_method, nonce);

  pimpl_->server_capabilities_ = server_capabilities;
  pimpl_->auth_method_ = auth_method;
  pimpl_->nonce_ = nonce;
  next_state = HandshakeState::GREETED;

  return response;
}

HandshakeResponse DuktapeStatementReader::handle_handshake_greeted(
    const std::vector<uint8_t> &payload, HandshakeState &next_state) {
  HandshakeResponse response;

  response.exec_time = get_default_exec_time();

  // decode the payload

  auto decode_res =
      classic_protocol::decode<classic_protocol::message::client::Greeting>(
          net::buffer(payload), pimpl_->server_capabilities_);

  if (!decode_res) {
    throw std::system_error(decode_res.error(),
                            "decoding client greeting failed");
  }

  auto greeting = decode_res.value().second;

  pimpl_->username_ = greeting.username();
  if (greeting.capabilities().test(
          classic_protocol::capabilities::pos::plugin_auth)) {
    pimpl_->auth_method_ = greeting.auth_method_name();
  } else {
    // 4.1 or so
    pimpl_->auth_method_ = MySQLNativePassword::name;
  }

  if (pimpl_->auth_method_ == CachingSha2Password::name) {
    // auth_response() should be empty
    //
    // ask for the real full authentication
    pimpl_->nonce_ = std::string(20, 'a');

    response.response_type = HandshakeResponse::ResponseType::AUTH_SWITCH;
    response.response =
        std::make_unique<AuthSwitch>(pimpl_->auth_method_, pimpl_->nonce_);

    next_state = HandshakeState::AUTH_SWITCHED;
  } else if (pimpl_->auth_method_ == MySQLNativePassword::name ||
             pimpl_->auth_method_ == ClearTextPassword::name) {
    auto auth_method_data = greeting.auth_method_data();

    // authenticate wants a vector<uint8_t>
    std::vector<uint8_t> auth_method_data_vec(auth_method_data.begin(),
                                              auth_method_data.end());
    if (pimpl_->authenticate(pimpl_->username_, auth_method_data_vec)) {
      response.response_type = HandshakeResponse::ResponseType::OK;
      response.response = std::make_unique<OkResponse>();

      next_state = HandshakeState::DONE;
    } else {
      response.response_type = HandshakeResponse::ResponseType::ERROR;
      response.response = std::make_unique<ErrorResponse>(
          ER_ACCESS_DENIED_ERROR,  // 1045
          "Access Denied for user '" + pimpl_->username_ + "'@'localhost'",
          "28000");
      next_state = HandshakeState::DONE;
    }
  } else {
    response.response_type = HandshakeResponse::ResponseType::ERROR;
    response.response =
        std::make_unique<ErrorResponse>(0, "unknown auth-method");

    next_state = HandshakeState::DONE;
  }

  harness_assert(response.response_type !=
                 HandshakeResponse::ResponseType::UNKNOWN);

  return response;
}
HandshakeResponse DuktapeStatementReader::handle_handshake_auth_switched(
    const std::vector<uint8_t> &payload, HandshakeState &next_state) {
  HandshakeResponse response;

  response.exec_time = get_default_exec_time();

  // empty password is signaled by {0},
  // -> authenticate expects {}
  // -> client expects OK, instead of AUTH_FAST in this case
  if (payload == std::vector<uint8_t>{0} &&
      pimpl_->authenticate(pimpl_->username_, {})) {
    response.response_type = HandshakeResponse::ResponseType::OK;
    response.response = std::make_unique<OkResponse>();

    next_state = HandshakeState::DONE;
  } else if (pimpl_->authenticate(pimpl_->username_, payload)) {
    if (pimpl_->auth_method_ == CachingSha2Password::name) {
      // caching-sha2-password is special and needs the auth-fast state
      response.response_type = HandshakeResponse::ResponseType::AUTH_FAST;
      response.response = std::make_unique<AuthFast>();
    } else {
      response.response_type = HandshakeResponse::ResponseType::OK;
      response.response = std::make_unique<OkResponse>();
    }

    next_state = HandshakeState::DONE;
  } else {
    response.response_type = HandshakeResponse::ResponseType::ERROR;
    response.response = std::make_unique<ErrorResponse>(
        ER_ACCESS_DENIED_ERROR,
        "Access Denied for user '" + pimpl_->username_ + "'@'localhost'",
        "28000");
    next_state = HandshakeState::DONE;
  }

  return response;
}

HandshakeResponse DuktapeStatementReader::handle_handshake(
    const std::vector<uint8_t> &payload) {
  switch (handshake_state_) {
    case HandshakeState::INIT:
      return handle_handshake_init(payload, handshake_state_);
    case HandshakeState::GREETED:
      return handle_handshake_greeted(payload, handshake_state_);
    case HandshakeState::AUTH_SWITCHED:
      return handle_handshake_auth_switched(payload, handshake_state_);
    default: {
      HandshakeResponse response;

      response.response_type = HandshakeResponse::ResponseType::ERROR;
      response.response =
          std::make_unique<ErrorResponse>(0, "wrong handshake state");

      handshake_state_ = HandshakeState::DONE;
      return response;
    }
  }
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
      protocol->send_error(1064, "Unknown statement. (end of stmts)");
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
    protocol->send_resultset(pimpl_->get_result(-1), exec_time);
    response_sent = true;
  } else {
    duk_pop(ctx);  // result
    duk_get_prop_string(ctx, -1, "error");
    if (!duk_is_undefined(ctx, -1)) {
      protocol->send_error(pimpl_->get_error(-1));
      response_sent = true;
    } else {
      duk_pop(ctx);  // error
      duk_get_prop_string(ctx, -1, "ok");
      if (!duk_is_undefined(ctx, -1)) {
        protocol->send_ok(pimpl_->get_ok(-1));
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
    protocol->send_error(1064, "Unsupported command");
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

}  // namespace server_mock
