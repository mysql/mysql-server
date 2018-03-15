/*
 * Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.
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
#include <set>
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

const char *const ER_TEXT_OPTION_VALUE_IS_SCALAR =
    "Value requires to be set through non array function";
const char *const ER_TEXT_INVALID_SSL_MODE = "Invalid value for SSL mode";
const char *const ER_TEXT_INVALID_SSL_FIPS_MODE =
    "Invalid value for SSL fips mode";
const char *const ER_TEXT_OPTION_NOT_SUPPORTED = "Option not supported";
const char *const ER_TEXT_CAPABILITY_NOT_SUPPORTED = "Capability not supported";
const char *const ER_TEXT_OPTION_NOT_SUPPORTED_AFTER_CONNECTING =
    "Operation not supported after connecting";
const char *const ER_TEXT_NOT_CONNECTED = "Not connected";
const char *const ER_TEXT_ALREADY_CONNECTED = "Already connected";
const char *const ER_TEXT_CA_IS_REQUIRED =
    "TLS was marked that requires \"CA\", but it was not configured";
const char *const ER_TEXT_INVALID_IP_MODE =
    "Invalid value for host-IP resolver";
const char *const ER_TEXT_INVALID_AUTHENTICATION_CONFIGURED =
    "Ambiguous authentication methods given";

namespace details {

enum class Capability_datatype { String, Int, Bool };

/** This class implemented the default behavior of the factory.
 *  Still it implements
 */
class Protocol_factory_default : public Protocol_factory {
 public:
  std::shared_ptr<XProtocol> create_protocol(
      std::shared_ptr<Context> context) override {
    return std::make_shared<Protocol_impl>(context, this);
  }

  std::unique_ptr<XConnection> create_connection(
      std::shared_ptr<Context> context) override {
    std::unique_ptr<XConnection> result{new Connection_impl(context)};
    return result;
  }

  std::unique_ptr<XQuery_result> create_result(
      std::shared_ptr<XProtocol> protocol, Query_instances *query_instances,
      std::shared_ptr<Context> context) override {
    std::unique_ptr<XQuery_result> result{
        new Query_result(protocol, query_instances, context)};

    return result;
  }
};

class Any_filler : public Argument_value::Argument_visitor {
 public:
  explicit Any_filler(::Mysqlx::Datatypes::Any *any) : m_any(any) {}

 private:
  ::Mysqlx::Datatypes::Any *m_any;

  void visit() override {
    m_any->set_type(::Mysqlx::Datatypes::Any_Type_SCALAR);
    m_any->mutable_scalar()->set_type(::Mysqlx::Datatypes::Scalar_Type_V_NULL);
  }

  void visit(const int64_t value) override {
    m_any->set_type(::Mysqlx::Datatypes::Any_Type_SCALAR);
    m_any->mutable_scalar()->set_type(::Mysqlx::Datatypes::Scalar_Type_V_SINT);
    m_any->mutable_scalar()->set_v_signed_int(value);
  }

  void visit(const uint64_t value) override {
    m_any->set_type(::Mysqlx::Datatypes::Any_Type_SCALAR);
    m_any->mutable_scalar()->set_type(::Mysqlx::Datatypes::Scalar_Type_V_UINT);
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
    m_any->mutable_scalar()->set_type(::Mysqlx::Datatypes::Scalar_Type_V_FLOAT);
    m_any->mutable_scalar()->set_v_float(value);
  }

