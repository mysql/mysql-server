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

#include "polyglot_wrappers/types_polyglot.h"

#include "utils/polyglot_api_clean.h"

#include "languages/polyglot_language.h"
#include "utils/polyglot_error.h"

namespace shcore {
namespace polyglot {
Polyglot_object::Polyglot_object(const Polyglot_type_bridger *type_bridger,
                                 poly_thread thread, poly_context context,
                                 poly_value object,
                                 const std::string &class_name)
    : m_types{type_bridger},
      m_thread{thread},
      m_context{context},
      m_object{m_thread, object},
      m_class_name{class_name} {}

std::string Polyglot_object::class_name() const { return m_class_name; }

void Polyglot_object::append_json(JSON_dumper &dumper) const {
  dumper.start_object();
  dumper.append_string("class", class_name());
  dumper.end_object();
}

std::vector<std::string> Polyglot_object::get_members() const {
  size_t size{0};

  throw_if_error(poly_value_get_member_keys, m_thread, m_context,
                 m_object.get(), &size, nullptr);

  std::vector<poly_value> poly_keys;
  poly_keys.resize(size);

  throw_if_error(poly_value_get_member_keys, m_thread, m_context,
                 m_object.get(), &size, &poly_keys[0]);

  std::vector<std::string> keys;
  for (auto key : poly_keys) {
    keys.push_back(to_string(m_thread, key));
  }

  return keys;
}

Value Polyglot_object::get_member(const std::string &prop) const {
  return m_types->poly_value_to_native_value(get_poly_member(prop));
}

poly_value Polyglot_object::get_poly_member(const std::string &prop) const {
  poly_value member;
  throw_if_error(poly_value_get_member, m_thread, m_object.get(), prop.c_str(),
                 &member);

  return member;
}

bool Polyglot_object::has_member(const std::string &prop) const {
  bool found = false;
  throw_if_error(poly_value_has_member, m_thread, m_object.get(), prop.c_str(),
                 &found);

  return found;
}

void Polyglot_object::set_member(const std::string &prop, Value value) {
  set_poly_member(prop, m_types->native_value_to_poly_value(value));
}

void Polyglot_object::set_poly_member(const std::string &prop,
                                      poly_value value) {
  throw_if_error(poly_value_put_member, m_thread, m_object.get(), prop.c_str(),
                 value);
}

Value Polyglot_object::call(const std::string &name,
                            const std::vector<Value> &args) {
  Value ret_val = {};

  const auto member = get_poly_member(name);

  bool executable = false;
  throw_if_error(poly_value_can_execute, m_thread, member, &executable);

  if (executable) {
    std::vector<poly_value> poly_args;
    poly_args.reserve(args.size());
    for (const auto &arg : args) {
      poly_args.push_back(m_types->native_value_to_poly_value(arg));
    }

    poly_value result;
    throw_if_error(poly_value_execute, m_thread, member, &poly_args[0],
                   poly_args.size(), &result);
    ret_val = m_types->poly_value_to_native_value(result);
  } else {
    throw std::runtime_error("Called member " + name +
                             " of JS object is not a function");
  }

  return ret_val;
}

bool Polyglot_object::remove_member(const std::string &name) {
  bool removed{false};

  throw_if_error(poly_value_remove_member, m_thread, m_object.get(),
                 name.c_str(), &removed);

  return removed;
}

bool Polyglot_object::is_exception() const {
  bool is_exception = false;
  throw_if_error(poly_value_is_exception, m_thread, m_object.get(),
                 &is_exception);

  return is_exception;
}

void Polyglot_object::throw_exception() const {
  const auto rc = poly_value_throw_exception(m_thread, m_object.get());
  throw Polyglot_error(m_thread, rc);
}

Polyglot_function::Polyglot_function(std::weak_ptr<Polyglot_language> language,
                                     poly_value function)
    : m_language(std::move(language)) {
  const auto ctx = m_language.lock();
  if (!ctx) {
    throw std::logic_error(
        "Unable to wrap JavaScript function, context is gone!");
  }

  m_function = ctx->store(function);

  poly_value name;
  throw_if_error(poly_value_get_member, ctx->thread(), m_function, "name",
                 &name);

  m_name = ctx->to_string(name);
}
Polyglot_function::~Polyglot_function() {
  const auto ctx = m_language.lock();
  if (ctx) {
    ctx->erase(m_function);
  }
}

Value Polyglot_function::invoke(const std::vector<Value> &args) {
  Value ret_val = {};

  const auto ctx = m_language.lock();
  if (!ctx) {
    throw std::logic_error(
        "Unable to execute polyglot function, context is gone!");
  }

  std::vector<poly_value> poly_args;
  for (const auto &arg : args) {
    poly_args.push_back(ctx->convert(arg));
  }

  poly_value result;
  throw_if_error(poly_value_execute, ctx->thread(), m_function, &poly_args[0],
                 poly_args.size(), &result);

  ret_val = ctx->convert(result);

  return ret_val;
}
}  // namespace polyglot
}  // namespace shcore