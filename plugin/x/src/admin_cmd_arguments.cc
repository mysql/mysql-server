/*
 * Copyright (c) 2017, 2019, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#include "plugin/x/src/admin_cmd_arguments.h"

#include <algorithm>
#include <limits>

#include "plugin/x/ngs/include/ngs/mysqlx/getter_any.h"
#include "plugin/x/src/xpl_error.h"
#include "plugin/x/src/xpl_regex.h"

namespace xpl {

namespace {
using Argument_appearance =
    Admin_command_handler::Command_arguments::Appearance_type;
inline bool is_optional(const Argument_appearance appearance) {
  return appearance == Argument_appearance::k_optional;
}
}  // namespace

Admin_command_arguments_list::Admin_command_arguments_list(const List &args)
    : m_args(args), m_current(m_args.begin()), m_args_consumed(0) {}

Admin_command_arguments_list &Admin_command_arguments_list::string_arg(
    Argument_name_list name, std::string *ret_value,
    const Appearance_type appearance) {
  if (check_scalar_arg(name, Mysqlx::Datatypes::Scalar::V_STRING, "string",
                       appearance)) {
    const std::string &value = m_current->scalar().v_string().value();
    if (memchr(value.data(), 0, value.length())) {
      m_error = ngs::Error(ER_X_CMD_ARGUMENT_VALUE,
                           "Invalid value for argument '%s'", *name.begin());
      return *this;
    }
    *ret_value = value;
    ++m_current;
  }
  return *this;
}

Admin_command_arguments_list &Admin_command_arguments_list::string_list(
    Argument_name_list name, std::vector<std::string> *ret_value,
    const Appearance_type appearance) {
  std::string value;
  do {
    string_arg(name, &value, appearance);
    ret_value->push_back(value);
    value.clear();
  } while (!is_end());
  return *this;
}

Admin_command_arguments_list &Admin_command_arguments_list::sint_arg(
    Argument_name_list name, int64_t *ret_value,
    const Appearance_type appearance) {
  if (check_scalar_arg(name, Mysqlx::Datatypes::Scalar::V_SINT, "signed int",
                       appearance)) {
    if (m_current->scalar().type() == Mysqlx::Datatypes::Scalar::V_UINT)
      *ret_value = (int64_t)m_current->scalar().v_unsigned_int();
    else if (m_current->scalar().type() == Mysqlx::Datatypes::Scalar::V_SINT)
      *ret_value = m_current->scalar().v_signed_int();
    ++m_current;
  }
  return *this;
}

Admin_command_arguments_list &Admin_command_arguments_list::uint_arg(
    Argument_name_list name, uint64_t *ret_value,
    const Appearance_type appearance) {
  if (check_scalar_arg(name, Mysqlx::Datatypes::Scalar::V_UINT, "unsigned int",
                       appearance)) {
    if (m_current->scalar().type() == Mysqlx::Datatypes::Scalar::V_UINT)
      *ret_value = m_current->scalar().v_unsigned_int();
    else if (m_current->scalar().type() == Mysqlx::Datatypes::Scalar::V_SINT)
      *ret_value = (uint64_t)m_current->scalar().v_signed_int();
    ++m_current;
  }
  return *this;
}

Admin_command_arguments_list &Admin_command_arguments_list::bool_arg(
    Argument_name_list name, bool *ret_value,
    const Appearance_type appearance) {
  if (check_scalar_arg(name, Mysqlx::Datatypes::Scalar::V_BOOL, "bool",
                       appearance)) {
    *ret_value = m_current->scalar().v_bool();
    ++m_current;
  }
  return *this;
}

Admin_command_arguments_list &Admin_command_arguments_list::docpath_arg(
    Argument_name_list name, std::string *ret_value, const Appearance_type) {
  m_args_consumed++;
  if (!m_error) {
    if (m_current == m_args.end()) {
      m_error = ngs::Error(ER_X_CMD_NUM_ARGUMENTS, "Too few arguments");
    } else {
      if (m_current->type() == Mysqlx::Datatypes::Any::SCALAR &&
          m_current->has_scalar() &&
          (m_current->scalar().type() == Mysqlx::Datatypes::Scalar::V_STRING &&
           m_current->scalar().has_v_string())) {
        *ret_value = m_current->scalar().v_string().value();
        // We could perform some extra validation on the document path here, but
        // since the path will be quoted and escaped when used, it would be
        // redundant.
        // Plus, the best way to have the exact same syntax as the server
        // is to let the server do it.
        if (ret_value->empty() || ret_value->size() < 2)
          m_error = ngs::Error(ER_X_CMD_ARGUMENT_VALUE,
                               "Invalid document path value for argument %s",
                               *name.begin());
      } else {
        arg_type_mismatch(*name.begin(), m_args_consumed,
                          "document path string");
      }
    }
    ++m_current;
  }
  return *this;
}

Admin_command_arguments_list &Admin_command_arguments_list::object_list(
    Argument_name_list name, std::vector<Command_arguments *> *ret_value,
    const Appearance_type, unsigned expected_members_count) {
  List::difference_type left = m_args.end() - m_current;
  if (left % expected_members_count > 0) {
    m_error = ngs::Error(ER_X_CMD_NUM_ARGUMENTS,
                         "Too few values for argument '%s'", *name.begin());
    return *this;
  }
  for (unsigned i = 0; i < left / expected_members_count; ++i)
    ret_value->push_back(this);
  return *this;
}

bool Admin_command_arguments_list::is_end() const {
  return !(m_error.error == 0 && m_args.size() > m_args_consumed);
}

const ngs::Error_code &Admin_command_arguments_list::end() {
  if (m_error.error == ER_X_CMD_NUM_ARGUMENTS ||
      (m_error.error == 0 && m_args.size() > m_args_consumed)) {
    m_error = ngs::Error(ER_X_CMD_NUM_ARGUMENTS,
                         "Invalid number of arguments, expected %i but got %i",
                         m_args_consumed, m_args.size());
  }
  return m_error;
}

void Admin_command_arguments_list::arg_type_mismatch(const char *argname,
                                                     int argpos,
                                                     const char *type) {
  m_error = ngs::Error(ER_X_CMD_ARGUMENT_TYPE,
                       "Invalid type for argument '%s' at #%i (should be %s)",
                       argname, argpos, type);
}

bool Admin_command_arguments_list::check_scalar_arg(
    const Argument_name_list &argname, Mysqlx::Datatypes::Scalar::Type type,
    const char *type_name, const Appearance_type appearance) {
  m_args_consumed++;
  if (!m_error) {
    if (m_current == m_args.end()) {
      if (!is_optional(appearance))
        m_error = ngs::Error(ER_X_CMD_NUM_ARGUMENTS,
                             "Insufficient number of arguments");
    } else {
      if (m_current->type() == Mysqlx::Datatypes::Any::SCALAR &&
          m_current->has_scalar()) {
        if (m_current->scalar().type() == type) {
          // TODO(user): add charset check for strings?
          // return true only if value to be consumed is available
          return true;
        } else if (type == Mysqlx::Datatypes::Scalar::V_SINT &&
                   m_current->scalar().type() ==
                       Mysqlx::Datatypes::Scalar::V_UINT &&
                   m_current->scalar().v_unsigned_int() <
                       static_cast<uint64_t>(
                           std::numeric_limits<int64_t>::max())) {
          return true;
        } else if (type == Mysqlx::Datatypes::Scalar::V_UINT &&
                   m_current->scalar().type() ==
                       Mysqlx::Datatypes::Scalar::V_SINT &&
                   m_current->scalar().v_signed_int() >= 0) {
          return true;
        } else {
          if (!(is_optional(appearance) &&
                m_current->scalar().type() ==
                    Mysqlx::Datatypes::Scalar::V_NULL)) {
            arg_type_mismatch(*argname.begin(), m_args_consumed, type_name);
          }
        }
      } else {
        arg_type_mismatch(*argname.begin(), m_args_consumed, type_name);
      }
      ++m_current;
    }
  }
  return false;
}

namespace {
using Object_field = ::Mysqlx::Datatypes::Object_ObjectField;

template <typename T>
class General_argument_validator {
 public:
  General_argument_validator(const char *, bool *) {}
  void operator()(const T &input, T *output) { *output = input; }
};

template <typename Type, typename Validator = General_argument_validator<Type>>
class Argument_type_handler {
 public:
  Argument_type_handler(const char *name, Type *value)
      : m_validator(name, &m_error), m_value(value), m_error(false) {}

  explicit Argument_type_handler(const char *name)
      : m_validator(name, &m_error), m_value(nullptr), m_error(false) {}

  void assign(Type *value) { m_value = value; }
  void operator()(const Type &value) { m_validator(value, m_value); }
  void operator()() { set_error(); }
  template <typename Other_type>
  void operator()(const Other_type &, const uint32_t = 0) {
    this->operator()();
  }
  bool is_error() const { return m_error; }
  void set_error() { m_error = true; }

 private:
  Validator m_validator;
  Type *m_value;
  bool m_error;
};

class String_argument_validator {
 public:
  String_argument_validator(const char *name, bool *error)
      : m_name(name), m_error(error) {}

  void operator()(const std::string &input, std::string *output) {
    if (memchr(input.data(), 0, input.length())) {
      *m_error = true;
      return;
    }
    *output = input;
  }

 protected:
  const char *m_name;
  bool *m_error;
};

class Docpath_argument_validator : String_argument_validator {
 public:
  Docpath_argument_validator(const char *name, bool *error)
      : String_argument_validator(name, error) {}

  void operator()(const std::string &input, std::string *output) {
    static const std::string k_doc_member_regex =
        "^" DOC_MEMBER_REGEX_NO_BACKSLASH_ESCAPES "$";
    static const Regex re(k_doc_member_regex.c_str());
    std::string value;
    String_argument_validator::operator()(input, &value);
    if (*m_error) return;
    if (re.match(value.c_str()))
      *output = value;
    else
      *m_error = true;
  }
};

inline std::string get_indexed_name(const std::string &name, const int i) {
  return name + "[" + std::to_string(i) + "]";
}

}  // namespace

Admin_command_arguments_object::Admin_command_arguments_object(const List &args)
    : m_args_empty(args.size() == 0),
      m_is_object(args.size() == 1 && args.Get(0).has_obj()),
      m_object(m_is_object ? args.Get(0).obj() : Object::default_instance()),
      m_args_consumed(0) {}

Admin_command_arguments_object::Admin_command_arguments_object(
    const Object &obj)
    : m_args_empty(true),
      m_is_object(true),
      m_object(obj),
      m_args_consumed(0) {}

void Admin_command_arguments_object::set_number_args_error(
    const std::string &name) {
  m_error = ngs::Error(ER_X_CMD_NUM_ARGUMENTS,
                       "Invalid number of arguments, expected value for '%s'",
                       get_path(name).c_str());
}

void Admin_command_arguments_object::set_arg_value_error(
    const std::string &name) {
  m_error =
      ngs::Error(ER_X_CMD_ARGUMENT_VALUE, "Invalid value for argument '%s'",
                 get_path(name).c_str());
}

template <typename H>
void Admin_command_arguments_object::get_scalar_arg(
    const Argument_name_list &name, const Appearance_type appearance,
    H *handler) {
  const Object::ObjectField *field = get_object_field(name, appearance);
  if (!field) return;

  get_scalar_value(field->value(), handler);
  if (handler->is_error()) set_arg_value_error(*name.begin());
}

const Admin_command_arguments_object::Object::ObjectField *
Admin_command_arguments_object::get_object_field(
    const Argument_name_list &name, const Appearance_type appearance) {
  if (m_error) return nullptr;

  ++m_args_consumed;

  if (!m_is_object) {
    if (!is_optional(appearance)) set_number_args_error(*name.begin());
    return nullptr;
  }

  for (const auto &arg_name : name) {
    const Object_field_list &fld = m_object.fld();
    Object_field_list::const_iterator i = std::find_if(
        fld.begin(), fld.end(), [arg_name](const Object_field &fld) -> bool {
          return fld.has_key() && fld.key() == arg_name;
        });
    if (i != fld.end()) {
      return &(*i);
    }
  }

  if (!is_optional(appearance)) set_number_args_error(*name.begin());
  return nullptr;
}

template <typename H>
void Admin_command_arguments_object::get_scalar_value(const Any &value,
                                                      H *handler) {
  try {
    ngs::Getter_any::put_scalar_value_to_functor(value, *handler);
  } catch (const ngs::Error_code &) {
    handler->set_error();
  }
}

Admin_command_arguments_object &Admin_command_arguments_object::string_arg(
    Argument_name_list name, std::string *ret_value,
    const Appearance_type appearance) {
  Argument_type_handler<std::string, String_argument_validator> handler(
      *name.begin(), ret_value);
  get_scalar_arg(name, appearance, &handler);
  return *this;
}

Admin_command_arguments_object &Admin_command_arguments_object::string_list(
    Argument_name_list name, std::vector<std::string> *ret_value,
    const Appearance_type appearance) {
  const Object::ObjectField *field = get_object_field(name, appearance);
  if (!field) return *this;

  if (!field->value().has_type()) {
    set_number_args_error(*name.begin());
    return *this;
  }

  std::vector<std::string> values;
  Argument_type_handler<std::string, String_argument_validator> handler(
      *name.begin());

  switch (field->value().type()) {
    case ::Mysqlx::Datatypes::Any_Type_ARRAY:
      if (field->value().array().value_size() == 0) {
        set_arg_value_error(*name.begin());
        break;
      }
      for (int i = 0; i < field->value().array().value_size(); ++i) {
        handler.assign(&(*values.insert(values.end(), "")));
        get_scalar_value(field->value().array().value(i), &handler);
        if (handler.is_error()) {
          set_arg_value_error(get_indexed_name(*name.begin(), i));
          break;
        }
      }
      break;

    case ::Mysqlx::Datatypes::Any_Type_SCALAR:
      handler.assign(&(*values.insert(values.end(), "")));
      get_scalar_value(field->value(), &handler);
      if (handler.is_error()) set_arg_value_error(*name.begin());
      break;

    default:
      set_arg_value_error(*name.begin());
  }

  if (!m_error) *ret_value = values;

  return *this;
}

Admin_command_arguments_object &Admin_command_arguments_object::sint_arg(
    Argument_name_list name, int64_t *ret_value,
    const Appearance_type appearance) {
  Argument_type_handler<google::protobuf::int64> handler(*name.begin(),
                                                         ret_value);
  get_scalar_arg(name, appearance, &handler);
  return *this;
}

Admin_command_arguments_object &Admin_command_arguments_object::uint_arg(
    Argument_name_list name, uint64_t *ret_value,
    const Appearance_type appearance) {
  Argument_type_handler<google::protobuf::uint64> handler(*name.begin(),
                                                          ret_value);
  get_scalar_arg(name, appearance, &handler);
  return *this;
}

Admin_command_arguments_object &Admin_command_arguments_object::bool_arg(
    Argument_name_list name, bool *ret_value,
    const Appearance_type appearance) {
  Argument_type_handler<bool> handler(*name.begin(), ret_value);
  get_scalar_arg(name, appearance, &handler);
  return *this;
}

Admin_command_arguments_object &Admin_command_arguments_object::docpath_arg(
    Argument_name_list name, std::string *ret_value,
    const Appearance_type appearance) {
  Argument_type_handler<std::string, Docpath_argument_validator> handler(
      *name.begin(), ret_value);
  get_scalar_arg(name, appearance, &handler);
  return *this;
}

Admin_command_arguments_object &Admin_command_arguments_object::object_list(
    Argument_name_list name, std::vector<Command_arguments *> *ret_value,
    const Appearance_type appearance, unsigned) {
  const Object::ObjectField *field = get_object_field(name, appearance);
  if (!field) return *this;

  if (!field->value().has_type()) {
    set_number_args_error(*name.begin());
    return *this;
  }

  std::vector<Command_arguments *> values;
  switch (field->value().type()) {
    case ::Mysqlx::Datatypes::Any_Type_ARRAY:
      if (field->value().array().value_size() == 0) {
        set_arg_value_error(*name.begin());
        break;
      }
      for (int i = 0; i < field->value().array().value_size(); ++i) {
        const Any &any = field->value().array().value(i);
        if (!any.has_type() ||
            any.type() != ::Mysqlx::Datatypes::Any_Type_OBJECT) {
          set_arg_value_error(get_indexed_name(*name.begin(), i));
          break;
        }
        values.push_back(add_sub_object(
            any.obj(), get_path(get_indexed_name(*name.begin(), i))));
      }
      break;

    case ::Mysqlx::Datatypes::Any_Type_OBJECT:
      values.push_back(
          add_sub_object(field->value().obj(), get_path(*name.begin())));
      break;

    default:
      set_arg_value_error(*name.begin());
  }

  if (!m_error) *ret_value = values;

  return *this;
}

Admin_command_arguments_object *Admin_command_arguments_object::add_sub_object(
    const Object &object, const std::string &path) {
  Admin_command_arguments_object *obj =
      new Admin_command_arguments_object(object);
  m_sub_objects.push_back(std::shared_ptr<Admin_command_arguments_object>(obj));
  obj->set_path(path);
  return obj;
}

const ngs::Error_code &Admin_command_arguments_object::end() {
  if (m_error) return m_error;

  if (m_is_object) {
    if (m_object.fld().size() > m_args_consumed)
      m_error =
          ngs::Error(ER_X_CMD_NUM_ARGUMENTS,
                     "Invalid number of arguments, expected %i but got %i",
                     m_args_consumed, m_object.fld().size());
  } else {
    if (!m_args_empty)
      m_error =
          ngs::Error(ER_X_CMD_ARGUMENT_TYPE,
                     "Invalid type of arguments, expected object of arguments");
  }
  return m_error;
}

bool Admin_command_arguments_object::is_end() const {
  return !(m_error.error == 0 && m_is_object &&
           m_object.fld().size() > m_args_consumed);
}

}  // namespace xpl
