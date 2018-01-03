/*
 * Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.
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

#include "plugin/x/client/xsession_impl.h"

#include <algorithm>
#include <array>
#include <map>
#include <memory>
#include <string>
#include <utility>

#include "errmsg.h"
#include "my_compiler.h"
#include "plugin/x/client/mysqlxclient/xerror.h"
#include "plugin/x/client/xcapability_builder.h"
#include "plugin/x/client/xconnection_impl.h"
#include "plugin/x/client/xprotocol_factory.h"
#include "plugin/x/client/xprotocol_impl.h"
#include "plugin/x/client/xquery_result_impl.h"
#include "plugin/x/generated/mysqlx_error.h"
#include "plugin/x/generated/mysqlx_version.h"


namespace xcl {

const char * const ER_TEXT_INVALID_SSL_MODE =
    "Invalid value for SSL mode";
const char * const ER_TEXT_OPTION_NOT_SUPPORTED =
    "Option not supported";
const char * const ER_TEXT_CAPABILITY_NOT_SUPPORTED =
    "Capability not supported";
const char * const ER_TEXT_OPTION_NOT_SUPPORTED_AFTER_CONNECTING =
    "Operation not supported after connecting";
const char * const ER_TEXT_NOT_CONNECTED =
    "Not connected";
const char * const ER_TEXT_ALREADY_CONNECTED =
    "Already connected";
const char * const ER_TEXT_TLS_IS_REQUIRED =
    "TLS was marked as \"REQUIRED\", but it was not configured";
const char * const ER_TEXT_CA_IS_REQUIRED =
    "TLS was marked that requires \"CA\", but it was not configured";
const char * const ER_TEXT_INVALID_IP_MODE =
    "Invalid value for host-IP resolver";

namespace details {

enum class Capability_datatype {
  String,
  Int,
  Bool
};

/** This class implemented the default behavior of the factory.
 *  Still it implements
 */
class Protocol_factory_default: public Protocol_factory {
 public:
  std::shared_ptr<XProtocol> create_protocol(
      std::shared_ptr<Context> context) override {
    return std::make_shared<Protocol_impl>(context, this);
  }

  std::unique_ptr<XConnection> create_connection(
      std::shared_ptr<Context> context) override {
    std::unique_ptr<XConnection> result{
      new Connection_impl(context)
    };
    return result;
  }

  std::unique_ptr<XQuery_result> create_result(
      std::shared_ptr<XProtocol> protocol,
      Query_instances *query_instances,
      std::shared_ptr<Context> context) override {
    std::unique_ptr<XQuery_result> result{
      new Query_result(protocol, query_instances, context)
    };

    return result;
  }
};

class  Any_filler : public Argument_value::Argument_visitor {
 public:
  explicit Any_filler(::Mysqlx::Datatypes::Any *any)
  : m_any(any) {
  }

 private:
  ::Mysqlx::Datatypes::Any *m_any;

  void visit() override {
    m_any->set_type(::Mysqlx::Datatypes::Any_Type_SCALAR);
    m_any->mutable_scalar()->set_type(
        ::Mysqlx::Datatypes::Scalar_Type_V_NULL);
  }

  void visit(const int64_t value) override {
    m_any->set_type(::Mysqlx::Datatypes::Any_Type_SCALAR);
    m_any->mutable_scalar()->set_type(
        ::Mysqlx::Datatypes::Scalar_Type_V_SINT);
    m_any->mutable_scalar()->set_v_signed_int(value);
  }

  void visit(const uint64_t value) override {
    m_any->set_type(::Mysqlx::Datatypes::Any_Type_SCALAR);
    m_any->mutable_scalar()->set_type(
        ::Mysqlx::Datatypes::Scalar_Type_V_UINT);
    m_any->mutable_scalar()->set_v_unsigned_int(value);
  }

