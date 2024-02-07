/*
 * Copyright (c) 2015, 2024, Oracle and/or its affiliates.
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
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

// MySQL DB access module, for use by plugins and others
// For the module that implements interactive DB functionality see mod_db

#ifndef PLUGIN_X_CLIENT_XSESSION_IMPL_H_
#define PLUGIN_X_CLIENT_XSESSION_IMPL_H_

#include <chrono>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "plugin/x/client/context/xcontext.h"
#include "plugin/x/client/mysqlxclient/xargument.h"
#include "plugin/x/client/mysqlxclient/xsession.h"

namespace xcl {

class Result;
class Protocol_impl;
class Protocol_factory;

class Session_impl : public XSession {
 public:
  explicit Session_impl(std::unique_ptr<Protocol_factory> factory = {});
  ~Session_impl() override;

  XProtocol::Client_id client_id() const override {
    return m_context->m_client_id;
  }
  XProtocol &get_protocol() override;

  XError set_mysql_option(const Mysqlx_option option,
                          const bool value) override;
  XError set_mysql_option(const Mysqlx_option option,
                          const std::string &value) override;
  XError set_mysql_option(const Mysqlx_option option,
                          const std::vector<std::string> &values_list) override;
  XError set_mysql_option(const Mysqlx_option option,
                          const char *value) override;
  XError set_mysql_option(const Mysqlx_option option,
                          const int64_t value) override;

  XError set_capability(const Mysqlx_capability capability, const bool value,
                        const bool required = true) override;
  XError set_capability(const Mysqlx_capability capability,
                        const std::string &value,
                        const bool required = true) override;
  XError set_capability(const Mysqlx_capability capability, const char *value,
                        const bool required = true) override;
  XError set_capability(const Mysqlx_capability capability, const int64_t value,
                        const bool required = true) override;
  XError set_capability(const Mysqlx_capability capability,
                        const Argument_object &value,
                        const bool required = true) override;
  XError set_capability(const Mysqlx_capability capability,
                        const Argument_uobject &value,
                        const bool required = true) override;

  XError connect(const char *host, const uint16_t port, const char *user,
                 const char *pass, const char *schema) override;

  XError connect(const char *socket_file, const char *user, const char *pass,
                 const char *schema) override;

  XError reauthenticate(const char *user, const char *pass,
                        const char *schema) override;

  std::unique_ptr<XQuery_result> execute_sql(const std::string &sql,
                                             XError *out_error) override;

  std::unique_ptr<XQuery_result> execute_stmt(const std::string &ns,
                                              const std::string &sql,
                                              const Argument_array &args,
                                              XError *out_error) override;

  void close() override;

  Argument_uobject get_connect_attrs() const override;
  bool is_protocol() const { return m_protocol.use_count(); }

 private:
  using Context_ptr = std::shared_ptr<Context>;
  using Protocol_factory_ptr = std::unique_ptr<Protocol_factory>;
  using XProtocol_ptr = std::shared_ptr<XProtocol>;

  void setup_protocol();
  void setup_session_notices_handler();
  void setup_general_notices_handler();
  void setup_server_supported_compression(
      const Mysqlx::Datatypes::Object_ObjectField *field);
  void setup_server_supported_features(
      const Mysqlx::Connection::Capabilities *capabilities);

  static std::vector<Auth> get_methods_sequence_from_auto(
      const Auth auto_authentication, const bool can_use_plain);
  static bool is_auto_method(const Auth authentication_method);
  static std::string get_method_from_auth(const Auth auth);

  Argument_object &get_capabilites(const bool required);
  bool needs_servers_capabilities() const;
  bool is_connected();
  XError authenticate(const char *user, const char *pass, const char *schema,
                      Connection_type connection_type);
  static Handler_result handle_notices(std::shared_ptr<Context> context,
                                       const Mysqlx::Notice::Frame::Type,
                                       const char *, const uint32_t);

  std::pair<XError, std::vector<std::string>> validate_and_adjust_auth_methods(
      const std::vector<Auth> &auth_methods, bool can_use_plain);
  Argument_value get_compression_capability(
      const bool include_compression_level = true) const;

  Argument_object m_required_capabilities;
  Argument_object m_optional_capabilities;
  XProtocol_ptr m_protocol;
  Context_ptr m_context;
  Protocol_factory_ptr m_factory;
  std::set<Auth> m_server_supported_auth_methods{Auth::k_mysql41, Auth::k_plain,
                                                 Auth::k_sha256_memory};

  class Session_connect_timeout_scope_guard {
   public:
    explicit Session_connect_timeout_scope_guard(Session_impl *parent);
    ~Session_connect_timeout_scope_guard();

   private:
    Session_impl *m_parent;
    XProtocol::Handler_id m_handler_id;
    std::chrono::steady_clock::time_point m_start_time;
  };
};

}  // namespace xcl

#endif  // PLUGIN_X_CLIENT_XSESSION_IMPL_H_
