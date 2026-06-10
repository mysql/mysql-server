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

#ifndef MYSQLSHDK_SCRIPTING_POLYGLOT_LANGUAGES_POLYGLOT_LANGUAGE_H_
#define MYSQLSHDK_SCRIPTING_POLYGLOT_LANGUAGES_POLYGLOT_LANGUAGE_H_

#include <cassert>
#include <list>
#include <memory>
#include <stack>
#include <string>
#include <vector>

#include "utils/polyglot_api_clean.h"

#include "jit_executor_type_conversion.h"
#include "languages/polyglot_common_context.h"
#include "mysqlrouter/jit_executor_exceptions.h"
#include "mysqlrouter/polyglot_file_system.h"
#include "native_wrappers/polyglot_object_bridge.h"
#include "polyglot_wrappers/types_polyglot.h"
#include "utils/polyglot_scope.h"
#include "utils/polyglot_store.h"

namespace shcore {
namespace polyglot {

class ICollectable;
class Polyglot_language;
/**
 * Allows creating a temporary global variable with a random name which will
 * only live until the execution context of the instance of this class ends.
 *
 * The reason this is needed is because in graal, non primitive values
 * originating from guest languages (like JS) are not sharable, this means:
 *
 * - A guest language specific object is not mappable to C++ equivalent, i.e.
 * (i.e. undefined, ArrayBuffer)
 * - There's no way we can generate a guest language specific object from C++
 * (i.e. undefined, ArrayBuffer or Exception Objects)
 *
 * The way this problem is addressed, is by creating a temporary object in the
 * global context, to create the object in the guest language and then if
 * needed, storing it to create a reference so the object can be deleted from
 * the main context (to avoid pollution) without actually losing the object,
 * then the reference is provided to the guest language.
 *
 * Additional Information:
 * https://github.com/oracle/graaljs/issues/70#issuecomment-436257058
 * https://github.com/oracle/graal/blob/master/sdk/CHANGELOG.md#version-10-rc9
 */
class Scoped_global final {
 public:
  Scoped_global() = delete;

  // Generates a random name for the scoped value, if value is provided, it will
  // be set as the global value, otherwise just a random name is created to be
  // used in the execute function.
  Scoped_global(const Polyglot_language *language, poly_value value = nullptr);

  Scoped_global(const Scoped_global &) = delete;
  Scoped_global &operator=(const Scoped_global &) = delete;

  Scoped_global(Scoped_global &&) = delete;
  Scoped_global &operator=(Scoped_global &&) = delete;

  // Executes some code referencing the scoped value, the string <<global>>
  // should be used in the text to refer to the created global (internally
  // <<global>> will be replaced with the generated value), returns the result
  // of the code execution
  poly_value execute(const std::string &code);

  ~Scoped_global();

