/* Copyright (c) 2023, 2024, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2.0,
as published by the Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License, version 2.0, for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef TEST_HARNESS_SUITE_H
#define TEST_HARNESS_SUITE_H

#include <gtest/gtest.h>
#include "mysql/components/minimal_chassis.h"
#include "mysql/components/my_service.h"
#include "mysql/components/services/dynamic_loader.h"
#include "mysql/components/services/registry.h"

/**
  Implements a Gmock Test suite class to test a component.

  Load the component under test and fetches a reference to it.

  It can also load an optional local test harness component, should the
  component under test require extra services the minimal chassis doesn't
  provide. Usually the harness is test oriented and mocks the services so
  that the test can run.

  @tparam test_service_t The service type of the service to test
  @tparam component_name The name of the component to load, or nullptr to avoid
  loading extra components
  @tparam service_name The name of the service to instantiate, or nullptr to
  avoid fetching service refs
  @tparam component_instance The instance of the test harness component to load
  with the minchassis, or nullptr if none

  @sa @ref PAGE_COMPONENT_MOCK_UNIT_TEST_TOOLS.
*/
template <typename test_service_t, char const *component_name,
          char const *service_name, mysql_component_t *component_instance>
class TestHarnessSuite_templ : public ::testing::Test {
  using mysql_dyloader_t = SERVICE_TYPE(dynamic_loader);
  using mysql_registry_t = SERVICE_TYPE_NO_CONST(registry);

 protected:
  bool component_load(char const *component_to_load, char const *svc_to_load) {
    EXPECT_TRUE(m_urn.empty());

    EXPECT_EQ(m_dl, nullptr);
    m_dl = new my_service<mysql_dyloader_t>("dynamic_loader", m_reg);
    EXPECT_TRUE(m_dl->is_valid());

    m_urn = "file://";
    if (PLUGIN_DIR != nullptr) {
      const char *path = PLUGIN_DIR;
#ifdef WIN32
      std::string plugin_buf(PLUGIN_DIR);
      for (auto f : plugin_buf)
        if (f == '\\') f = '/';
      path = plugin_buf.c_str();
#endif
      m_urn.append(path).append("/");
    }
    m_urn.append(component_to_load);
    const char *urns[] = {m_urn.c_str()};
    if ((*m_dl)->load(urns, 1)) {
      m_urn.clear();
      return true;
    }
    assert(!m_test_svc);
    if (m_dl && svc_to_load != nullptr)
      m_test_svc = new my_service<test_service_t>(svc_to_load, m_reg);
    else
      m_test_svc = nullptr;
    return false;
  }
  void component_unload() {
    if (m_test_svc) {
      delete m_test_svc;
      m_test_svc = nullptr;
    }
    if (m_urn.size() > 0 && m_dl->is_valid()) {
      const char *urns[] = {m_urn.c_str()};
      (*m_dl)->unload(urns, 1);
      m_urn.clear();
    }
  }
  TestHarnessSuite_templ()
      : m_reg(nullptr), m_dl(nullptr), m_test_svc(nullptr) {
    EXPECT_FALSE(minimal_chassis_init(&m_reg, component_instance));

    if (component_name != nullptr &&
        component_load(component_name, service_name))
      m_urn.clear();
  }

  ~TestHarnessSuite_templ() override {
    if (component_name != nullptr) component_unload();
    delete m_dl;
    EXPECT_FALSE(minimal_chassis_deinit(m_reg, component_instance));
  }

  void SetUp() override {}

  void TearDown() override {}

 protected:
  mysql_registry_t *m_reg;                 /// Reference to the registry
  my_service<mysql_dyloader_t> *m_dl;      /// reference to the dynamic loader
  my_service<test_service_t> *m_test_svc;  /// reference to the test service
  std::string m_urn;  /// the calculated URN of the component
};

#endif