  void visit(const bool value) override {
    m_any->set_type(::Mysqlx::Datatypes::Any_Type_SCALAR);
    m_any->mutable_scalar()->set_type(::Mysqlx::Datatypes::Scalar_Type_V_BOOL);
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
  if (scalar.type() != Mysqlx::Datatypes::Scalar::V_UINT) return false;

  *out_value = scalar.v_unsigned_int();

  return true;
}

bool get_array_of_strings_from_any(const Mysqlx::Datatypes::Any &any,
                                   std::vector<std::string> *out_strings) {
  out_strings->clear();

  if (!any.has_type() || Mysqlx::Datatypes::Any_Type_ARRAY != any.type())
    return false;

  for (const auto &element : any.array().value()) {
    if (!element.has_type() ||
        Mysqlx::Datatypes::Any_Type_SCALAR != element.type())
      return false;

    const auto &scalar = element.scalar();

    if (!scalar.has_type()) return false;

    switch (scalar.type()) {
      case Mysqlx::Datatypes::Scalar_Type_V_STRING:
        out_strings->push_back(scalar.v_string().value());
        break;

      case Mysqlx::Datatypes::Scalar_Type_V_OCTETS:
        out_strings->push_back(scalar.v_octets().value());
        break;

      default:
        return false;
    }
  }

  return true;
}

std::pair<std::string, Capability_datatype> get_capability_type(
    const XSession::Mysqlx_capability capability) {
  if (XSession::Capability_can_handle_expired_password == capability)
    return {"client.pwd_expire_ok", Capability_datatype::Bool};

  if (XSession::Capability_client_interactive == capability)
    return {"client.interactive", Capability_datatype::Bool};

  return {};
}

template <typename Container_type>
XError translate_texts_into_auth_types(
    const std::vector<std::string> &values_list, Container_type *out_auths_list,
    const bool ignore_not_found = false) {
  auto to_upper = [](std::string str) {
    for (auto &c : str) c = toupper(c);
    return str;
  };
  std::vector<std::string> auth_strings;
  std::transform(std::begin(values_list), std::end(values_list),
                 std::back_inserter(auth_strings), to_upper);

  const std::set<Session_impl::Auth> scalar_values{
      Session_impl::Auth::Auto, Session_impl::Auth::Auto_from_capabilities};

  const std::map<std::string, Session_impl::Auth> modes{
      {"AUTO", Session_impl::Auth::Auto},
      {"FROM_CAPABILITIES", Session_impl::Auth::Auto_from_capabilities},
      {"FALLBACK", Session_impl::Auth::Auto_fallback},
      {"MYSQL41", Session_impl::Auth::Mysql41},
      {"PLAIN", Session_impl::Auth::Plain},
      {"SHA256_MEMORY", Session_impl::Auth::Sha256_memory}};

  out_auths_list->clear();
  for (const auto &mode_text : auth_strings) {
    auto mode_value = modes.find(mode_text);

    if (modes.end() == mode_value) {
      if (ignore_not_found) continue;

      out_auths_list->clear();
      return XError{CR_X_UNSUPPORTED_OPTION_VALUE, ER_TEXT_INVALID_SSL_MODE};
    }

    if (1 < auth_strings.size() &&
        0 < scalar_values.count(mode_value->second)) {
      out_auths_list->clear();
      return XError{CR_X_UNSUPPORTED_OPTION_VALUE,
                    ER_TEXT_OPTION_VALUE_IS_SCALAR};
    }

    out_auths_list->insert(out_auths_list->end(), mode_value->second);
  }

  return {};
}

const char *value_or_empty_string(const char *value) {
  if (nullptr == value) return "";

  return value;
}
const char *value_or_default_string(const char *value,
                                    const char *value_default) {
  if (nullptr == value) return value_default;

  if (0 == strlen(value)) return value_default;

  return value;
}

}  // namespace details

Session_impl::Session_impl(std::unique_ptr<Protocol_factory> factory)
    : m_context(std::make_shared<Context>()), m_factory(std::move(factory)) {
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

XProtocol &Session_impl::get_protocol() { return *m_protocol; }

XError Session_impl::set_mysql_option(const Mysqlx_option option,
                                      const bool value MY_ATTRIBUTE((unused))) {
  if (is_connected()) {
    return XError{CR_ALREADY_CONNECTED,
                  ER_TEXT_OPTION_NOT_SUPPORTED_AFTER_CONNECTING};
  }

  switch (option) {
    case XSession::Mysqlx_option::Consume_all_notices:
      m_context->m_consume_all_notices = value;
      break;
    default:
      return XError{CR_X_UNSUPPORTED_OPTION, ER_TEXT_OPTION_NOT_SUPPORTED};
  }

  return {};
}

XError Session_impl::set_mysql_option(const Mysqlx_option option,
                                      const char *value) {
  const std::string value_str = nullptr == value ? "" : value;

  return set_mysql_option(option, value_str);
}

XError Session_impl::set_mysql_option(const Mysqlx_option option,
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
    case Mysqlx_option::Ssl_fips_mode:
      return setup_ssl_fips_mode_from_text(value);
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
      return details::translate_texts_into_auth_types({value},
                                                      &m_use_auth_methods);
    default:
      return XError{CR_X_UNSUPPORTED_OPTION, ER_TEXT_OPTION_NOT_SUPPORTED};
  }

  return {};
}

