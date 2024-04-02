/*
  Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "helper/make_shared_ptr.h"
#include "mrs/authentication/authorize_manager.h"

#include "mock/mock_auth_handler_factory.h"
#include "mock/mock_mysqlcachemanager.h"

using helper::MakeSharedPtr;
using testing::_;
using testing::Mock;
using testing::Return;
using testing::StrictMock;
using testing::Test;

class RouteManagerTests : public Test {
 public:
  void SetUp() override {
    sut_.reset(new mrs::authentication::AuthorizeManager(
        &mock_mysqlcache_, jwt_secret_, mock_factory_));
  }

  void verifyAndClearMocks(const std::vector<void *> &mocks) {
    Mock::VerifyAndClearExpectations(&mock_factory_);
    Mock::VerifyAndClearExpectations(&mock_mysqlcache_);

    for (auto p : mocks) Mock::VerifyAndClearExpectations(p);
  }

  std::string jwt_secret_{"Sshhhh do not tell anyone !"};
  StrictMock<MockMysqlCacheManager> mock_mysqlcache_;
  MakeSharedPtr<StrictMock<MockAuthHandlerFactory>> mock_factory_;
  std::unique_ptr<mrs::interface::AuthorizeManager> sut_;
};