  void visit(const double value) override {
    m_any->set_type(::Mysqlx::Datatypes::Any_Type_SCALAR);
    m_any->mutable_scalar()->set_type(
        ::Mysqlx::Datatypes::Scalar_Type_V_DOUBLE);
    m_any->mutable_scalar()->set_v_double(value);
  }

  void visit(const float value) override {
    m_any->set_type(::Mysqlx::Datatypes::Any_Type_SCALAR);
    m_any->mutable_scalar()->set_type(
        ::Mysqlx::Datatypes::Scalar_Type_V_FLOAT);
    m_any->mutable_scalar()->set_v_float(value);
  }

  void visit(const bool value) override {
    m_any->set_type(::Mysqlx::Datatypes::Any_Type_SCALAR);
    m_any->mutable_scalar()->set_type(
        ::Mysqlx::Datatypes::Scalar_Type_V_BOOL);
    m_any->mutable_scalar()->set_v_bool(value);
  }

  void visit(const Object &obj) override {
    m_any->set_type(::Mysqlx::Datatypes::Any_Type_OBJECT);
    auto any_object = m_any->mutable_obj();

    for (const auto &key_value : obj) {
      auto fld = any_object->add_fld();
      Any_filler filler(fld->mutable_value());

      fld->set_key(key_value.first);
      key_value.second.accept(&filler);
    }
  }

  void visit(const Arguments &values) override {
    m_any->set_type(::Mysqlx::Datatypes::Any_Type_ARRAY);
    auto any_array = m_any->mutable_array();

    for (const auto &value : values) {
      Any_filler filler(any_array->add_value());
      value.accept(&filler);
    }
  }

  void visit(const std::string &value,
             const Argument_value::String_type st) override {
    m_any->set_type(::Mysqlx::Datatypes::Any_Type_SCALAR);

    switch (st) {
    case Argument_value::String_type::TString:
    case Argument_value::String_type::TDecimal:
      m_any->mutable_scalar()->set_type(
          ::Mysqlx::Datatypes::Scalar_Type_V_STRING);
      m_any->mutable_scalar()->mutable_v_string()->set_value(value);
      break;

    case Argument_value::String_type::TOctets:
      m_any->mutable_scalar()->set_type(
          ::Mysqlx::Datatypes::Scalar_Type_V_OCTETS);
      m_any->mutable_scalar()->mutable_v_octets()->set_value(value);
      break;
    }
  }
};

bool scalar_get_v_uint(const Mysqlx::Datatypes::Scalar &scalar,
                       uint64_t *out_value) {
  if (scalar.type() !=
      Mysqlx::Datatypes::Scalar::V_UINT)
    return false;

  *out_value = scalar.v_unsigned_int();

  return true;
}

std::pair<std::string, Capability_datatype> get_capability_type(
    const XSession::Mysqlx_capability capability) {
  if (XSession::Capability_can_handle_expired_password ==
      capability)
    return {"client.pwd_expire_ok", Capability_datatype::Bool};

  if (XSession::Capability_client_interactive == capability)
    return {"client.interactive", Capability_datatype::Bool};

  return {};
}

const char *value_or_empty_string(const char *value) {
  if (nullptr == value)
    return "";

  return value;
}
const char *value_or_default_string(
    const char *value,
    const char *value_default) {
  if (nullptr == value)
    return value_default;

  if (0 == strlen(value))
    return value_default;

  return value;
}

}  // namespace details

Session_impl::Session_impl(std::unique_ptr<Protocol_factory> factory)
: m_context(std::make_shared<Context>()),
  m_factory(std::move(factory)) {
  if (nullptr == m_factory.get()) {
    m_factory.reset(new details::Protocol_factory_default());
  }

  setup_protocol();
}

Session_impl::~Session_impl() {
  auto &connection = get_protocol().get_connection();

  if (connection.state().is_connected()) {
    connection.close();
  }
}

XProtocol &Session_impl::get_protocol() {
  return *m_protocol;
}

