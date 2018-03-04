/*
 * Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
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

#ifndef _NGS_SSL_CONTEXT_INTERFACE_H_
#define _NGS_SSL_CONTEXT_INTERFACE_H_

#include "plugin/x/ngs/include/ngs_common/options.h"

namespace ngs {

class Connection_vio;

class Ssl_context_interface {
public:
  virtual bool setup(const char* tls_version,
                     const char* ssl_key,
                     const char* ssl_ca,
                     const char* ssl_capath,
                     const char* ssl_cert,
                     const char* ssl_cipher,
                     const char* ssl_crl,
                     const char* ssl_crlpath) = 0;
  virtual bool activate_tls(Connection_vio &conn,
                            const int handshake_timeout) = 0;

  virtual IOptions_context_ptr options() = 0;
  virtual bool has_ssl() = 0;

  virtual ~Ssl_context_interface() = default;
};

} // namespace ngs

#endif // _NGS_SSL_CONTEXT_INTERFACE_H_


