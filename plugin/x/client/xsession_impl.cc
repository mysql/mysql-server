/*
 * Copyright (c) 2015, 2023, Oracle and/or its affiliates.
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
#include <chrono>  // NOLINT(build/c++11)
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>

#include "errmsg.h"         // NOLINT(build/include_subdir)
#include "my_compiler.h"    // NOLINT(build/include_subdir)
#include "my_config.h"      // NOLINT(build/include_subdir)
#include "my_dbug.h"        // NOLINT(build/include_subdir)
#include "my_macros.h"      // NOLINT(build/include_subdir)
#include "mysql_version.h"  // NOLINT(build/include_subdir)
#include "mysqld_error.h"   // NOLINT(build/include_subdir)

#include "plugin/x/client/mysqlxclient/xerror.h"
#include "plugin/x/client/validator/descriptor.h"
#include "plugin/x/client/validator/option_compression_validator.h"
#include "plugin/x/client/validator/option_connection_validator.h"
#include "plugin/x/client/validator/option_context_validator.h"
#include "plugin/x/client/validator/option_ssl_validator.h"
#include "plugin/x/client/visitor/any_filler.h"
#include "plugin/x/client/xcapability_builder.h"
#include "plugin/x/client/xconnection_impl.h"
#include "plugin/x/client/xprotocol_factory.h"
#include "plugin/x/client/xprotocol_impl.h"
#include "plugin/x/client/xquery_result_impl.h"
#include "plugin/x/generated/mysqlx_error.h"
#include "plugin/x/generated/mysqlx_version.h"

namespace xcl {

const char *const ER_TEXT_CAPABILITY_NOT_SUPPORTED = "Capability not supported";
const char *const ER_TEXT_CAPABILITY_VALUE_INVALID =
    "Invalid value for capability";
const char *const ER_TEXT_OPTION_NOT_SUPPORTED = "Option not supported";
const char *const ER_TEXT_OPTION_VALUE_INVALID = "Invalid value for option";
const char *const ER_TEXT_OPTION_NOT_SUPPORTED_AFTER_CONNECTING =
    "Operation not supported after connecting";
const char *const ER_TEXT_NOT_CONNECTED = "Not connected";
const char *const ER_TEXT_ALREADY_CONNECTED = "Already connected";
const char *const ER_TEXT_CA_IS_REQUIRED =
    "TLS was marked that requires \"CA\", but it was not configured";
const char *const ER_TEXT_INVALID_AUTHENTICATION_CONFIGURED =
    "Ambiguous authentication methods given";

namespace details {

/** Check error code, if its client side error */
bool is_client_error(const XError &e) {
  const auto error_code = e.error();

  return (CR_X_ERROR_FIRST <= error_code && CR_X_ERROR_LAST >= error_code) ||
         (CR_ERROR_FIRST <= error_code && CR_ERROR_LAST >= error_code);
}