XError Session_impl::set_mysql_option(
    const Mysqlx_option option, const std::vector<std::string> &values_list) {
  if (is_connected())
    return XError{CR_ALREADY_CONNECTED,
                  ER_TEXT_OPTION_NOT_SUPPORTED_AFTER_CONNECTING};

  switch (option) {
    case Mysqlx_option::Authentication_method:
      return details::translate_texts_into_auth_types(values_list,
                                                      &m_use_auth_methods);
    default:
      return XError{CR_X_UNSUPPORTED_OPTION, ER_TEXT_OPTION_NOT_SUPPORTED};
  }
}

XError Session_impl::set_mysql_option(const Mysqlx_option option,
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

    case Mysqlx_option::Datetime_length_discriminator:
      m_context->m_datetime_length_discriminator = value;
      break;

    default:
      return XError{CR_X_UNSUPPORTED_OPTION, ER_TEXT_OPTION_NOT_SUPPORTED};
  }

  return {};
}

XError Session_impl::set_capability(const Mysqlx_capability capability,
                                    const bool value) {
  auto capability_type = details::get_capability_type(capability);

  if (details::Capability_datatype::Bool != capability_type.second)
    return XError{CR_X_UNSUPPORTED_CAPABILITY_VALUE,
                  ER_TEXT_CAPABILITY_NOT_SUPPORTED};

  m_capabilities[capability_type.first] = value;

  return XError();
}

XError Session_impl::set_capability(const Mysqlx_capability capability,
                                    const std::string &value) {
  auto capability_type = details::get_capability_type(capability);

  if (details::Capability_datatype::String != capability_type.second)
    return XError{CR_X_UNSUPPORTED_CAPABILITY_VALUE,
                  ER_TEXT_CAPABILITY_NOT_SUPPORTED};

  m_capabilities[capability_type.first] = value;

  return {};
}

XError Session_impl::set_capability(const Mysqlx_capability capability,
                                    const char *value) {
  auto capability_type = details::get_capability_type(capability);

  if (details::Capability_datatype::String != capability_type.second)
    return XError{CR_X_UNSUPPORTED_CAPABILITY_VALUE,
                  ER_TEXT_CAPABILITY_NOT_SUPPORTED};

  m_capabilities[capability_type.first] = value;

  return {};
}

XError Session_impl::set_capability(const Mysqlx_capability capability,
                                    const int64_t value) {
  auto capability_type = details::get_capability_type(capability);

  if (details::Capability_datatype::String != capability_type.second)
    return XError{CR_X_UNSUPPORTED_CAPABILITY_VALUE,
                  ER_TEXT_CAPABILITY_NOT_SUPPORTED};

  m_capabilities[capability_type.first] = value;

  return {};
}

XError Session_impl::connect(const char *host, const uint16_t port,
                             const char *user, const char *pass,
                             const char *schema) {
  if (is_connected())
    return XError{CR_ALREADY_CONNECTED, ER_TEXT_ALREADY_CONNECTED};

  auto result = get_protocol().get_connection().connect(
      details::value_or_empty_string(host), port ? port : MYSQLX_TCP_PORT,
      m_internet_protocol);

  if (result) return result;

  auto connection_type =
      get_protocol().get_connection().state().get_connection_type();

  return authenticate(user, pass, schema, connection_type);
}

XError Session_impl::connect(const char *socket_file, const char *user,
                             const char *pass, const char *schema) {
  if (is_connected())
    return XError{CR_ALREADY_CONNECTED, ER_TEXT_ALREADY_CONNECTED};

  auto result = get_protocol().get_connection().connect_to_localhost(
      details::value_or_default_string(socket_file, MYSQLX_UNIX_ADDR));

  if (result) return result;

  auto connection_type =
      get_protocol().get_connection().state().get_connection_type();

  return authenticate(user, pass, schema, connection_type);
}

XError Session_impl::reauthenticate(const char *user, const char *pass,
                                    const char *schema) {
  if (!is_connected())
    return XError{CR_CONNECTION_ERROR, ER_TEXT_NOT_CONNECTED};

  auto error = get_protocol().send(::Mysqlx::Session::Reset());

  if (error) return error;

  error = get_protocol().recv_ok();

  if (error) return error;

  auto connection_type =
      get_protocol().get_connection().state().get_connection_type();

  return authenticate(user, pass, schema, connection_type);
}