XError Session_impl::set_mysql_option(
    const Mysqlx_option option,
    const bool value MY_ATTRIBUTE((unused))) {
  if (is_connected()) {
    return XError{CR_ALREADY_CONNECTED,
      ER_TEXT_OPTION_NOT_SUPPORTED_AFTER_CONNECTING};
  }

  switch (option) {
    case XSession::Mysqlx_option::Consume_all_notices:
      m_context->m_consume_all_notices = value;
      break;
    case XSession::Mysqlx_option::Compatibility_mode:
      m_compatibility_mode = value;
      break;
    default:
      return XError{
        CR_X_UNSUPPORTED_OPTION,
        ER_TEXT_OPTION_NOT_SUPPORTED};
  }

  return {};
}

XError Session_impl::set_mysql_option(
    const Mysqlx_option option,
    const char *value) {
  const std::string value_str = nullptr == value ? "" : value;

  return set_mysql_option(option, value_str);
}

XError Session_impl::set_mysql_option(
    const Mysqlx_option option,
    const std::string &value) {

  if (is_connected())
    return XError{CR_ALREADY_CONNECTED,
      ER_TEXT_OPTION_NOT_SUPPORTED_AFTER_CONNECTING};

  switch (option) {
    case Mysqlx_option::Hostname_resolve_to:
      return setup_ip_mode_from_text(value);
    case Mysqlx_option::Allowed_tls:
      m_context->m_ssl_config.m_tls_version = value;
      break;
    case Mysqlx_option::Ssl_mode:
      return setup_ssl_mode_from_text(value);
    case Mysqlx_option::Ssl_key:
      m_context->m_ssl_config.m_key = value;
      break;
    case Mysqlx_option::Ssl_ca:
      m_context->m_ssl_config.m_ca = value;
      break;
    case Mysqlx_option::Ssl_ca_path:
      m_context->m_ssl_config.m_ca_path = value;
      break;
    case Mysqlx_option::Ssl_cert:
      m_context->m_ssl_config.m_cert = value;
      break;
    case Mysqlx_option::Ssl_cipher:
      m_context->m_ssl_config.m_cipher = value;
      break;
    case Mysqlx_option::Ssl_crl:
      m_context->m_ssl_config.m_crl = value;
      break;
    case Mysqlx_option::Ssl_crl_path:
      m_context->m_ssl_config.m_crl_path = value;
      break;
    case Mysqlx_option::Authentication_method:
      return setup_authentication_methods_from_text({value});
    default:
      return XError{
        CR_X_UNSUPPORTED_OPTION,
        ER_TEXT_OPTION_NOT_SUPPORTED};
  }

  return {};
}

XError Session_impl::set_mysql_option(const Mysqlx_option option,
    const std::vector<std::string> &values_list) {

  if (is_connected())
    return XError{CR_ALREADY_CONNECTED,
      ER_TEXT_OPTION_NOT_SUPPORTED_AFTER_CONNECTING};

  switch (option) {
    case Mysqlx_option::Authentication_method:
      return setup_authentication_methods_from_text(values_list);
    default:
      return XError{
        CR_X_UNSUPPORTED_OPTION,
        ER_TEXT_OPTION_NOT_SUPPORTED};
  }
}

XError Session_impl::set_mysql_option(
    const Mysqlx_option option,
    const int64_t value) {

  if (is_connected())
    return XError{CR_ALREADY_CONNECTED,
      ER_TEXT_OPTION_NOT_SUPPORTED_AFTER_CONNECTING};

  switch (option) {
    case Mysqlx_option::Read_timeout:
      m_context->m_connection_config.m_timeout_read = value;
      break;

    case Mysqlx_option::Write_timeout:
      m_context->m_connection_config.m_timeout_write = value;
      break;

    case Mysqlx_option::Connect_timeout:
      m_context->m_connection_config.m_timeout_connect = value;
      break;

    default:
      return XError{
        CR_X_UNSUPPORTED_OPTION,
        ER_TEXT_OPTION_NOT_SUPPORTED};
  }

  return {};
}

