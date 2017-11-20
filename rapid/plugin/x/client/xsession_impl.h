/*
 * Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.
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

// MySQL DB access module, for use by plugins and others
// For the module that implements interactive DB functionality see mod_db

#ifndef X_CLIENT_XSESSION_IMPL_H_
#define X_CLIENT_XSESSION_IMPL_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "mysqlxclient/xargument.h"
#include "mysqlxclient/xsession.h"
#include "xcontext.h"


namespace xcl {

class Result;
class Protocol_impl;
class Protocol_factory;

class Session_impl : public XSession {
 public:
  enum class Auth {
    Auto,
    Mysql41,
    Plain
  };

 public:
  explicit Session_impl(std::unique_ptr<Protocol_factory> factory = {});
  ~Session_impl() override;

  XProtocol::Client_id client_id() const override { return m_context->m_client_id; }
  XProtocol &get_protocol() override;

  XError set_mysql_option(const Mysqlx_option option,
                          const bool value) override;
  XError set_mysql_option(const Mysqlx_option option,
                          const std::string &value) override;
  XError set_mysql_option(const Mysqlx_option option,
                          const char *value) override;
  XError set_mysql_option(const Mysqlx_option option,
                          const int64_t value) override;

  XError set_capability(const Mysqlx_capability capability,
                        const bool value) override;
  XError set_capability(const Mysqlx_capability capability,
                        const std::string &value) override;
  XError set_capability(const Mysqlx_capability capability,
                        const char *value) override;
  XError set_capability(const Mysqlx_capability capability,
                        const int64_t value) override;

  XError connect(const char *host,
                 const uint16_t port,
                 const char *user,
                 const char *pass,
                 const char *schema) override;

  XError connect(const char *socket_file,
                 const char *user,
                 const char *pass,
                 const char *schema) override;

  XError reauthenticate(const char *user,
                        const char *pass,
                        const char *schema) override;

  std::unique_ptr<XQuery_result> execute_sql(
      const std::string &sql,
      XError *out_error) override;

  std::unique_ptr<XQuery_result> execute_stmt(
      const std::string &ns,
      const std::string &sql,
      const Arguments &args,
      XError *out_error) override;

  void close();

 private:
  using Context_ptr          = std::shared_ptr<Context>;
  using Protocol_factory_ptr = std::unique_ptr<Protocol_factory>;
  using XProtocol_ptr        = std::shared_ptr<XProtocol>;


  void   setup_protocol();
  void   setup_session_notices_handler();
  void   setup_general_notices_handler();
  XError setup_authentication_method_from_text(const std::string &value);
  XError setup_ssl_mode_from_text(const std::string &value);
  XError setup_ip_mode_from_text(const std::string &value);

  std::string get_method_from_auth(const Auth auth,
                                   const std::string &auth_auto);

  bool is_connected();
  XError authenticate(const char *user,
                      const char *pass,
                      const char *schema);
  static Handler_result handle_notices(
        std::shared_ptr<Context> context,
        const Mysqlx::Notice::Frame::Type,
        const char *,
        const uint32_t);

  Object                m_capabilities;
  XProtocol_ptr         m_protocol;
  Context_ptr           m_context;
  Protocol_factory_ptr  m_factory;
  Internet_protocol     m_internet_protocol { Internet_protocol::Any };
  Auth                  m_auth { Auth::Auto };
};

}  // namespace xcl

#endif  // X_CLIENT_XSESSION_IMPL_H_
