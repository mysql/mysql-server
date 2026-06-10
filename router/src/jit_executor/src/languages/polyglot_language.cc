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

#include "languages/polyglot_language.h"

#include <algorithm>
#include <cassert>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

#include "languages/polyglot_common_context.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/scoped_callback.h"
#include "native_wrappers/polyglot_file_system_wrapper.h"
#include "utils/polyglot_error.h"
#include "utils/polyglot_scope.h"
#include "utils/utils_path.h"
#include "utils/utils_string.h"

#include "native_wrappers/polyglot_collectable.h"

namespace shcore {
namespace polyglot {

IMPORT_LOG_FUNCTIONS()

Polyglot_language::Current_script::Current_script()
    : m_root(/*path::getcwd()*/ "") {
  // fake top-level script
  push("mysqlrouter");
}

std::string Polyglot_language::Current_script::current_folder() const {
  return path::dirname(current_script());
}

void Polyglot_language::Current_script::push(const std::string &script) {
  m_scripts.push(path::is_absolute(script)
                     ? script
                     : path::normalize(path::join_path(m_root, script)));
}

Scoped_global::Scoped_global(const Polyglot_language *language,
                             poly_value value)
    : m_language(language) {
  assert(m_language);
  m_name = shcore::str_format(
      "___%s___",
      shcore::get_random_string(10, "abcdefABCDEF0123456789").c_str());

  if (value != nullptr) {
    m_language->globals()->set_poly_member(m_name, value);
  }
}

poly_value Scoped_global::execute(const std::string &code) {
  poly_value result;

  auto actual_code = shcore::str_replace(code, "<<global>>", m_name);

  if (const auto rc = m_language->eval("(internal)", actual_code, &result);
      rc != poly_ok) {
    throw Polyglot_error(m_language->thread(), rc);
  }

  return result;
}

Scoped_global::~Scoped_global() {
  try {
    m_language->globals()->remove_member(m_name);
  } catch (const std::exception &error) {
    log_error("polyglot error while cleaning temporary global '%s': %s",
              m_name.c_str(), error.what());
  }
}

Polyglot_language::Polyglot_language(Polyglot_common_context *common_context,
                                     const std::string &debug_port)
    : m_common_context{common_context}, m_debug_port{debug_port} {}

void Polyglot_language::enable_debug() {
  throw_if_error(poly_context_builder_option, thread(), m_context_builder,
                 "inspect", m_debug_port.c_str());

  throw_if_error(poly_context_builder_option, thread(), m_context_builder,
                 "inspect.Suspend", "false");

  throw_if_error(poly_context_builder_option, thread(), m_context_builder,
                 "inspect.WaitAttached", "false");
}

void Polyglot_language::init_context_builder() {
  m_context_builder = NULL;
  throw_if_error(poly_create_context_builder, thread(), nullptr, 0,
                 &m_context_builder);

  auto engine = !m_debug_port.empty() ? nullptr : m_common_context->engine();

  if (engine) {
    throw_if_error(poly_context_builder_engine, m_thread, m_context_builder,
                   engine);
  }

  throw_if_error(poly_context_builder_allow_all_access, thread(),
                 m_context_builder, false);

  throw_if_error(poly_context_builder_allow_buffer_access_constrained_policy,
                 thread(), m_context_builder, true);

  throw_if_error(poly_context_builder_output, thread(), m_context_builder,
                 &Polyglot_language::output_callback,
                 &Polyglot_language::error_callback, this);
}

void Polyglot_language::initialize(const std::shared_ptr<IFile_system> &fs) {
  m_file_system = fs;

  if (const auto rc =
          poly_attach_thread(m_common_context->isolate(), &m_thread);
      rc != poly_ok) {
    throw Polyglot_generic_error("error attaching thread to isolate");
  }

  m_scope = std::make_unique<Polyglot_scope>(thread());

  m_storage = std::make_unique<Polyglot_storage>(thread());

  init_context_builder();

  {
    // If there's no file system and debug should be enabled, we should do it
    // here, otherwise we need to wait until the file system is set
    if (!m_debug_port.empty() && !m_file_system) {
      enable_debug();
    }
    poly_context context;
    throw_if_error(poly_context_builder_build, thread(), m_context_builder,
                   &context);

    m_context = Store(thread(), context);
  }

  m_types = std::make_unique<Polyglot_type_bridger>(shared_from_this());
  m_types->init();

  // NOTE: The file system is a weird citizen, because it is a ProxyObject,
  // which means it requires to be created using a specific context, however,
  // for a context to use a custom file system, the file system needs to be
  // specified on the context builder (yes, before the context is created).
  // This problem is handled as follows:
  // The context builder is initialized and a context is created (lines above
  // this comment). If a custom file system is specified then the
  // set_file_system function will update the context builder to specify the
  // proxy file system and a new context will be created, to finally update
  // the context in the file system to the one that will be used in the
  // language.
  if (m_file_system) {
    set_file_system();
  }

  {
    Polyglot_scope context_creation_scope(thread());

    poly_value globals;
    if (const auto rc = poly_context_get_bindings(thread(), context(),
                                                  get_language_id(), &globals);
        rc != poly_ok) {
      throw Polyglot_generic_error("error getting context bindings");
    }
    m_globals = std::make_shared<Polyglot_object>(m_types.get(), thread(),
                                                  context(), globals, "");
  }
}

void Polyglot_language::finalize() {
  m_globals.reset();
  m_types->dispose();

  mysql_harness::ScopedCallback cleanup{[this]() {
    m_context.reset();
    m_storage->clear();
    m_scope->close();
  }};

  if (const auto rc = poly_context_close(thread(), context(), true);
      rc != poly_ok) {
    throw Polyglot_error(thread(), rc);
  }

  m_common_context->clean_collectables();
}

void Polyglot_language::throw_jit_executor_exception(
    const Jit_executor_exception &exception) {
  throw_if_error(poly_throw_custom_exception, thread(), exception.id(),
                 exception.what());
}

shcore::Value Polyglot_language::to_native_object(
    poly_value object, const std::string &class_name) {
  return Value(std::make_shared<polyglot::Polyglot_object>(
      m_types.get(), thread(), context(), object, class_name));
}

int64_t Polyglot_language::eval(const std::string &source,
                                const std::string &code_str,
                                poly_value *result) const {
  auto ret_val =
      poly_context_eval(thread(), context(), get_language_id(),
                        source.empty() ? k_origin_shell : source.c_str(),
                        code_str.c_str(), result);

  return ret_val;
}

poly_value Polyglot_language::create_source(const std::string &path) const {
  poly_value source;
  shcore::polyglot::throw_if_error(poly_create_source, thread(),
                                   get_language_id(), path.c_str(), &source);

  return source;
}

std::pair<Value, bool> Polyglot_language::debug(const std::string &path) {
  poly_value source = create_source(path);

  poly_value result;
  if (const auto rc =
          poly_context_eval_source(thread(), context(), source, &result);
      rc != poly_ok) {
    throw Polyglot_error(thread(), rc);
  }

  return {convert(result), false};
}

std::pair<Value, bool> Polyglot_language::execute(const std::string &code_str,
                                                  const std::string &source) {
  const auto &origin = source.empty() ? k_origin_shell : source;
  const auto script_scope = enter_script(origin);

  clear_is_terminating();

  poly_value result;
  if (const auto rc = eval(origin, code_str, &result); rc != poly_ok) {
    throw Polyglot_error(thread(), rc);
  }

  return {convert(result), false};
}

void Polyglot_language::output_callback(const char *bytes, size_t length,
                                        void *data) {
  auto self = static_cast<Polyglot_language *>(data);
  self->output_handler(bytes, length);
}

void Polyglot_language::error_callback(const char *bytes, size_t length,
                                       void *data) {
  auto self = static_cast<Polyglot_language *>(data);
  self->error_handler(bytes, length);
}

std::string Polyglot_language::current_script_folder() const {
  return m_current_script.current_folder();
}

void Polyglot_language::throw_exception_object(poly_value exception) const {
  if (const auto rc = poly_throw_exception_object(thread(), exception);
      rc != poly_ok) {
    log_error("While throwing exception, another exception occurred: %s",
              Polyglot_error(thread(), rc).what());
  }
}

void Polyglot_language::throw_exception_object(
    const shcore::Dictionary_t &data) const {
  try {
    auto exception = create_exception_object(data->get_string(k_key_message),
                                             convert(shcore::Value(data)));
    throw_exception_object(exception);
  } catch (const std::exception &error) {
    log_error("While throwing exception, another exception occurred: %s",
              error.what());
  }
}

void Polyglot_language::throw_exception_object(
    const Polyglot_error &error) const {
  throw_exception_object(error.data());
}

void Polyglot_language::set_global(const std::string &name,
                                   const Value &value) const {
  set_global(name, convert(value));
}

void Polyglot_language::set_global(const std::string &name,
                                   poly_value value) const {
  m_globals->set_poly_member(name, value);
}

void Polyglot_language::set_global_function(const std::string &name,
                                            poly_callback callback,
                                            void *data) {
  auto function = wrap_callback(callback, data ? data : this);
  set_global(name, function);
}

Store Polyglot_language::get_global(const std::string &name) const {
  return Store(thread(), m_globals->get_poly_member(name));
}

poly_value Polyglot_language::poly_string(const std::string &data) const {
  return polyglot::poly_string(thread(), context(), data);
}

std::string Polyglot_language::to_string(poly_value obj) const {
  return polyglot::to_string(thread(), obj);
}

void Polyglot_language::terminate() {
  m_terminating = true;

  poly_thread i_thread;
  if (poly_ok != poly_attach_thread(m_common_context->isolate(), &i_thread)) {
    throw Polyglot_generic_error("Error attaching thread on terminate handler");
  }

  throw_if_error(poly_context_interrupt, i_thread, context());

  if (poly_ok != poly_detach_thread(i_thread)) {
    throw Polyglot_generic_error("Error detaching thread on terminate handler");
  }
}

bool Polyglot_language::is_terminating() const { return m_terminating; }

void Polyglot_language::clear_is_terminating() { m_terminating = false; }

poly_value Polyglot_language::wrap_callback(poly_callback callback,
                                            void *data) const {
  poly_value function;

  throw_if_error(poly_create_function, thread(), context(), callback, data,
                 &function);

  return function;
}

/**
 * Context needs to be either deleted immediately (i.e. in case of an error),
 * or stored till JS context exists (in order to preserve execution
 * environment).
 */
poly_context Polyglot_language::copy_global_context() const {
  poly_context new_context;
  throw_if_error(poly_context_builder_build, thread(), m_context_builder,
                 &new_context);

  poly_value new_globals;
  if (const auto rc = poly_context_get_bindings(
          thread(), new_context, get_language_id(), &new_globals);
      rc != poly_ok) {
    throw Polyglot_generic_error("error getting context bindings");
  }

  auto members = m_globals->get_members();
  for (const auto &member : members) {
    poly_value poly_member;
    throw_if_error(poly_value_get_member, thread(), m_globals->get(),
                   member.c_str(), &poly_member);

    throw_if_error(poly_value_put_member, thread(), new_globals, member.c_str(),
                   poly_member);
  }

  return new_context;
}

poly_context Polyglot_language::context() const { return m_context.get(); }

poly_thread Polyglot_language::thread() const { return m_thread; }

const std::shared_ptr<Polyglot_object> &Polyglot_language::globals() const {
  return m_globals;
}

Value Polyglot_language::convert(poly_value value) const {
  return m_types->poly_value_to_native_value(value);
}

poly_value Polyglot_language::convert(const Value &value) const {
  return m_types->native_value_to_poly_value(value);
}

Argument_list Polyglot_language::convert_args(
    const std::vector<poly_value> &args) const {
  return m_types->convert_args(args);
}

poly_value Polyglot_language::type_info(poly_value value) const {
  return m_types->type_info(value);
}

poly_reference Polyglot_language::store(poly_value value) {
  return m_storage->store(value);
}

void Polyglot_language::erase(poly_reference value) { m_storage->erase(value); }

bool Polyglot_language::get_member(poly_value object, const char *name,
                                   poly_value *member) const {
  bool has_member{false};
  throw_if_error(poly_value_has_member, thread(), object, name, &has_member);

  if (has_member) {
    throw_if_error(poly_value_get_member, thread(), object, name, member);
  }

  return has_member;
}

Polyglot_language::Script_scope Polyglot_language::enter_script(
    const std::string &s) {
  return Script_scope(this, s);
}

void Polyglot_language::set_file_system() {
  Polyglot_file_system_wrapper fs_wrapper(shared_from_this());

  auto poly_fs = fs_wrapper.wrap(m_file_system);

  throw_if_error(poly_context_builder_set_file_system, thread(),
                 m_context_builder, poly_fs);

  if (!m_debug_port.empty()) {
    enable_debug();
  }

  poly_context context;
  throw_if_error(poly_context_builder_build, thread(), m_context_builder,
                 &context);

  m_context = Store(thread(), context);

  throw_if_error(poly_file_system_set_context, thread(), poly_fs,
                 m_context.get());
}

}  // namespace polyglot
}  // namespace shcore