XError Session_impl::set_capability(
    const Mysqlx_capability capability,
    const bool value) {
  auto capability_type = details::get_capability_type(capability);

  if (details::Capability_datatype::Bool != capability_type.second)
    return XError{
      CR_X_UNSUPPORTED_CAPABILITY_VALUE,
      ER_TEXT_CAPABILITY_NOT_SUPPORTED};

  m_capabilities[capability_type.first] = value;

  return XError();
}

XError Session_impl::set_capability(
    const Mysqlx_capability capability,
    const std::string &value) {
  auto capability_type = details::get_capability_type(capability);

  if (details::Capability_datatype::String != capability_type.second)
    return XError{
      CR_X_UNSUPPORTED_CAPABILITY_VALUE,
      ER_TEXT_CAPABILITY_NOT_SUPPORTED};

  m_capabilities[capability_type.first] = value;

  return {};
}

XError Session_impl::set_capability(
    const Mysqlx_capability capability,
    const char *value) {
  auto capability_type = details::get_capability_type(capability);

  if (details::Capability_datatype::String != capability_type.second)
    return XError{
      CR_X_UNSUPPORTED_CAPABILITY_VALUE,
      ER_TEXT_CAPABILITY_NOT_SUPPORTED};

  m_capabilities[capability_type.first] = value;

  return {};
}

XError Session_impl::set_capability(
    const Mysqlx_capability capability,
    const int64_t value) {
  auto capability_type = details::get_capability_type(capability);

  if (details::Capability_datatype::String != capability_type.second)
    return XError{
      CR_X_UNSUPPORTED_CAPABILITY_VALUE,
      ER_TEXT_CAPABILITY_NOT_SUPPORTED};

  m_capabilities[capability_type.first] = value;

  return {};
}

XError Session_impl::connect(
    const char *host,
    const uint16_t port,
    const char *user,
    const char *pass,
    const char *schema) {
  if (is_connected())
    return XError{CR_ALREADY_CONNECTED, ER_TEXT_ALREADY_CONNECTED};

  auto result = get_protocol().get_connection().connect(
      details::value_or_empty_string(host),
      port ? port : MYSQLX_TCP_PORT,
      m_internet_protocol);

  if (result)
    return result;

  auto connection_type =
      get_protocol().get_connection().state().get_connection_type();

  return authenticate(user, pass, schema, connection_type);
}

XError Session_impl::connect(
    const char *socket_file,
    const char *user,
    const char *pass,
    const char *schema) {
  if (is_connected())
    return XError{CR_ALREADY_CONNECTED, ER_TEXT_ALREADY_CONNECTED};

  auto result = get_protocol().get_connection().connect_to_localhost(
      details::value_or_default_string(socket_file, MYSQLX_UNIX_ADDR));

  if (result)
    return result;

  auto connection_type =
      get_protocol().get_connection().state().get_connection_type();

  return authenticate(user, pass, schema, connection_type);
}

XError Session_impl::reauthenticate(
    const char *user,
    const char *pass,
    const char *schema) {
  if (!is_connected())
    return XError{CR_CONNECTION_ERROR, ER_TEXT_NOT_CONNECTED};

  auto error = get_protocol().send(::Mysqlx::Session::Reset());

  if (error)
    return error;

  error = get_protocol().recv_ok();

  if (error)
    return error;

  auto connection_type =
      get_protocol().get_connection().state().get_connection_type();

  return authenticate(user, pass, schema, connection_type);
}

std::unique_ptr<XQuery_result> Session_impl::execute_sql(
    const std::string &sql,
    XError *out_error) {
  if (!is_connected()) {
    *out_error = XError{CR_CONNECTION_ERROR, ER_TEXT_NOT_CONNECTED};

    return {};
  }

  ::Mysqlx::Sql::StmtExecute stmt;

  stmt.set_stmt(sql);
  return m_protocol->execute_stmt(stmt, out_error);
}

