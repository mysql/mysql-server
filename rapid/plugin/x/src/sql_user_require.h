/*
 * Copyright (c) 2015, 2016 Oracle and/or its affiliates. All rights reserved.
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

#ifndef _SQL_USER_REQUIRE_H_
#define _SQL_USER_REQUIRE_H_

#include "sql_data_context.h"

namespace ngs
{

class IOptions_session;
typedef ngs::shared_ptr<IOptions_session> IOptions_session_ptr;

}  // namespace ngs

namespace xpl
{

  class Sql_user_require
  {
  public:
    std::string ssl_type;
    std::string ssl_cipher;
    std::string ssl_x509_issuer;
    std::string ssl_x509_subject;

    ngs::Error_code validate(ngs::IOptions_session_ptr &options) const;

  private:
    const static std::string SSL_TYPE_NONE;
    const static std::string SSL_TYPE_SSL;
    const static std::string SSL_TYPE_X509;
    const static std::string SSL_TYPE_SPECIFIC;

    ngs::Error_code check_ssl(ngs::IOptions_session_ptr &options) const;
    ngs::Error_code check_x509(ngs::IOptions_session_ptr &options) const;
    ngs::Error_code check_specific(ngs::IOptions_session_ptr &options) const;
  };

} // namespace xpl

#endif // _SQL_USER_REQUIRE_H_