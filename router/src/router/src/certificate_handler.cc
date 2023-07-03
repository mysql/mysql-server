/*
  Copyright (c) 2020, 2022, Oracle and/or its affiliates.

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

#include "certificate_handler.h"

#include "mysql/harness/net_ts/impl/file.h"
#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/stdx/io/file_handle.h"

bool CertificateHandler::no_cert_files_exists() const {
  return !ca_key_path_.exists() && !ca_cert_path_.exists() &&
         !router_key_path_.exists() && !router_cert_path_.exists();
}

bool CertificateHandler::router_cert_files_exists() const {
  return router_key_path_.exists() && router_cert_path_.exists();
}

stdx::expected<void, std::error_code> CertificateHandler::create() {
  const auto ca_pkey = cert_gen_.generate_evp_pkey();
  {
    if (!ca_pkey) {
      return stdx::make_unexpected(ca_pkey.error());
    }
    auto ca_key_file_res = stdx::io::file_handle::file(
        {}, ca_key_path_.str(), stdx::io::mode::write,
        stdx::io::creation::only_if_not_exist);
    if (!ca_key_file_res) {
      return stdx::make_unexpected(ca_key_file_res.error());
    }
    const auto ca_key_string = cert_gen_.pkey_to_string(ca_pkey->get());
    const auto ca_key_write_res =
        ca_key_file_res->write(ca_key_string.data(), ca_key_string.length());
    if (!ca_key_write_res)
      return stdx::make_unexpected(ca_key_write_res.error());
  }

  const auto ca_cert =
      cert_gen_.generate_x509(ca_pkey->get(), k_CA_CN, 1, nullptr, nullptr);
  {
    if (!ca_cert) {
      return stdx::make_unexpected(ca_cert.error());
    }
    auto ca_cert_file_res = stdx::io::file_handle::file(
        {}, ca_cert_path_.str(), stdx::io::mode::write,
        stdx::io::creation::only_if_not_exist);
    if (!ca_cert_file_res) {
      return stdx::make_unexpected(ca_cert_file_res.error());
    }
    const auto ca_cert_string = cert_gen_.cert_to_string(ca_cert->get());
    const auto ca_cert_write_res =
        ca_cert_file_res->write(ca_cert_string.data(), ca_cert_string.length());
    if (!ca_cert_write_res)
      return stdx::make_unexpected(ca_cert_write_res.error());
  }

  const auto router_pkey = cert_gen_.generate_evp_pkey();
  {
    if (!router_pkey) return stdx::make_unexpected(router_pkey.error());

    auto router_key_file_res = stdx::io::file_handle::file(
        {}, router_key_path_.str(), stdx::io::mode::write,
        stdx::io::creation::only_if_not_exist);
    if (!router_key_file_res) {
      return stdx::make_unexpected(router_key_file_res.error());
    }
    const auto router_key_string = cert_gen_.pkey_to_string(router_pkey->get());
    const auto router_key_write_res = router_key_file_res->write(
        router_key_string.data(), router_key_string.length());
    if (!router_key_write_res)
      return stdx::make_unexpected(router_key_write_res.error());
  }

  {
    const auto router_cert = cert_gen_.generate_x509(
        router_pkey->get(), k_router_CN, 2, ca_cert->get(), ca_pkey->get());
    if (!router_cert) {
      return stdx::make_unexpected(router_cert.error());
    }
    auto router_cert_file_res = stdx::io::file_handle::file(
        {}, router_cert_path_.str(), stdx::io::mode::write,
        stdx::io::creation::only_if_not_exist);
    if (!router_cert_file_res) {
      return stdx::make_unexpected(router_cert_file_res.error());
    }
    const auto router_cert_string =
        cert_gen_.cert_to_string(router_cert->get());
    const auto router_cert_write_res = router_cert_file_res->write(
        router_cert_string.data(), router_cert_string.length());
    if (!router_cert_write_res)
      return stdx::make_unexpected(router_cert_write_res.error());
  }

  return {};
}
