/*
  Copyright (c) 2020, 2021, Oracle and/or its affiliates.

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

#include <fstream>
#include <memory>
#include <string>
#include <system_error>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "filesystem_utils.h"
#include "mysql/harness/filesystem.h"
#include "test/helpers.h"

class CertificateHandlerTest : public ::testing::Test {
 public:
  void open_cert_files() {
    std::ofstream{ca_key_path.str()};
    std::ofstream{ca_cert_path.str()};
    std::ofstream{router_key_path.str()};
    std::ofstream{router_cert_path.str()};

    mysql_harness::make_file_public(ca_key_path.str());
    mysql_harness::make_file_public(ca_cert_path.str());
    mysql_harness::make_file_public(router_key_path.str());
    mysql_harness::make_file_public(router_cert_path.str());
  }

  TmpDir temp_dir;

  const mysql_harness::Path ca_key_path =
      mysql_harness::Path(temp_dir()).join("ca-key.pem");
  const mysql_harness::Path ca_cert_path =
      mysql_harness::Path(temp_dir()).join("ca.pem");
  const mysql_harness::Path router_key_path =
      mysql_harness::Path(temp_dir()).join("router-key.pem");
  const mysql_harness::Path router_cert_path =
      mysql_harness::Path(temp_dir()).join("router.pem");

  CertificateHandler cert_handler{ca_key_path, ca_cert_path, router_key_path,
                                  router_cert_path};
};

TEST_F(CertificateHandlerTest, no_cert_file_exists) {
  open_cert_files();
  EXPECT_FALSE(cert_handler.no_cert_files_exists());

  mysql_harness::delete_file(router_key_path.str());
  EXPECT_FALSE(cert_handler.no_cert_files_exists());

  mysql_harness::delete_file(router_cert_path.str());
  EXPECT_FALSE(cert_handler.no_cert_files_exists());

  mysql_harness::delete_file(ca_key_path.str());
  EXPECT_FALSE(cert_handler.no_cert_files_exists());

  mysql_harness::delete_file(ca_cert_path.str());
  EXPECT_TRUE(cert_handler.no_cert_files_exists());
}

TEST_F(CertificateHandlerTest, router_cert_file_exist) {
  open_cert_files();
  EXPECT_TRUE(cert_handler.router_cert_files_exists());

  mysql_harness::delete_file(ca_key_path.str());
  EXPECT_TRUE(cert_handler.router_cert_files_exists());

  mysql_harness::delete_file(ca_cert_path.str());
  EXPECT_TRUE(cert_handler.router_cert_files_exists());

  mysql_harness::delete_file(router_key_path.str());
  EXPECT_FALSE(cert_handler.router_cert_files_exists());

  mysql_harness::delete_file(router_cert_path.str());
  EXPECT_FALSE(cert_handler.router_cert_files_exists());
}

TEST_F(CertificateHandlerTest, create_success) {
  EXPECT_NO_THROW(cert_handler.create());

  EXPECT_TRUE(file_contains_regex(ca_key_path, "BEGIN RSA PRIVATE KEY"));
  EXPECT_TRUE(file_contains_regex(router_key_path, "BEGIN RSA PRIVATE KEY"));
  EXPECT_TRUE(file_contains_regex(ca_cert_path, "BEGIN CERTIFICATE"));
  EXPECT_TRUE(file_contains_regex(router_cert_path, "BEGIN CERTIFICATE"));
}

TEST_F(CertificateHandlerTest, create_fail) {
  const mysql_harness::Path ca_key_path =
      mysql_harness::Path(temp_dir()).join("not_there").join("ca-key.pem");
  const mysql_harness::Path ca_cert_path =
      mysql_harness::Path(temp_dir()).join("not_there").join("ca.pem");
  const mysql_harness::Path router_key_path =
      mysql_harness::Path(temp_dir()).join("not_there").join("router-key.pem");
  const mysql_harness::Path router_cert_path =
      mysql_harness::Path(temp_dir()).join("not_there").join("router.pem");

  CertificateHandler handler{ca_key_path, ca_cert_path, router_key_path,
                             router_cert_path};
  const auto &res = handler.create();
  EXPECT_FALSE(res);
  EXPECT_EQ(res.error(), std::errc::no_such_file_or_directory);
}

int main(int argc, char *argv[]) {
  init_test_logger();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