 private:
  const Polyglot_language *m_language;
  std::string m_name;
};

/**
 * The polyglot library may support several languages (guest languages), and
 * provides out of the box C++ interfaces to handle common language elements,
 * such as Objects, Maps, Arrays, Functions and primitive data types.
 *
 * However, it does not provide out of the box C++ interface for guest language
 * non primitive data types or specific objects.
 *
 * This class is to be used as the base class to handle the common language
 * elements supported in the polyglot library, any guest language supported by
 * the shell is meant to extend this class and add handling of the elements that
 * are specific to it.
 */
class Polyglot_language
    : public std::enable_shared_from_this<Polyglot_language> {
 public:
  class Script_scope final {
   public:
    Script_scope() = delete;

    Script_scope(Polyglot_language *owner, const std::string &s)
        : m_owner(owner) {
      assert(m_owner);
      m_owner->m_current_script.push(s);
    }

    Script_scope(const Script_scope &) = delete;
    Script_scope(Script_scope &&) = default;

    Script_scope &operator=(const Script_scope &) = delete;
    Script_scope &operator=(Script_scope &&) = default;

    ~Script_scope() { m_owner->m_current_script.pop(); }

   private:
    Polyglot_language *m_owner = nullptr;
  };

  class Current_script {
   public:
    Current_script();

    Current_script(const Current_script &) = delete;
    Current_script &operator=(const Current_script &) = delete;

    Current_script(Current_script &&) = delete;
    Current_script &operator=(Current_script &&) = delete;

    void push(const std::string &script);

    void pop() { m_scripts.pop(); }

    std::string current_script() const { return m_scripts.top(); }

    std::string current_folder() const;

   private:
    std::string m_root;
    std::stack<std::string> m_scripts;
  };

  explicit Polyglot_language(Polyglot_common_context *common_context,
                             const std::string &debug_port = "");

  Polyglot_language(const Polyglot_language &) = delete;
  Polyglot_language &operator=(const Polyglot_language &) = delete;

  Polyglot_language(Polyglot_language &&) = delete;
  Polyglot_language &operator=(Polyglot_language &&) = delete;

  virtual ~Polyglot_language() = default;

  virtual const char *get_language_id() const = 0;

  virtual void init_context_builder();

  virtual void initialize(const std::shared_ptr<IFile_system> &fs = {});

  virtual void finalize();

  /**
   * The polyglot_utils file has the following functions that can be used to
   * export C++ functions as polyglot global functions:
   *
   * - polyglot_handler*
   * - native_handler*
   *
   * These wrappers allow exposing class functions as long as the class
   * containing them contains a language() function that returns a pointer to a
   * Polyglot language.
   *
   * This function, basically enables subclasses to expose functions as polyglot
   * globals.
   */
  Polyglot_language *language() { return this; }

  /**
   * Throws a specific exception type in the GraalVM environment, with a
   * specific message
   */
  void throw_jit_executor_exception(const Jit_executor_exception &exception);

  // These functions represent operations than have specific handling depending
  // on the guest language, for that reason only the interface is defined here
  // and the subclasses provide specific language handling.

  /**
   * Return the a guest language interpretation of the Undefined C++ value.
   */
  virtual poly_value undefined() const = 0;

  /**
   * Returns true if the provided value is undefined in the guest language.
   */
  virtual bool is_undefined(poly_value value) const = 0;

  /**
   * Return the guest language interpretation of a binary string.
   */
  virtual poly_value array_buffer(const std::string &data) const = 0;

  /**
   * Return true if the guest language identifies the value as an object
   */
  virtual bool is_object(poly_value object,
                         std::string *class_name = nullptr) const = 0;

  /**
   * Converts a C++ object into a representation of the object in the guest
   * language
   */
  virtual poly_value from_native_object(
      const Object_bridge_t &object) const = 0;

  /**
   * Converts a guest language object into its C++ representation
   */
  virtual shcore::Value to_native_object(poly_value object,
                                         const std::string &class_name);

  /**
   * Translates a dictionary object into an exception in the guest language.
   */
  void throw_exception_object(const shcore::Dictionary_t &data) const;

  /**
   * Translates a Polyglot_error into an exception in the guest language.
   */
  void throw_exception_object(const Polyglot_error &data) const;

  /**
   * Retrieves the list of keywords in the guest language
   */
  virtual const std::vector<std::string> &keywords() const = 0;

  /**
   * Wraps a call to poly_context_eval
   */
  int64_t eval(const std::string &source, const std::string &code_str,
               poly_value *result) const;

  poly_value create_source(const std::string &path) const;

  /**
   * Debugs the given script
   */
  std::pair<Value, bool> debug(const std::string &path);

  /**
   * Executes the given script
   */
  std::pair<Value, bool> execute(const std::string &code_str,
                                 const std::string &source);

  /**
   * This function should be implemented to provide plugin load support
   */
  virtual bool load_plugin([[maybe_unused]] const std::string &path) {
    throw std::runtime_error("Plugin support is not available.");
  }

  /**
   * Creates a copy of the global context
   */
  poly_context copy_global_context() const;

  poly_value poly_string(const std::string &data) const;
  std::string to_string(poly_value obj) const;

  poly_context context() const;
  poly_thread thread() const;
  const std::shared_ptr<Polyglot_object> &globals() const;

  poly_reference store(poly_value value);
  void erase(poly_reference value);

  void set_global(const std::string &name, const Value &value) const;
  void set_global(const std::string &name, poly_value value) const;
  void set_global_function(const std::string &name, poly_callback callback,
                           void *data = nullptr);
  Store get_global(const std::string &name) const;

  void terminate();
  bool is_terminating() const;

  Script_scope enter_script(const std::string &s);

  Value convert(poly_value value) const;
  poly_value convert(const Value &value) const;

  Argument_list convert_args(const std::vector<poly_value> &args) const;
  poly_value type_info(poly_value value) const;

  bool get_member(poly_value object, const char *name,
                  poly_value *member) const;

  std::string current_script_folder() const;

  Polyglot_common_context *common_context() { return m_common_context; }

 protected:
  poly_value wrap_callback(poly_callback callback, void *data) const;
  void clear_is_terminating();

  Polyglot_common_context *m_common_context;

  poly_thread m_thread = nullptr;
  poly_context_builder m_context_builder = nullptr;
  Store m_context;
  std::unique_ptr<Polyglot_type_bridger> m_types;
  std::shared_ptr<Polyglot_object> m_globals;
  Current_script m_current_script;
  std::unique_ptr<Polyglot_storage> m_storage;
  std::shared_ptr<IFile_system> m_file_system;

 protected:
  std::string m_debug_port;

 private:
  std::unique_ptr<Polyglot_scope> m_scope;
  bool m_terminating = false;
  void set_file_system();
  void throw_exception_object(poly_value exception) const;
  static void output_callback(const char *bytes, size_t length, void *data);
  static void error_callback(const char *bytes, size_t length, void *data);

  virtual poly_value create_exception_object(
      const std::string &error, poly_value exception_object) const = 0;
  virtual void output_handler(const char *bytes, size_t length) = 0;
  virtual void error_handler(const char *bytes, size_t length) = 0;
  void enable_debug();
  virtual std::optional<std::string> get_debug_port() const { return {}; }
};

}  // namespace polyglot
}  // namespace shcore

#endif  // MYSQLSHDK_SCRIPTING_POLYGLOT_LANGUAGES_POLYGLOT_LANGUAGE_H_