std::unique_ptr<XQuery_result> Session_impl::execute_stmt(
    const std::string &ns,
    const std::string &sql,
    const Arguments &arguments,
    XError *out_error) {
  if (!is_connected()) {
    *out_error = XError{CR_CONNECTION_ERROR, ER_TEXT_NOT_CONNECTED};

    return {};
  }

  ::Mysqlx::Sql::StmtExecute stmt;

  stmt.set_stmt(sql);
  stmt.set_namespace_(ns);


  for (const auto &argument : arguments) {
    details::Any_filler filler(stmt.mutable_args()->Add());

    argument.accept(&filler);
  }

  return m_protocol->execute_stmt(stmt, out_error);
}

void Session_impl::close() {
  if (is_connected()) {
    m_protocol->execute_close();

    m_protocol.reset();
  }
}

void Session_impl::setup_protocol() {
  m_protocol = m_factory->create_protocol(m_context);
  setup_session_notices_handler();
  setup_general_notices_handler();
}

void Session_impl::setup_general_notices_handler() {
  auto context  = m_context;

  m_protocol->add_notice_handler(
          [context] (XProtocol *p MY_ATTRIBUTE((unused)),
                  const bool is_global MY_ATTRIBUTE((unused)),
                  const Mysqlx::Notice::Frame::Type type MY_ATTRIBUTE((unused)),
                  const char *payload MY_ATTRIBUTE((unused)),
                  const uint32_t payload_size MY_ATTRIBUTE((unused)))
                  -> Handler_result {
        return context->m_consume_all_notices ?
            Handler_result::Consumed :
            Handler_result::Continue;
      },
      Handler_position::End,
      Handler_priority_low);
}

void Session_impl::setup_session_notices_handler() {
  auto context  = m_context;

  m_protocol->add_notice_handler(
      [context] (XProtocol *p MY_ATTRIBUTE((unused)),
              const bool is_global MY_ATTRIBUTE((unused)),
              const Mysqlx::Notice::Frame::Type type,
              const char *payload,
              const uint32_t payload_size) -> Handler_result {
        return handle_notices(context, type, payload, payload_size);
      },
      Handler_position::End,
      Handler_priority_high);
}

bool Session_impl::is_connected() {
  if (!m_protocol)
    return false;

  return m_protocol->get_connection().state().is_connected();
}

XError Session_impl::authenticate(
    const char *user,
    const char *pass,
    const char *schema,
    Connection_type connection_type) {
  auto       &protocol   = get_protocol();
  auto       &connection = protocol.get_connection();

  if (!m_capabilities.empty()) {
    Capabilities_builder builder;

    auto capabilities_set = builder
        .add_capabilities_from_object(m_capabilities)
        .get_result();
    auto error = protocol.execute_set_capability(capabilities_set);

    if (error)
      return error;
  }

  if (!connection.state().is_ssl_activated()) {
    if (!connection.state().is_ssl_configured() &&
        m_context->m_ssl_config.does_mode_requires_ssl())
      return XError{
        CR_X_TLS_WRONG_CONFIGURATION,
        ER_TEXT_TLS_IS_REQUIRED};

    if (m_context->m_ssl_config.does_mode_requires_ca() &&
        !m_context->m_ssl_config.is_ca_configured())
      return XError{CR_X_TLS_WRONG_CONFIGURATION,
        ER_TEXT_CA_IS_REQUIRED};

    if (connection.state().is_ssl_configured()) {
      Capabilities_builder builder;
      auto capability_set_tls = builder.add_capability(
          "tls", Argument_value{true}).get_result();
      auto error = protocol.execute_set_capability(capability_set_tls);

      if (!error)
        error = connection.activate_tls();

      if (error) {
        if (ER_X_CAPABILITIES_PREPARE_FAILED != error.error() ||
            m_context->m_ssl_config.m_mode != Ssl_config::Mode::Ssl_preferred) {
          return error;
        }
      }
    }
  }

  const auto can_use_plain = connection.state().is_ssl_activated() ||
      (connection_type == Connection_type::Unix_socket);
  const auto &optional_auth_methods = validate_and_adjust_auth_methods(
      m_auth_methods, can_use_plain);
  const auto &error = optional_auth_methods.first;
  if (error)
    return error;

  XError auth_error;
  for (const auto &auth_method : optional_auth_methods.second) {
    if (auth_method == "PLAIN" && !can_use_plain) {
      if (&auth_method != &optional_auth_methods.second.back()) {
        // There are other auth methods in chain, lets try them
        continue;
      } else {
        return XError{CR_X_INVALID_AUTH_METHOD,
            "Invalid authentication method: PLAIN over unsecure channel"};
      }
    }
    auth_error = protocol.execute_authenticate(
        details::value_or_empty_string(user),
        details::value_or_empty_string(pass),
        details::value_or_empty_string(schema),
        auth_method);
    // Authentication successful, otherwise try to use different auth method
    if (!auth_error)
      return {};
  }

  return auth_error;
}

