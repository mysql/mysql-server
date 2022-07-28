/*
  Copyright (c) 2018, 2022, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is also distributed with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have included with MySQL.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef MYSQL_ROUTING_DESTINATION_TLS_CONTEXT_INCLUDED
#define MYSQL_ROUTING_DESTINATION_TLS_CONTEXT_INCLUDED

#include "mysqlrouter/routing_export.h"

#include <map>
#include <mutex>
#include <string>

#include "mysql/harness/tls_client_context.h"
#include "ssl_mode.h"  // SslVerify

/**
 * TlsClientContext per destination.
 */
class ROUTING_EXPORT DestinationTlsContext {
 public:
  /**
   * set SslVerify.
   */
  void verify(SslVerify ssl_verify);

  /**
   * set CA file.
   */
  void ca_file(const std::string &file);

  /**
   * set CA path.
   */
  void ca_path(const std::string &path);

  /**
   * set CRL file.
   */
  void crl_file(const std::string &file);

  /**
   * set CRL path.
   */
  void crl_path(const std::string &path);

  /**
   * set allowed EC curves.
   */
  void curves(const std::string &curves);

  /**
   * set allowed ciphers.
   */
  void ciphers(const std::string &ciphers);

  /**
   * get a TlsClientContent for a destination.
   *
   * If no TlsClientContext exists for the destination, creates a
   * TlsClientContent based on:
   *
   * - verify()
   * - ca_file()
   * - ca_path()
   * - crl_file()
   * - crl_path()
   * - curves()
   * - ciphers()
   *
   * If that succeeds, it the resulting TlsClientContext is cached and a pointer
   * to it is returned.
   *
   * If a TlsClientContext for the destination exists, a pointer to it is
   * returned.
   *
   * @param dest_id identified of a destination
   */
  TlsClientContext *get(const std::string &dest_id);

 private:
  SslVerify ssl_verify_{SslVerify::kDisabled};
  std::string ca_file_;
  std::string ca_path_;
  std::string crl_file_;
  std::string crl_path_;
  std::string curves_;
  std::string ciphers_;

  std::map<std::string, std::unique_ptr<TlsClientContext>> tls_contexts_;

  std::mutex mtx_;
};

#endif