std::unique_ptr<XQuery_result> Session_impl::execute_sql(const std::string &sql,
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
    const std::string &ns, const std::string &sql, const Arguments &arguments,
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
  auto context = m_context;

  m_protocol->add_notice_handler(
      [context](XProtocol *p MY_ATTRIBUTE((unused)),
                const bool is_global MY_ATTRIBUTE((unused)),
                const Mysqlx::Notice::Frame::Type type MY_ATTRIBUTE((unused)),
                const char *payload MY_ATTRIBUTE((unused)),
                const uint32_t payload_size MY_ATTRIBUTE(
                    (unused))) -> Handler_result {
        return context->m_consume_all_notices ? Handler_result::Consumed
                                              : Handler_result::Continue;
      },
      Handler_position::End, Handler_priority_low);
}

void Session_impl::setup_session_notices_handler() {
  auto context = m_context;

  m_protocol->add_notice_handler(
      [context](XProtocol *p MY_ATTRIBUTE((unused)),
                const bool is_global MY_ATTRIBUTE((unused)),
                const Mysqlx::Notice::Frame::Type type, const char *payload,
                const uint32_t payload_size) -> Handler_result {
        return handle_notices(context, type, payload, payload_size);
      },
      Handler_position::End, Handler_priority_high);
}

void Session_impl::setup_server_supported_features(
    const Mysqlx::Connection::Capabilities *capabilities) {
  const bool ignore_unknows_mechanisms = true;

  for (const auto &capability : capabilities->capabilities()) {
    if ("authentication.mechanisms" == capability.name()) {
      std::vector<std::string> names_of_auth_methods;
      const auto &any = capability.value();

      details::get_array_of_strings_from_any(any, &names_of_auth_methods);

      details::translate_texts_into_auth_types(names_of_auth_methods,
                                               &m_server_supported_auth_methods,
                                               ignore_unknows_mechanisms);
    }
  }
}

bool Session_impl::is_connected() {
  if (!m_protocol) return false;

  return m_protocol->get_connection().state().is_connected();
}

XError Session_impl::authenticate(const char *user, const char *pass,
                                  const char *schema,
                                  Connection_type connection_type) {
  auto &protocol = get_protocol();
  auto &connection = protocol.get_connection();

  if (!m_capabilities.empty()) {
    Capabilities_builder builder;

    auto capabilities_set =
        builder.add_capabilities_from_object(m_capabilities).get_result();
    auto error = protocol.execute_set_capability(capabilities_set);

    if (error) return error;
  }

  if (!connection.state().is_ssl_activated()) {
    if (m_context->m_ssl_config.does_mode_requires_ca() &&
        !m_context->m_ssl_config.is_ca_configured())
      return XError{CR_X_TLS_WRONG_CONFIGURATION, ER_TEXT_CA_IS_REQUIRED};

    if (connection.state().is_ssl_configured()) {
      Capabilities_builder builder;
      auto capability_set_tls =
          builder.add_capability("tls", Argument_value{true}).get_result();
      auto error = protocol.execute_set_capability(capability_set_tls);

      if (!error) error = connection.activate_tls();

      if (error) {
        if (ER_X_CAPABILITIES_PREPARE_FAILED != error.error() ||
            m_context->m_ssl_config.m_mode != Ssl_config::Mode::Ssl_preferred) {
          return error;
        }
      }
    }
  }

  if (needs_servers_capabilities()) {
    XError out_error;
    const auto capabilities = protocol.execute_fetch_capabilities(&out_error);

    if (out_error) return out_error;

    setup_server_supported_features(capabilities.get());
  }

  const auto is_secure_connection =
      connection.state().is_ssl_activated() ||
      (connection_type == Connection_type::Unix_socket);
  const auto &optional_auth_methods = validate_and_adjust_auth_methods(
      m_use_auth_methods, is_secure_connection);
  const auto &error = optional_auth_methods.first;
  if (error) return error;

  XError auth_error;
  for (const auto &auth_method : optional_auth_methods.second) {
    const bool is_last = &auth_method == &optional_auth_methods.second.back();
    if (auth_method == "PLAIN" && !is_secure_connection) {
      // If this is not the last authentication mechanism then do not report
      // error but try those other methods instead.
      if (is_last) {
        return XError{
            CR_X_INVALID_AUTH_METHOD,
            "Invalid authentication method: PLAIN over unsecure channel"};
      }
    } else {
      auth_error = protocol.execute_authenticate(
          details::value_or_empty_string(user),
          details::value_or_empty_string(pass),
          details::value_or_empty_string(schema), auth_method);
    }

    // Authentication successful, otherwise try to use different auth method
    if (!auth_error) return {};
  }

  return auth_error;
}