std::pair<XError, std::vector<std::string>>
Session_impl::validate_and_adjust_auth_methods(std::vector<Auth> auth_methods,
    const bool can_use_plain) {
  const auto auth_methods_count = auth_methods.size();
  if (auth_methods_count <= 1) {
    if (auth_methods_count == 0 ||
        (auth_methods_count == 1 && auth_methods[0] == Auth::Auto)) {
      // Authentication methods contain only "AUTO" or no auth method was given.
      // This means that the corresponding auth methods will be used:
      //   For MySQL 5.7:
      //     PLAIN if SSL is enabled, MYSQL41 otherwise
      //   For MySQL 8.0 and above:
      //     sequence of SHA256_MEMORY -> (optional) PLAIN -> MYSQL41
      auth_methods.clear();
      if (m_compatibility_mode) {
        if (can_use_plain)
          auth_methods.push_back(Auth::Plain);
        else
          auth_methods.push_back(Auth::Mysql41);
      } else {
        auth_methods.push_back(Auth::Sha256_memory);
        if (can_use_plain)
          auth_methods.push_back(Auth::Plain);
        auth_methods.push_back(Auth::Mysql41);
      }
    }
  } else {
    if (std::find(std::begin(auth_methods), std::end(auth_methods),
        Auth::Auto) != std::end(auth_methods))
      return {XError{CR_X_INVALID_AUTH_METHOD,
                    "Ambigious authentication methods given"}, {}};
  }

  std::vector<std::string> auth_method_string_list;
  std::transform(std::begin(auth_methods), std::end(auth_methods),
      std::back_inserter(auth_method_string_list), get_method_from_auth);
  return {{}, auth_method_string_list};
}

Handler_result Session_impl::handle_notices(
      std::shared_ptr<Context> context,
      const Mysqlx::Notice::Frame::Type type,
      const char *payload,
      const uint32_t payload_size) {
  if (Mysqlx::Notice::Frame_Type_SESSION_STATE_CHANGED == type) {
    Mysqlx::Notice::SessionStateChanged session_changed;

    if (session_changed.ParseFromArray(payload, payload_size) &&
        session_changed.IsInitialized() &&
        session_changed.has_value()) {
      if (Mysqlx::Notice::SessionStateChanged::CLIENT_ID_ASSIGNED ==
          session_changed.param()) {
        return details::scalar_get_v_uint(
            session_changed.value(),
            &context->m_client_id) ?
                Handler_result::Consumed :
                Handler_result::Error;
      }
    }
  }

  return Handler_result::Continue;
}

