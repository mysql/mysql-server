/*
  Copyright (c) 2020, 2023, Oracle and/or its affiliates.

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

#ifndef ROUTER_CERTIFICATE_HANDLER_INCLUDED
#define ROUTER_CERTIFICATE_HANDLER_INCLUDED

#include "mysqlrouter/router_export.h"

#include "certificate_generator.h"
#include "mysql/harness/filesystem.h"

class ROUTER_LIB_EXPORT CertificateHandler {
 public:
  /**
   * Handle X.509 Router and CA keys and certificates.
   *
   * @param[in] ca_key_path Path to CA key file.
   * @param[in] ca_cert_path Path to CA certificate file.
   * @param[in] router_key_path Path to Router key file.
   * @param[in] router_cert_path Path to Router certificate file.
   */
  CertificateHandler(mysql_harness::Path ca_key_path,
                     mysql_harness::Path ca_cert_path,
                     mysql_harness::Path router_key_path,
                     mysql_harness::Path router_cert_path)
      : ca_key_path_{std::move(ca_key_path)},
        ca_cert_path_{std::move(ca_cert_path)},
        router_key_path_{std::move(router_key_path)},
        router_cert_path_{std::move(router_cert_path)} {}

  /**
   * Check if none of the Router and CA key/certificate files exists.
   *
   * @retval true No certificate file exists.
   * @retval false Some certificate files exists.
   */
  bool no_cert_files_exists() const;

  /**
   * Check if Router key and certificate files exists.
   *
   * @retval true Both Router key and certificate files exists.
   * @retval false Router certificate and/or key file is missing.
   */
  bool router_cert_files_exists() const;

  /**
   * Create Router and CA key and certificate files at configured paths.
   *
   * @return std::error_code on failure
   */
  stdx::expected<void, std::error_code> create();

 private:
  CertificateGenerator cert_gen_;
  mysql_harness::Path ca_key_path_;
  mysql_harness::Path ca_cert_path_;
  mysql_harness::Path router_key_path_;
  mysql_harness::Path router_cert_path_;
  const std::string k_CA_CN{"MySQL_Router_Auto_Generated_CA_Certificate"};
  const std::string k_router_CN{
      "MySQL_Router_Auto_Generated_Router_Certificate"};
};

#endif  // ROUTER_CERTIFICATE_HANDLER_INCLUDED
