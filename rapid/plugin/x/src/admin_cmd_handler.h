/*
 * Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#ifndef _ADMIN_CMD_HANDLER_H_
#define _ADMIN_CMD_HANDLER_H_

#include <string>
#include <map>
#include <vector>
#include <google/protobuf/repeated_field.h>
#include "ngs_common/smart_ptr.h"

#include "ngs/error_code.h"
#include "ngs/protocol_fwd.h"
#include "mysqlx_datatypes.pb.h"

namespace xpl
{
class Session;
class Sql_data_context;
class Session_options;

class Admin_command_handler
{
public:
  class Command_arguments
  {
  public:
    typedef std::vector<std::string> Argument_list;
    typedef ::google::protobuf::RepeatedPtrField< ::Mysqlx::Datatypes::Any > List;
    static const char* const PLACEHOLDER;

    virtual ~Command_arguments() {}
    virtual Command_arguments &string_arg(const char *name, std::string &ret_value, bool optional = false) = 0;
    virtual Command_arguments &string_list(const char *name, std::vector<std::string> &ret_value, bool optional = false) = 0;
    virtual Command_arguments &sint_arg(const char *name, int64_t &ret_value, bool optional = false) = 0;
    virtual Command_arguments &uint_arg(const char *name, uint64_t &ret_value, bool optional = false) = 0;
    virtual Command_arguments &bool_arg(const char *name, bool &ret_value, bool optional = false) = 0;
    virtual Command_arguments &docpath_arg(const char *name, std::string &ret_value, bool optional = false) = 0;
    virtual Command_arguments &object_list(const char *name, std::vector<Command_arguments*> &ret_value,
                                           bool optional = false, unsigned expected_members_count = 3) = 0;

    virtual bool is_end() const = 0;
    virtual const ngs::Error_code &end() = 0;
    virtual const ngs::Error_code &error() const = 0;
  };

  Admin_command_handler(Session &session);

  ngs::Error_code execute(const std::string &namespace_, const std::string &command, Command_arguments &args);

protected:
  typedef Command_arguments::Argument_list Argument_list;
  typedef Command_arguments::List Value_list;

  ngs::Error_code ping(Command_arguments &args);

  ngs::Error_code list_clients(Command_arguments &args);
  ngs::Error_code kill_client(Command_arguments &args);

  ngs::Error_code create_collection(Command_arguments &args);
  ngs::Error_code drop_collection(Command_arguments &args);
  ngs::Error_code ensure_collection(Command_arguments &args);

  ngs::Error_code create_collection_index(Command_arguments &args);
  ngs::Error_code drop_collection_index(Command_arguments &args);

  ngs::Error_code list_objects(Command_arguments &args);

  ngs::Error_code enable_notices(Command_arguments &args);
  ngs::Error_code disable_notices(Command_arguments &args);
  ngs::Error_code list_notices(Command_arguments &args);

  typedef ngs::Error_code (Admin_command_handler::*Method_ptr)(Command_arguments &args);
  static const struct Command_handler : private std::map<std::string, Method_ptr>
  {
    Command_handler();
    ngs::Error_code execute(Admin_command_handler *admin, const std::string &namespace_,
                            const std::string &command, Command_arguments &args) const;
  } m_command_handler;

  Session &m_session;
  Sql_data_context &m_da;
  Session_options &m_options;
};


class Admin_command_arguments_list: public Admin_command_handler::Command_arguments
{
public:
  explicit Admin_command_arguments_list(const List &args);

  virtual Admin_command_arguments_list &string_arg(const char *name, std::string &ret_value, bool optional);
  virtual Admin_command_arguments_list &string_list(const char *name, std::vector<std::string> &ret_value, bool optional);
  virtual Admin_command_arguments_list &sint_arg(const char *name, int64_t &ret_value, bool optional);
  virtual Admin_command_arguments_list &uint_arg(const char *name, uint64_t &ret_value, bool optional);
  virtual Admin_command_arguments_list &bool_arg(const char *name, bool &ret_value, bool optional);
  virtual Admin_command_arguments_list &docpath_arg(const char *name, std::string &ret_value, bool optional);
  virtual Admin_command_arguments_list &object_list(const char *name, std::vector<Command_arguments*> &ret_value,
                                                    bool optional, unsigned expected_members_count);

  virtual bool is_end() const;
  virtual const ngs::Error_code &end();
  virtual const ngs::Error_code &error() const { return m_error; }

protected:
  bool check_scalar_arg(const char *argname, Mysqlx::Datatypes::Scalar::Type type, const char *type_name, bool optional);
  void arg_type_mismatch(const char *argname, int argpos, const char *type);

  const List &m_args;
  List::const_iterator m_current;
  ngs::Error_code m_error;
  int m_args_consumed;
};


class Admin_command_arguments_object : public Admin_command_handler::Command_arguments
{
public:
  typedef ::Mysqlx::Datatypes::Object Object;
  explicit Admin_command_arguments_object(const List &args);
  explicit Admin_command_arguments_object(const Object &obj);

  virtual Admin_command_arguments_object &string_arg(const char *name, std::string &ret_value, bool optional);
  virtual Admin_command_arguments_object &string_list(const char *name, std::vector<std::string> &ret_value, bool optional);
  virtual Admin_command_arguments_object &sint_arg(const char *name, int64_t &ret_value, bool optional);
  virtual Admin_command_arguments_object &uint_arg(const char *name, uint64_t &ret_value, bool optional);
  virtual Admin_command_arguments_object &bool_arg(const char *name, bool &ret_value, bool optional);
  virtual Admin_command_arguments_object &docpath_arg(const char *name, std::string &ret_value, bool optional);
  virtual Admin_command_arguments_object &object_list(const char *name, std::vector<Command_arguments*> &ret_value,
                                                      bool optional, unsigned expected_members_count);

  virtual bool is_end() const;
  virtual const ngs::Error_code &end();
  virtual const ngs::Error_code &error() const { return m_error; }

private:
  typedef ::Mysqlx::Datatypes::Any Any;
  typedef ::google::protobuf::RepeatedPtrField<Object::ObjectField> Object_field_list;

  template<typename H> void get_scalar_arg(const char *name, bool optional, H &handler);
  template<typename H> void get_scalar_value(const Any &value, H &handler);
  const Object::ObjectField *get_object_field(const char *name, bool optional);
  void expected_value_error(const char *name);
  Admin_command_arguments_object *add_sub_object(const Object &object);

  const bool m_args_empty;
  const bool m_is_object;
  const Object &m_object;
  ngs::Error_code m_error;
  int m_args_consumed;
  std::vector<ngs::shared_ptr<Admin_command_arguments_object> > m_sub_objects;
};

} // namespace xpl

#endif