XError Session_impl::setup_ssl_mode_from_text(const std::string &value) {
  const std::size_t mode_text_max_lenght = 20;
  std::string mode_text;

  mode_text.reserve(mode_text_max_lenght);
  for (const auto c : value) {
    mode_text.push_back(toupper(c));
  }

  const std::map<std::string, Ssl_config::Mode> modes {
    { "PREFERRED",       Ssl_config::Mode::Ssl_preferred },
    { "DISABLED",        Ssl_config::Mode::Ssl_disabled },
    { "REQUIRED",        Ssl_config::Mode::Ssl_required },
    { "VERIFY_CA",       Ssl_config::Mode::Ssl_verify_ca },
    { "VERIFY_IDENTITY", Ssl_config::Mode::Ssl_verify_identity }
  };

  auto mode_value = modes.find(mode_text);

  if (modes.end() == mode_value)
    return XError{
      CR_X_UNSUPPORTED_OPTION_VALUE,
      ER_TEXT_INVALID_SSL_MODE};

  m_context->m_ssl_config.m_mode = mode_value->second;

  return {};
}

XError Session_impl::setup_authentication_methods_from_text(
    const std::vector<std::string> &values_list) {

  auto to_upper = [](std::string str) {
    for (auto &c : str)
      c = toupper(c);
    return str;
  };
  std::vector<std::string> auth_strings;
  std::transform(std::begin(values_list), std::end(values_list),
     std::back_inserter(auth_strings), to_upper);

  const std::map<std::string, Auth> modes {
    { "AUTO",          Auth::Auto },
    { "MYSQL41",       Auth::Mysql41 },
    { "PLAIN",         Auth::Plain },
    { "SHA256_MEMORY", Auth::Sha256_memory }
  };

  m_auth_methods.clear();
  for (const auto &mode_text : auth_strings) {
    auto mode_value = modes.find(mode_text);

    if (modes.end() == mode_value) {
      m_auth_methods.clear();
      return XError{CR_X_UNSUPPORTED_OPTION_VALUE, ER_TEXT_INVALID_SSL_MODE};
    }

    m_auth_methods.push_back(mode_value->second);
  }

  return {};
}

XError Session_impl::setup_ip_mode_from_text(const std::string &value) {
  std::string mode_text;

  for (const auto c : value) {
    mode_text.push_back(toupper(c));
  }

  static std::map<std::string, Internet_protocol> modes {
    { "ANY", Internet_protocol::Any},
    { "IP4", Internet_protocol::V4},
    { "IP6", Internet_protocol::V6}
  };

  auto mode_value = modes.find(mode_text);

  if (modes.end() == mode_value)
    return XError{
      CR_X_UNSUPPORTED_OPTION_VALUE,
      ER_TEXT_INVALID_IP_MODE};

  m_internet_protocol = mode_value->second;

  return {};
}

std::string Session_impl::get_method_from_auth(const Auth auth) {
  switch (auth) {
    case Auth::Auto:
      return "AUTO";
    case Auth::Mysql41:
      return "MYSQL41";
    case Auth::Sha256_memory:
      return "SHA256_MEMORY";
    case Auth::Plain:
      return "PLAIN";
    default:
      return "UNKNOWN";
  }
}

std::unique_ptr<XSession> create_session(const char *socket_file,
                                         const char *user, const char *pass,
                                         const char *schema,
                                         XError *out_error) {
  auto result = create_session();
  auto error  = result->connect(socket_file, user, pass, schema);

  if (error) {
    if (nullptr != out_error)
      *out_error = error;
    result.reset();
  }

  return result;
}

std::unique_ptr<XSession> create_session(const char *host, const uint16_t port,
                                         const char *user, const char *pass,
                                         const char *schema,
                                         XError *out_error) {
  auto result = create_session();
  auto error = result->connect(host, port, user, pass, schema);

  if (error) {
    if (nullptr != out_error)
      *out_error = error;
    result.reset();
  }

  return result;
}

std::unique_ptr<XSession> create_session() {
  std::unique_ptr<XSession> result {
    new Session_impl()
  };

  return result;
}

}  // namespace xcl