std::vector<Session_impl::Auth> Session_impl::get_methods_sequence_from_auto(
    const Auth auto_authentication, const bool can_use_plain) {
  // Check all automatic methods and return possible sequences for them
  // This means that the corresponding auth sequences will be used:
  //   FALLBACK - MySQL 5.7 compatible automatic method:
  //     PLAIN if SSL is enabled, MYSQL41 otherwise,
  //   AUTO - MySQL 8.0 and above:
  //     sequence of SHA256_MEMORY -> (optional) PLAIN -> MYSQL41

  // Sequence like PLAIN, SHA256 or PLAIN, MYSQL41 will always fail
  // in case when PLAIN is going to fail still it may be used in future.
  const Auth plain_or_mysql41 = can_use_plain ? Auth::Plain : Auth::Mysql41;

  switch (auto_authentication) {
    case Auth::Auto_fallback:
      return {plain_or_mysql41, Auth::Sha256_memory};

    case Auth::Auto_from_capabilities:  // fall-through
    case Auth::Auto:
      if (can_use_plain)
        return {Auth::Sha256_memory, Auth::Plain, Auth::Mysql41};
      return {Auth::Sha256_memory, Auth::Mysql41};

    default:
      return {};
  }
}

bool Session_impl::is_auto_method(const Auth auto_authentication) {
  switch (auto_authentication) {
    case Auth::Auto:                    // fall-through
    case Auth::Auto_fallback:           // fall-through
    case Auth::Auto_from_capabilities:  // fall-through
      return true;

    default:
      return false;
  }
}

std::pair<XError, std::vector<std::string>>
Session_impl::validate_and_adjust_auth_methods(std::vector<Auth> auth_methods,
                                               const bool can_use_plain) {
  const auto auth_methods_count = auth_methods.size();
  const Auth first_method =
      auth_methods_count == 0 ? Auth::Auto : auth_methods[0];

  const auto auto_sequence =
      get_methods_sequence_from_auto(first_method, can_use_plain);
  if (!auto_sequence.empty()) {
    auth_methods.assign(auto_sequence.begin(), auto_sequence.end());
  } else {
    if (std::any_of(std::begin(auth_methods), std::end(auth_methods),
                    is_auto_method))
      return {XError{CR_X_INVALID_AUTH_METHOD,
                     ER_TEXT_INVALID_AUTHENTICATION_CONFIGURED},
              {}};
  }

  std::vector<std::string> auth_method_string_list;

  for (const auto auth_method : auth_methods) {
    if (0 < m_server_supported_auth_methods.count(auth_method))
      auth_method_string_list.push_back(get_method_from_auth(auth_method));
  }

  if (auth_method_string_list.empty()) {
    return {XError{CR_X_INVALID_AUTH_METHOD,
                   "Server doesn't support clients authentication methods"},
            {}};
  }

  return {{}, auth_method_string_list};
}