/**
  This class implemented the default behavior of the factory.
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

std::string to_upper(const std::string &value) {
  std::string result;

  result.reserve(value.length() + 1);
  for (const auto c : value) {
    result.push_back(toupper(c));
  }

  return result;
}

std::string to_lower(const std::string &value) {
  std::string result;

  result.reserve(value.length() + 1);
  for (const auto c : value) {
    result.push_back(tolower(c));
  }

  return result;
}

class Capability_descriptor : public Descriptor {
 public:
  Capability_descriptor() = default;
  Capability_descriptor(const std::string name, Validator *validator)
      : Descriptor(validator), m_name(name) {}

  std::string get_name() const { return m_name; }

  XError get_supported_error() const override {
    return XError{CR_X_UNSUPPORTED_CAPABILITY_VALUE,
                  ER_TEXT_CAPABILITY_NOT_SUPPORTED};
  }

  XError get_wrong_value_error(const Argument_value &) const override {
    return XError{CR_X_UNSUPPORTED_CAPABILITY_VALUE,
                  ER_TEXT_CAPABILITY_VALUE_INVALID};
  }

 private:
  const std::string m_name;
};

Capability_descriptor get_capability_descriptor(
    const XSession::Mysqlx_capability capability) {
  switch (capability) {
    case XSession::Capability_can_handle_expired_password:
      return {"client.pwd_expire_ok", new Bool_validator()};

    case XSession::Capability_client_interactive:
      return {"client.interactive", new Bool_validator()};

    case XSession::Capability_session_connect_attrs:
      return {"session_connect_attrs", new Object_validator()};

    default:
      return {};
  }
}

class Option_descriptor : public Descriptor {
 public:
  using Descriptor::Descriptor;

  XError get_supported_error() const override {
    return XError{CR_X_UNSUPPORTED_OPTION, ER_TEXT_OPTION_NOT_SUPPORTED};
  }

  XError get_wrong_value_error(const Argument_value &) const override {
    return XError{CR_X_UNSUPPORTED_OPTION_VALUE, ER_TEXT_OPTION_VALUE_INVALID};
  }
};

Option_descriptor get_option_descriptor(const XSession::Mysqlx_option option) {
  using Mysqlx_option = XSession::Mysqlx_option;
  using Con_conf = Connection_config;
  using Ctxt = Context;

  switch (option) {
    case Mysqlx_option::Hostname_resolve_to:
      return Option_descriptor{new Contex_ip_validator()};

    case Mysqlx_option::Connect_timeout:
      return Option_descriptor{
          new Con_int_store<&Con_conf::m_timeout_connect>()};

    case Mysqlx_option::Session_connect_timeout:
      return Option_descriptor{
          new Con_int_store<&Con_conf::m_timeout_session_connect>()};

    case Mysqlx_option::Read_timeout:
      return Option_descriptor{new Con_int_store<&Con_conf::m_timeout_read>()};

    case Mysqlx_option::Write_timeout:
      return Option_descriptor{new Con_int_store<&Con_conf::m_timeout_write>()};

    case Mysqlx_option::Allowed_tls:
      return Option_descriptor{new Ssl_str_store<&Ssl_config::m_tls_version>()};

    case Mysqlx_option::Ssl_mode:
      return Option_descriptor{new Ssl_mode_validator()};

    case Mysqlx_option::Ssl_fips_mode:
      return Option_descriptor{new Ssl_fips_validator()};

    case Mysqlx_option::Ssl_key:
      return Option_descriptor{new Ssl_str_store<&Ssl_config::m_key>()};

    case Mysqlx_option::Ssl_ca:
      return Option_descriptor{new Ssl_str_store<&Ssl_config::m_ca>()};

    case Mysqlx_option::Ssl_ca_path:
      return Option_descriptor{new Ssl_str_store<&Ssl_config::m_ca_path>()};

    case Mysqlx_option::Ssl_cert:
      return Option_descriptor{new Ssl_str_store<&Ssl_config::m_cert>()};

    case Mysqlx_option::Ssl_cipher:
      return Option_descriptor{new Ssl_str_store<&Ssl_config::m_cipher>()};

    case Mysqlx_option::Ssl_crl:
      return Option_descriptor{new Ssl_str_store<&Ssl_config::m_crl>()};

    case Mysqlx_option::Ssl_crl_path:
      return Option_descriptor{new Ssl_str_store<&Ssl_config::m_crl_path>()};

    case Mysqlx_option::Authentication_method:
      return Option_descriptor{new Contex_auth_validator()};

    case Mysqlx_option::Consume_all_notices:
      return Option_descriptor{
          new Ctxt_bool_store<&Ctxt::m_consume_all_notices>()};

    case Mysqlx_option::Datetime_length_discriminator:
      return Option_descriptor{
          new Ctxt_uint32_store<&Ctxt::m_datetime_length_discriminator>()};

    case Mysqlx_option::Network_namespace:
      return Option_descriptor{
          new Con_str_store<&Con_conf::m_network_namespace>()};

    case Mysqlx_option::Compression_negotiation_mode:
      return Option_descriptor{new Compression_negotiation_validator()};

    case Mysqlx_option::Compression_algorithms:
      return Option_descriptor{new Compression_algorithms_validator()};

    case Mysqlx_option::Compression_combine_mixed_messages:
      return Option_descriptor{new Compression_bool_store<
          &Compression_config::m_use_server_combine_mixed_messages>()};

    case Mysqlx_option::Compression_max_combine_messages:
      return Option_descriptor{new Compression_int_store<
          &Compression_config::m_use_server_max_combine_messages>()};

    case Mysqlx_option::Compression_level_client:
      return Option_descriptor{new Compression_optional_int_store<
          &Compression_config::m_use_level_client>()};

    case Mysqlx_option::Compression_level_server:
      return Option_descriptor{new Compression_optional_int_store<
          &Compression_config::m_use_level_server>()};

    case Mysqlx_option::Buffer_recevie_size:
      return Option_descriptor{
          new Con_int_store<&Con_conf::m_buffer_receive_size>()};

    default:
      return {};
  }
}

void translate_texts_into_auth_types(
    const std::vector<std::string> &values_list,
    std::set<Auth> *out_auths_list) {
  static const std::map<std::string, Auth> modes{
      {"MYSQL41", Auth::k_mysql41},
      {"PLAIN", Auth::k_plain},
      {"SHA256_MEMORY", Auth::k_sha256_memory}};

  out_auths_list->clear();
  for (const auto &mode_text : values_list) {
    auto mode_value = modes.find(details::to_upper(mode_text));

    if (modes.end() == mode_value) continue;

    out_auths_list->insert(out_auths_list->end(), mode_value->second);
  }
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

class Notice_server_hello_ignore {
 public:
  explicit Notice_server_hello_ignore(XProtocol *protocol)
      : m_protocol(protocol) {
    m_handler_id = m_protocol->add_notice_handler(
        *this, Handler_position::Begin, Handler_priority_low);
  }

  ~Notice_server_hello_ignore() {
    if (XCL_HANDLER_ID_NOT_VALID != m_handler_id) {
      m_protocol->remove_notice_handler(m_handler_id);
    }
  }

  Notice_server_hello_ignore(const Notice_server_hello_ignore &) = default;

  Handler_result operator()(XProtocol *, const bool is_global,
                            const Mysqlx::Notice::Frame::Type type,
                            const char *, const uint32_t) {
    const bool is_hello_notice =
        Mysqlx::Notice::Frame_Type_SERVER_HELLO == type;

    if (!is_global) return Handler_result::Continue;
    if (!is_hello_notice) return Handler_result::Continue;

    if (m_already_received) return Handler_result::Error;

    m_already_received = true;

    return Handler_result::Consumed;
  }

  bool m_already_received = false;
  XProtocol::Handler_id m_handler_id = XCL_HANDLER_ID_NOT_VALID;
  XProtocol *m_protocol;
};

template <typename Value>
XError set_object_capability(Context *context, Argument_object *capabilities,
                             const XSession::Mysqlx_capability capability,
                             const Value &value) {
  DBUG_TRACE;
  auto capability_type = details::get_capability_descriptor(capability);

  auto error = capability_type.is_valid(context, value);

  if (error) return error;

  (*capabilities)[capability_type.get_name()] = value;

  return {};
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
  if (is_connected()) get_protocol().get_connection().close();
}

XProtocol &Session_impl::get_protocol() { return *m_protocol; }

XError Session_impl::set_mysql_option(const Mysqlx_option option,
                                      const bool value) {
  DBUG_TRACE;
  if (is_connected()) {
    return XError{CR_ALREADY_CONNECTED,
                  ER_TEXT_OPTION_NOT_SUPPORTED_AFTER_CONNECTING};
  }

  auto option_type = details::get_option_descriptor(option);

  return option_type.is_valid(m_context.get(), value);
}

XError Session_impl::set_mysql_option(const Mysqlx_option option,
                                      const char *value) {
  DBUG_TRACE;
  const std::string value_str = nullptr == value ? "" : value;

  return set_mysql_option(option, value_str);
}

XError Session_impl::set_mysql_option(const Mysqlx_option option,
                                      const std::string &value) {
  DBUG_TRACE;
  if (is_connected())
    return XError{CR_ALREADY_CONNECTED,
                  ER_TEXT_OPTION_NOT_SUPPORTED_AFTER_CONNECTING};

  auto option_type = details::get_option_descriptor(option);

  return option_type.is_valid(m_context.get(), value);
}

XError Session_impl::set_mysql_option(
    const Mysqlx_option option, const std::vector<std::string> &values_list) {
  DBUG_TRACE;
  if (is_connected())
    return XError{CR_ALREADY_CONNECTED,
                  ER_TEXT_OPTION_NOT_SUPPORTED_AFTER_CONNECTING};

  Argument_array array;
  for (const auto &value : values_list) {
    array.push_back(Argument_value{value});
  }

  auto option_type = details::get_option_descriptor(option);

  return option_type.is_valid(m_context.get(), array);
}

XError Session_impl::set_mysql_option(const Mysqlx_option option,
                                      const int64_t value) {
  DBUG_TRACE;
  if (is_connected())
    return XError{CR_ALREADY_CONNECTED,
                  ER_TEXT_OPTION_NOT_SUPPORTED_AFTER_CONNECTING};

  auto option_type = details::get_option_descriptor(option);

  return option_type.is_valid(m_context.get(), value);
}

Argument_object &Session_impl::get_capabilites(const bool required) {
  DBUG_TRACE;
  if (required) return m_required_capabilities;

  DBUG_LOG("debug", "Returning optional set");
  return m_optional_capabilities;
}

XError Session_impl::set_capability(const Mysqlx_capability capability,
                                    const bool value, const bool required) {
  DBUG_TRACE;
  auto capability_type = details::get_capability_descriptor(capability);

  auto error = capability_type.is_valid(m_context.get(), value);

  if (error) return error;

  get_capabilites(required)[capability_type.get_name()] = value;

  return XError();
}

XError Session_impl::set_capability(const Mysqlx_capability capability,
                                    const std::string &value,
                                    const bool required) {
  DBUG_TRACE;
  auto capability_type = details::get_capability_descriptor(capability);

  auto error = capability_type.is_valid(m_context.get(), value);

  if (error) return error;

  get_capabilites(required)[capability_type.get_name()] = value;

  return {};
}

XError Session_impl::set_capability(const Mysqlx_capability capability,
                                    const char *value, const bool required) {
  DBUG_TRACE;
  auto capability_type = details::get_capability_descriptor(capability);

  auto error = capability_type.is_valid(m_context.get(), value);

  if (error) return error;

  get_capabilites(required)[capability_type.get_name()] =
      Argument_value{value, Argument_value::String_type::k_string};

  return {};
}

XError Session_impl::set_capability(const Mysqlx_capability capability,
                                    const int64_t value, const bool required) {
  DBUG_TRACE;
  auto capability_type = details::get_capability_descriptor(capability);

  auto error = capability_type.is_valid(m_context.get(), value);

  if (error) return error;

  get_capabilites(required)[capability_type.get_name()] = value;

  return {};
}

XError Session_impl::set_capability(const Mysqlx_capability capability,
                                    const Argument_object &value,
                                    const bool required) {
  DBUG_TRACE;

  return details::set_object_capability(
      m_context.get(), &get_capabilites(required), capability, value);
}

XError Session_impl::set_capability(const Mysqlx_capability capability,
                                    const Argument_uobject &value,
                                    const bool required) {
  DBUG_TRACE;

  return details::set_object_capability(
      m_context.get(), &get_capabilites(required), capability, value);
}

XError Session_impl::connect(const char *host, const uint16_t port,
                             const char *user, const char *pass,
                             const char *schema) {
  DBUG_TRACE;

  if (is_connected())
    return XError{CR_ALREADY_CONNECTED, ER_TEXT_ALREADY_CONNECTED};

  Session_connect_timeout_scope_guard timeout_guard{this};
  auto &connection = get_protocol().get_connection();
  const auto result = connection.connect(details::value_or_empty_string(host),
                                         port ? port : MYSQLX_TCP_PORT,
                                         m_context->m_internet_protocol);
  if (result) return result;

  get_protocol().reset_buffering();

  const auto connection_type = connection.state().get_connection_type();
  details::Notice_server_hello_ignore notice_ignore(m_protocol.get());

  return authenticate(user, pass, schema, connection_type);
}

XError Session_impl::connect(const char *socket_file, const char *user,
                             const char *pass, const char *schema) {
  DBUG_TRACE;

  if (is_connected())
    return XError{CR_ALREADY_CONNECTED, ER_TEXT_ALREADY_CONNECTED};

  Session_connect_timeout_scope_guard timeout_guard{this};
  auto &connection = get_protocol().get_connection();
  const auto result = connection.connect_to_localhost(
      details::value_or_default_string(socket_file, MYSQLX_UNIX_ADDR));

  if (result) return result;

  get_protocol().reset_buffering();

  const auto connection_type = connection.state().get_connection_type();

  details::Notice_server_hello_ignore notice_ignore(m_protocol.get());
  return authenticate(user, pass, schema, connection_type);
}

XError Session_impl::reauthenticate(const char *user, const char *pass,
                                    const char *schema) {
  DBUG_TRACE;

  if (!is_connected())
    return XError{CR_CONNECTION_ERROR, ER_TEXT_NOT_CONNECTED};

  auto error = get_protocol().send(::Mysqlx::Session::Reset());

  if (error) return error;

  Session_connect_timeout_scope_guard timeout_guard{this};
  error = get_protocol().recv_ok();

  if (error) return error;

  auto connection_type =
      get_protocol().get_connection().state().get_connection_type();

  return authenticate(user, pass, schema, connection_type);
}

std::unique_ptr<XQuery_result> Session_impl::execute_sql(const std::string &sql,
                                                         XError *out_error) {
  DBUG_TRACE;
  if (!is_connected()) {
    *out_error = XError{CR_CONNECTION_ERROR, ER_TEXT_NOT_CONNECTED};

    return {};
  }

  ::Mysqlx::Sql::StmtExecute stmt;

  stmt.set_stmt(sql);
  return m_protocol->execute_stmt(stmt, out_error);
}

std::unique_ptr<XQuery_result> Session_impl::execute_stmt(
    const std::string &ns, const std::string &sql,
    const Argument_array &arguments, XError *out_error) {
  DBUG_TRACE;
  if (!is_connected()) {
    *out_error = XError{CR_CONNECTION_ERROR, ER_TEXT_NOT_CONNECTED};

    return {};
  }

  ::Mysqlx::Sql::StmtExecute stmt;

  stmt.set_stmt(sql);
  stmt.set_namespace_(ns);

  for (const auto &argument : arguments) {
    Any_filler filler(stmt.mutable_args()->Add());

    argument.accept(&filler);
  }

  return m_protocol->execute_stmt(stmt, out_error);
}

void Session_impl::close() {
  DBUG_TRACE;
  if (is_connected()) {
    m_protocol->execute_close();

    m_protocol.reset();
  }
}

void Session_impl::setup_protocol() {
  DBUG_TRACE;
  m_protocol = m_factory->create_protocol(m_context);
  setup_session_notices_handler();
  setup_general_notices_handler();
}

void Session_impl::setup_general_notices_handler() {
  DBUG_TRACE;
  auto context = m_context;

  m_protocol->add_notice_handler(
      [context](XProtocol *p [[maybe_unused]],
                const bool is_global [[maybe_unused]],
                const Mysqlx::Notice::Frame::Type type [[maybe_unused]],
                const char *payload [[maybe_unused]],
                const uint32_t payload_size MY_ATTRIBUTE(
                    (unused))) -> Handler_result {
        return context->m_consume_all_notices ? Handler_result::Consumed
                                              : Handler_result::Continue;
      },
      Handler_position::End, Handler_priority_low);
}

void Session_impl::setup_session_notices_handler() {
  DBUG_TRACE;
  auto context = m_context;

  m_protocol->add_notice_handler(
      [context](XProtocol *p [[maybe_unused]],
                const bool is_global [[maybe_unused]],
                const Mysqlx::Notice::Frame::Type type, const char *payload,
                const uint32_t payload_size) -> Handler_result {
        return handle_notices(context, type, payload, payload_size);
      },
      Handler_position::End, Handler_priority_high);
}

void Session_impl::setup_server_supported_compression(
    const Mysqlx::Datatypes::Object_ObjectField *field) {
  std::vector<std::string> text_values;

  details::get_array_of_strings_from_any(field->value(), &text_values);
  auto &negotiator = m_context->m_compression_config.m_negotiator;

  if ("algorithm" == field->key()) {
    negotiator.server_supports_algorithms(text_values);
  }
}

void Session_impl::setup_server_supported_features(
    const Mysqlx::Connection::Capabilities *capabilities) {
  DBUG_TRACE;

  for (const auto &capability : capabilities->capabilities()) {
    if ("authentication.mechanisms" == capability.name()) {
      std::vector<std::string> names_of_auth_methods;
      const auto &any = capability.value();

      details::get_array_of_strings_from_any(any, &names_of_auth_methods);

      details::translate_texts_into_auth_types(
          names_of_auth_methods, &m_server_supported_auth_methods);
    }
    if ("compression" == capability.name()) {
      const auto &value = capability.value();
      if (value.type() == Mysqlx::Datatypes::Any_Type_OBJECT) {
        for (const auto &fld : value.obj().fld()) {
          setup_server_supported_compression(&fld);
        }
      }
    }
  }
}

bool Session_impl::is_connected() {
  DBUG_TRACE;
  if (!m_protocol) return false;

  return m_protocol->get_connection().state().is_connected();
}

XError Session_impl::authenticate(const char *user, const char *pass,
                                  const char *schema,
                                  Connection_type connection_type) {
  DBUG_TRACE;
  auto &protocol = get_protocol();
  auto &connection = protocol.get_connection();

  // After adding pipelining to mysqlxclient, all requests below should
  // be merged into single send operation, followed by a read operation/s
  if (!m_required_capabilities.empty()) {
    Capabilities_builder builder;

    auto required_capabilities_set =
        builder.clear()
            .add_capabilities_from_object(m_required_capabilities)
            .get_result();
    auto error = protocol.execute_set_capability(required_capabilities_set);

    if (error) return error;
  }

  for (const auto &capability : m_optional_capabilities) {
    Capabilities_builder builder;
    auto optional_capabilities_set =
        builder.clear()
            .add_capability(capability.first, capability.second)
            .get_result();
    const auto error =
        protocol.execute_set_capability(optional_capabilities_set);

    // Optional capabilities might fail
    if (error.is_fatal() || details::is_client_error(error)) return error;
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

    XError error;
    auto &config = m_context->m_compression_config;

    if (config.m_negotiator.update_compression_options(&config.m_use_algorithm,
                                                       &error)) {
      Capabilities_builder capability_builder;
      capability_builder.add_capability("compression",
                                        get_compression_capability());
      error = protocol.execute_set_capability(capability_builder.get_result());
      // We shouldn't fail here, server supports needed capability
      // still there is possibility that compression_level is not
      // supported by the server
      if (error && error.is_fatal()) return error;

      if (error) {
        const bool without_compression_level = false;
        capability_builder.clear();
        capability_builder.add_capability(
            "compression",
            get_compression_capability(without_compression_level));
        // We shouldn't fail here, server supports needed capability
        error =
            protocol.execute_set_capability(capability_builder.get_result());
      }
    }

    // Server doesn't support given compression configuration
    // and client didn't mark it as optional (its "required").
    if (error) return error;
  }

  if (m_context->m_compression_config.m_use_level_client.has_value())
    m_protocol->use_compression(
        m_context->m_compression_config.m_use_algorithm,
        m_context->m_compression_config.m_use_level_client.value());
  else
    m_protocol->use_compression(
        m_context->m_compression_config.m_use_algorithm);

  const auto is_secure_connection =
      connection.state().is_ssl_activated() ||
      (connection_type == Connection_type::Unix_socket);
  const auto &optional_auth_methods = validate_and_adjust_auth_methods(
      m_context->m_use_auth_methods, is_secure_connection);
  const auto &error = optional_auth_methods.first;
  if (error) return error;

  bool has_sha256_memory = false;
  bool fatal_error_received = false;
  XError reported_error;

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

      continue;
    }

    XError current_error = protocol.execute_authenticate(
        details::value_or_empty_string(user),
        details::value_or_empty_string(pass),
        details::value_or_empty_string(schema), auth_method);

    // Authentication successful, otherwise try to use different auth method
    if (!current_error) return {};

    const auto current_error_code = current_error.error();

    // In case of connection errors ('broken pipe', 'peer disconnected',
    // timeouts...) we should break the authentication sequence and return
    // an error.
    if (current_error_code == CR_SERVER_GONE_ERROR ||
        current_error_code == CR_X_WRITE_TIMEOUT ||
        current_error_code == CR_X_READ_TIMEOUT ||
        current_error_code == CR_UNKNOWN_ERROR) {
      // Expected disconnection
      if (fatal_error_received) return reported_error;

      // Unexpected disconnection
      return current_error;
    }

    // Try to choose most important error:
    //
    // |Priority |Action                        |
    // |---------|------------------------------|
    // |1        |No error was set              |
    // |2        |Last other than access denied |
    // |3        |Last access denied            |
    if (!reported_error || current_error_code != ER_ACCESS_DENIED_ERROR ||
        reported_error.error() == ER_ACCESS_DENIED_ERROR) {
      reported_error = current_error;
    }

    // Also we should stop the authentication sequence on fatal error.
    // Still it would break compatibility with servers that wrongly mark
    // Mysqlx::Error message with fatal flag.
    //
    // To workaround the problem of backward compatibility, we should
    // remember that fatal error was received and try to continue the
    // sequence. After reception of fatal error, following connection
    // errors are expected (CR_SERVER_GONE_ERROR, CR_X_WRITE_TIMEOUT....)
    // and must be ignored.
    if (current_error.is_fatal()) fatal_error_received = true;

    if (auth_method == "SHA256_MEMORY") has_sha256_memory = true;
  }

  // In case when SHA256_MEMORY was used and no PLAIN (because of not using
  // secure connection) and all errors where ER_ACCESS_DENIED, there is
  // possibility that password cache on the server side is empty.
  // We need overwrite the error to give the user a hint that
  // secure connection can be used.
  if (has_sha256_memory && !is_secure_connection &&
      reported_error.error() == ER_ACCESS_DENIED_ERROR) {
    reported_error = XError{CR_X_AUTH_PLUGIN_ERROR,
                            "Authentication failed, check username and "
                            "password or try a secure connection"};
  }

  return reported_error;
}

std::vector<Auth> Session_impl::get_methods_sequence_from_auto(
    const Auth auto_authentication, const bool can_use_plain) {
  DBUG_TRACE;
  // Check all automatic methods and return possible sequences for them
  // This means that the corresponding auth sequences will be used:
  //   FALLBACK - MySQL 5.7 compatible automatic method:
  //     PLAIN if SSL is enabled, MYSQL41 otherwise,
  //   AUTO - MySQL 8.0 and above:
  //     sequence of SHA256_MEMORY -> (optional) PLAIN -> MYSQL41

  // Sequence like PLAIN, SHA256 or PLAIN, MYSQL41 will always fail
  // in case when PLAIN is going to fail still it may be used in future.
  const Auth plain_or_mysql41 = can_use_plain ? Auth::k_plain : Auth::k_mysql41;

  switch (auto_authentication) {
    case Auth::k_auto_fallback:
      return {plain_or_mysql41, Auth::k_sha256_memory};

    case Auth::k_auto_from_capabilities:  // fall-through
    case Auth::k_auto:
      if (can_use_plain)
        return {Auth::k_sha256_memory, Auth::k_plain, Auth::k_mysql41};
      return {Auth::k_sha256_memory, Auth::k_mysql41};

    default:
      return {};
  }
}

bool Session_impl::is_auto_method(const Auth auto_authentication) {
  DBUG_TRACE;
  switch (auto_authentication) {
    case Auth::k_auto:                    // fall-through
    case Auth::k_auto_fallback:           // fall-through
    case Auth::k_auto_from_capabilities:  // fall-through
      return true;

    default:
      return false;
  }
}

std::pair<XError, std::vector<std::string>>
Session_impl::validate_and_adjust_auth_methods(
    const std::vector<Auth> &auth_methods, bool can_use_plain) {
  DBUG_TRACE;
  const Auth first_method =
      auth_methods.empty() ? Auth::k_auto : auth_methods[0];

  const auto auto_sequence =
      get_methods_sequence_from_auto(first_method, can_use_plain);
  if (auto_sequence.empty()) {
    if (std::any_of(std::begin(auth_methods), std::end(auth_methods),
                    is_auto_method))
      return {XError{CR_X_INVALID_AUTH_METHOD,
                     ER_TEXT_INVALID_AUTHENTICATION_CONFIGURED},
              {}};
  }

  std::vector<std::string> auth_method_string_list;

  for (const auto &auth_method :
       auto_sequence.empty() ? auth_methods : auto_sequence) {
    if (0 < m_server_supported_auth_methods.count(auth_method))
      auth_method_string_list.push_back(get_method_from_auth(auth_method));
  }

  if (auth_method_string_list.empty()) {
    return {XError{CR_X_INVALID_AUTH_METHOD,
                   "Server doesn't support clients authentication methods"},
            {}};
  }

  return {{}, std::move(auth_method_string_list)};
}

Handler_result Session_impl::handle_notices(
    std::shared_ptr<Context> context, const Mysqlx::Notice::Frame::Type type,
    const char *payload, const uint32_t payload_size) {
  DBUG_TRACE;

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

std::string Session_impl::get_method_from_auth(const Auth auth) {
  switch (auth) {
    case Auth::k_auto:
      return "AUTO";
    case Auth::k_mysql41:
      return "MYSQL41";
    case Auth::k_sha256_memory:
      return "SHA256_MEMORY";
    case Auth::k_auto_from_capabilities:
      return "FROM_CAPABILITIES";
    case Auth::k_auto_fallback:
      return "FALLBACK";
    case Auth::k_plain:
      return "PLAIN";
    default:
      return "UNKNOWN";
  }
}

bool Session_impl::needs_servers_capabilities() const {
  if (m_context->m_use_auth_methods.size() == 1 &&
      m_context->m_use_auth_methods[0] == Auth::k_auto_from_capabilities)
    return true;

  const auto &negotiator = m_context->m_compression_config.m_negotiator;
  if (negotiator.is_negotiation_needed()) return true;

  return false;
}

Argument_uobject Session_impl::get_connect_attrs() const {
  return {
      {"_client_name", Argument_value{HAVE_MYSQLX_FULL_PROTO(
                           "libmysqlxclient", "libmysqlxclient_lite")}},
      {"_client_version", Argument_value{PACKAGE_VERSION}},
      {"_os", Argument_value{SYSTEM_TYPE}},
      {"_platform", Argument_value{MACHINE_TYPE}},
      {"_client_license", Argument_value{STRINGIFY_ARG(LICENSE)}},
#ifdef _WIN32
      {"_pid", Argument_value{std::to_string(
                   static_cast<uint64_t>(GetCurrentProcessId()))}},
      {"_thread", Argument_value{std::to_string(
                      static_cast<uint64_t>(GetCurrentThreadId()))}},
#else
      {"_pid", Argument_value{std::to_string(static_cast<uint64_t>(getpid()))}},
#endif
  };
}

Argument_value Session_impl::get_compression_capability(
    const bool include_compression_level) const {
  static const std::map<Compression_algorithm, std::string> k_algorithm{
      {Compression_algorithm::k_deflate, "DEFLATE_STREAM"},
      {Compression_algorithm::k_lz4, "LZ4_MESSAGE"},
      {Compression_algorithm::k_zstd, "ZSTD_STREAM"}};

  Argument_object obj;
  auto &config = m_context->m_compression_config;
  obj["algorithm"] = k_algorithm.at(config.m_use_algorithm);
  obj["server_combine_mixed_messages"] =
      config.m_use_server_combine_mixed_messages;
  obj["server_max_combine_messages"] =
      static_cast<int64_t>(config.m_use_server_max_combine_messages);
  if (config.m_use_level_server.has_value() && include_compression_level)
    obj["level"] = static_cast<int64_t>(config.m_use_level_server.value());

  return Argument_value{obj};
}

Session_impl::Session_connect_timeout_scope_guard::
    Session_connect_timeout_scope_guard(Session_impl *parent)
    : m_parent{parent}, m_start_time{std::chrono::steady_clock::now()} {
  m_handler_id = m_parent->get_protocol().add_send_message_handler(
      [this](xcl::XProtocol *, const xcl::XProtocol::Client_message_type_id,
             const xcl::XProtocol::Message &) -> xcl::Handler_result {
        const auto timeout =
            m_parent->m_context->m_connection_config.m_timeout_session_connect;
        // Infinite timeout, do not set message handler
        if (timeout < 0) return Handler_result::Continue;

        auto &connection = m_parent->get_protocol().get_connection();
        const auto delta =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - m_start_time)
                .count();
        const auto new_timeout = (timeout - delta) / 1000;
        connection.set_write_timeout(
            details::make_vio_timeout((delta > timeout) ? 0 : new_timeout));
        connection.set_read_timeout(
            details::make_vio_timeout((delta > timeout) ? 0 : new_timeout));
        return Handler_result::Continue;
      });
}

Session_impl::Session_connect_timeout_scope_guard::
    ~Session_connect_timeout_scope_guard() {
  m_parent->get_protocol().remove_send_message_handler(m_handler_id);
  auto &connection = m_parent->get_protocol().get_connection();
  const auto read_timeout =
      m_parent->m_context->m_connection_config.m_timeout_read;
  connection.set_read_timeout(
      details::make_vio_timeout((read_timeout < 0) ? -1 : read_timeout / 1000));
  const auto write_timeout =
      m_parent->m_context->m_connection_config.m_timeout_write;
  connection.set_write_timeout(details::make_vio_timeout(
      (write_timeout < 0) ? -1 : write_timeout / 1000));
}

std::unique_ptr<XSession> create_session(const char *socket_file,
                                         const char *user, const char *pass,
                                         const char *schema,
                                         XError *out_error) {
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
  auto result = create_session();
  auto error = result->connect(host, port, user, pass, schema);

  if (error) {
    if (nullptr != out_error) *out_error = error;
    result.reset();
  }

  return result;
}

std::unique_ptr<XSession> create_session() {
  std::unique_ptr<XSession> result{new Session_impl()};

  return result;
}

}  // namespace xcl