Handler_result Session_impl::handle_notices(
    std::shared_ptr<Context> context, const Mysqlx::Notice::Frame::Type type,
    const char *payload, const uint32_t payload_size) {
  if (Mysqlx::Notice::Frame_Type_SESSION_STATE_CHANGED == type) {
    Mysqlx::Notice::SessionStateChanged session_changed;

    if (session_changed.ParseFromArray(payload, payload_size) &&
        session_changed.IsInitialized() && session_changed.value_size() == 1) {
      if (Mysqlx::Notice::SessionStateChanged::CLIENT_ID_ASSIGNED ==
          session_changed.param()) {
        return details::scalar_get_v_uint(session_changed.value(0),
                                          &context->m_client_id)
                   ? Handler_result::Consumed
                   : Handler_result::Error;
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

  const std::map<std::string, Ssl_config::Mode> modes{
      {"PREFERRED", Ssl_config::Mode::Ssl_preferred},
      {"DISABLED", Ssl_config::Mode::Ssl_disabled},
      {"REQUIRED", Ssl_config::Mode::Ssl_required},
      {"VERIFY_CA", Ssl_config::Mode::Ssl_verify_ca},
      {"VERIFY_IDENTITY", Ssl_config::Mode::Ssl_verify_identity}};

  auto mode_value = modes.find(mode_text);

  if (modes.end() == mode_value)
    return XError{CR_X_UNSUPPORTED_OPTION_VALUE, ER_TEXT_INVALID_SSL_MODE};

  m_context->m_ssl_config.m_mode = mode_value->second;

  return {};
}

XError Session_impl::setup_ssl_fips_mode_from_text(const std::string &value) {
  if (value == "") {
    m_context->m_ssl_config.m_ssl_fips_mode =
        Ssl_config::Mode_ssl_fips::Ssl_fips_mode_off;
    return {};
  }

  const std::size_t mode_text_max_lenght = 20;
  std::string mode_text;

  mode_text.reserve(mode_text_max_lenght);
  for (const auto c : value) {
    mode_text.push_back(toupper(c));
  }

  static std::map<std::string, Ssl_config::Mode_ssl_fips> modes{
      {"OFF", Ssl_config::Mode_ssl_fips::Ssl_fips_mode_off},
      {"ON", Ssl_config::Mode_ssl_fips::Ssl_fips_mode_on},
      {"STRICT", Ssl_config::Mode_ssl_fips::Ssl_fips_mode_strict}};

  auto mode_value = modes.find(mode_text);

  if (modes.end() == mode_value)
    return XError{CR_X_UNSUPPORTED_OPTION_VALUE, ER_TEXT_INVALID_SSL_FIPS_MODE};

  m_context->m_ssl_config.m_ssl_fips_mode = mode_value->second;

  return {};
}

XError Session_impl::setup_ip_mode_from_text(const std::string &value) {
  std::string mode_text;

  for (const auto c : value) {
    mode_text.push_back(toupper(c));
  }

  static std::map<std::string, Internet_protocol> modes{
      {"ANY", Internet_protocol::Any},
      {"IP4", Internet_protocol::V4},
      {"IP6", Internet_protocol::V6}};

  auto mode_value = modes.find(mode_text);

  if (modes.end() == mode_value)
    return XError{CR_X_UNSUPPORTED_OPTION_VALUE, ER_TEXT_INVALID_IP_MODE};

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
    case Auth::Auto_from_capabilities:
      return "FROM_CAPABILITIES";
    case Auth::Auto_fallback:
      return "FALLBACK";
    case Auth::Plain:
      return "PLAIN";
    default:
      return "UNKNOWN";
  }
}

bool Session_impl::needs_servers_capabilities() const {
  return m_use_auth_methods.size() == 1 &&
         m_use_auth_methods[0] == Auth::Auto_from_capabilities;
}

static void initialize_xmessages() {
  /* Workaround for initialization of protobuf data.
     Call default_instance for first msg from every
     protobuf file.

     This should have be changed to a proper fix.
   */
  Mysqlx::ServerMessages::default_instance();
  Mysqlx::Sql::StmtExecute::default_instance();
  Mysqlx::Session::AuthenticateStart::default_instance();
  Mysqlx::Resultset::ColumnMetaData::default_instance();
  Mysqlx::Notice::Warning::default_instance();
  Mysqlx::Expr::Expr::default_instance();
  Mysqlx::Expect::Open::default_instance();
  Mysqlx::Datatypes::Any::default_instance();
  Mysqlx::Crud::Update::default_instance();
  Mysqlx::Connection::Capabilities::default_instance();
}

std::unique_ptr<XSession> create_session(const char *socket_file,
                                         const char *user, const char *pass,
                                         const char *schema,
                                         XError *out_error) {
  initialize_xmessages();

  auto result = create_session();
  auto error = result->connect(socket_file, user, pass, schema);

  if (error) {
    if (nullptr != out_error) *out_error = error;
    result.reset();
  }

  return result;
}

std::unique_ptr<XSession> create_session(const char *host, const uint16_t port,
                                         const char *user, const char *pass,
                                         const char *schema,
                                         XError *out_error) {
  initialize_xmessages();

  auto result = create_session();
  auto error = result->connect(host, port, user, pass, schema);

  if (error) {
    if (nullptr != out_error) *out_error = error;
    result.reset();
  }

  return result;
}

std::unique_ptr<XSession> create_session() {
  initialize_xmessages();

  std::unique_ptr<XSession> result{new Session_impl()};

  return result;
}

}  // namespace xcl
