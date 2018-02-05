/* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

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
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <gtest/gtest.h>
#include <mysql/components/component_implementation.h>
#include <mysql/components/my_service.h>
#include <mysql/components/service_implementation.h>
#include <mysql/mysql_lex_string.h>
#include <stddef.h>

#include "m_ctype.h"

#include <component_status_var_service.h>
#include <component_sys_var_service.h>
#include <mysql/components/services/backup_lock_service.h>
#include <mysql/components/services/component_sys_var_service.h>
#include <mysql/components/services/persistent_dynamic_loader.h>
#include <persistent_dynamic_loader.h>
#include <security_context_imp.h>
#include <server_component.h>
#include <system_variable_source_imp.h>

#include "lex_string.h"
#include "my_compiler.h"
#include "my_io.h"
#include "scope_guard.h"
#include "sql/auth/dynamic_privileges_impl.h"
#include "sql/udf_registration_imp.h"

extern mysql_component_t COMPONENT_REF(mysql_server);

struct mysql_component_t *mysql_builtin_components[] = {
    &COMPONENT_REF(mysql_server), 0};

DEFINE_BOOL_METHOD(mysql_persistent_dynamic_loader_imp::load,
                   (void *, const char *[], int)) {
  return true;
}

DEFINE_BOOL_METHOD(mysql_persistent_dynamic_loader_imp::unload,
                   (void *, const char *[], int)) {
  return true;
}

DEFINE_BOOL_METHOD(register_privilege, (const char *, size_t)) { return true; }

DEFINE_BOOL_METHOD(unregister_privilege, (const char *, size_t)) {
  return true;
}

DEFINE_BOOL_METHOD(dynamic_privilege_services_impl::register_privilege,
                   (const char *, size_t)) {
  return true;
}

DEFINE_BOOL_METHOD(dynamic_privilege_services_impl::unregister_privilege,
                   (const char *, size_t)) {
  return true;
}

DEFINE_BOOL_METHOD(dynamic_privilege_services_impl::has_global_grant,
                   (Security_context_handle, const char *, size_t)) {
  return true;
}

DEFINE_BOOL_METHOD(mysql_udf_registration_imp::udf_unregister,
                   (const char *, int *)) {
  return true;
}

DEFINE_BOOL_METHOD(mysql_udf_registration_imp::udf_register_aggregate,
                   (const char *, enum Item_result, Udf_func_any, Udf_func_init,
                    Udf_func_deinit, Udf_func_add, Udf_func_clear)) {
  return true;
}

DEFINE_BOOL_METHOD(mysql_udf_registration_imp::udf_register,
                   (const char *, Item_result, Udf_func_any, Udf_func_init,
                    Udf_func_deinit)) {
  return true;
}

void component_sys_var_init() {}

void component_sys_var_deinit() {}

DEFINE_BOOL_METHOD(mysql_component_sys_variable_imp::register_variable,
                   (const char *, const char *, int, const char *,
                    mysql_sys_var_check_func, mysql_sys_var_update_func, void *,
                    void *)) {
  return true;
}

DEFINE_BOOL_METHOD(mysql_component_sys_variable_imp::get_variable,
                   (const char *, const char *, void **, size_t *)) {
  return true;
}

DEFINE_BOOL_METHOD(mysql_component_sys_variable_imp::unregister_variable,
                   (const char *, const char *)) {
  return true;
}

DEFINE_BOOL_METHOD(mysql_status_variable_registration_imp::register_variable,
                   (SHOW_VAR *)) {
  return true;
}

DEFINE_BOOL_METHOD(mysql_status_variable_registration_imp::unregister_variable,
                   (SHOW_VAR *)) {
  return true;
}

DEFINE_BOOL_METHOD(mysql_system_variable_source_imp::get,
                   (const char *, unsigned int, enum enum_variable_source *)) {
  return true;
}

DEFINE_BOOL_METHOD(mysql_acquire_backup_lock,
                   (MYSQL_THD, enum enum_backup_lock_service_lock_kind,
                    unsigned long)) {
  return true;
}

DEFINE_BOOL_METHOD(mysql_release_backup_lock, (MYSQL_THD)) { return true; }

DEFINE_BOOL_METHOD(mysql_security_context_imp::get,
                   (void *, Security_context_handle *)) {
  return true;
}

DEFINE_BOOL_METHOD(mysql_security_context_imp::set,
                   (void *, Security_context_handle)) {
  return true;
}

DEFINE_BOOL_METHOD(mysql_security_context_imp::create,
                   (Security_context_handle *)) {
  return true;
}

DEFINE_BOOL_METHOD(mysql_security_context_imp::destroy,
                   (Security_context_handle)) {
  return true;
}

DEFINE_BOOL_METHOD(mysql_security_context_imp::copy,
                   (Security_context_handle, Security_context_handle *)) {
  return true;
}

DEFINE_BOOL_METHOD(mysql_security_context_imp::lookup,
                   (Security_context_handle, const char *, const char *,
                    const char *, const char *)) {
  return true;
}

DEFINE_BOOL_METHOD(mysql_security_context_imp::get,
                   (Security_context_handle, const char *, void *)) {
  return true;
}

DEFINE_BOOL_METHOD(mysql_security_context_imp::set,
                   (Security_context_handle, const char *, void *)) {
  return true;
}
/* TODO following code resembles symbols used in sql library, these should be
  some day extracted to be reused both in sql library and server component unit
  tests. */
struct CHARSET_INFO;

CHARSET_INFO *system_charset_info = &my_charset_latin1;

char opt_plugin_dir[FN_REFLEN];

bool check_string_char_length(const LEX_CSTRING &, const char *, size_t,
                              const CHARSET_INFO *, bool) {
  MY_ASSERT_UNREACHABLE();
  return true;
}

bool check_valid_path(const char *, size_t) {
  MY_ASSERT_UNREACHABLE();
  return true;
}

namespace registry_unittest {

class registry : public ::testing::Test {
 protected:
  virtual void SetUp() { ASSERT_FALSE(mysql_services_bootstrap(&reg)); }

  virtual void TearDown() {
    ASSERT_FALSE(reg->release((my_h_service)reg));
    ASSERT_FALSE(mysql_services_shutdown());
  }
  SERVICE_TYPE(registry) * reg;
};

TEST_F(registry, bootstrap) { ASSERT_TRUE(reg != NULL); };

TEST_F(registry, basic_operations) {
  my_h_service hreg, hreg2;

  ASSERT_FALSE(reg->acquire("registry", &hreg));
  ASSERT_TRUE(hreg != NULL);
  ASSERT_FALSE(reg->acquire("registry.mysql_server", &hreg2));
  ASSERT_TRUE(hreg == hreg2);
  ASSERT_TRUE(hreg == (my_h_service)reg);
  ASSERT_FALSE(reg->release(hreg));
  ASSERT_FALSE(reg->release(hreg2));
  ASSERT_TRUE(reg->release(my_h_service{}));
}

TEST_F(registry, register_twice) {
  my_service<SERVICE_TYPE(registry_registration)> registration_service(
      "registry_registration", reg);
  ASSERT_FALSE(registration_service);
  {
    my_service<SERVICE_TYPE(registry)> service("test", reg);
    ASSERT_TRUE(service);
  }
  ASSERT_FALSE(registration_service->register_service(
      "test.test1", reinterpret_cast<my_h_service_imp *>(1)));
  ASSERT_TRUE(registration_service->register_service(
      "test.test1", reinterpret_cast<my_h_service_imp *>(1)));
  {
    my_service<SERVICE_TYPE(registry)> service("test", reg);
    ASSERT_FALSE(service);
  }

  ASSERT_FALSE(registration_service->unregister("test.test1"));
  {
    my_service<SERVICE_TYPE(registry)> service("test", reg);
    ASSERT_TRUE(service);
  }
}

TEST_F(registry, unregister_activelly_used) {
  my_service<SERVICE_TYPE(registry_registration)> registration_service(
      "registry_registration", reg);
  ASSERT_FALSE(registration_service);
  {
    my_service<SERVICE_TYPE(registry)> service("test", reg);
    ASSERT_TRUE(service);
  }
  ASSERT_FALSE(registration_service->register_service(
      "test.test1", reinterpret_cast<my_h_service_imp *>(1)));
  {
    my_service<SERVICE_TYPE(registry)> service("test", reg);
    ASSERT_FALSE(service);
    ASSERT_TRUE(registration_service->unregister("test.test1"));
  }

  ASSERT_FALSE(registration_service->unregister("test.test1"));
  {
    my_service<SERVICE_TYPE(registry)> service("test", reg);
    ASSERT_TRUE(service);
  }
}

TEST_F(registry, unregister_non_registered) {
  my_service<SERVICE_TYPE(registry_registration)> registration_service(
      "registry_registration", reg);
  ASSERT_FALSE(registration_service);
  ASSERT_TRUE(registration_service->unregister("test.test1"));
  {
    my_service<SERVICE_TYPE(registry)> service("test", reg);
    ASSERT_TRUE(service);
  }
}

TEST_F(registry, registration_and_default) {
  my_service<SERVICE_TYPE(registry_registration)> registration_service(
      "registry_registration", reg);
  ASSERT_FALSE(registration_service);
  {
    my_service<SERVICE_TYPE(registry)> service("test", reg);
    ASSERT_TRUE(service);
  }
  /* Null interface */
  ASSERT_TRUE(registration_service->register_service("test", my_h_service{}));
  /* Bad name */
  ASSERT_TRUE(registration_service->register_service(
      "test", reinterpret_cast<my_h_service_imp *>(1)));
  ASSERT_TRUE(registration_service->register_service(
      ".test", reinterpret_cast<my_h_service_imp *>(1)));
  ASSERT_TRUE(registration_service->register_service(
      "test.", reinterpret_cast<my_h_service_imp *>(1)));
  ASSERT_TRUE(registration_service->register_service(
      "test.test.test", reinterpret_cast<my_h_service_imp *>(1)));

  ASSERT_FALSE(registration_service->register_service(
      "test.test1", reinterpret_cast<my_h_service_imp *>(1)));
  {
    my_service<SERVICE_TYPE(registry)> service("test", reg);
    ASSERT_FALSE(service);
    ASSERT_EQ(
        static_cast<my_h_service_imp *>(static_cast<my_h_service>(service)),
        reinterpret_cast<my_h_service_imp *>(1));
  }
  ASSERT_FALSE(registration_service->register_service(
      "test.test2", reinterpret_cast<my_h_service_imp *>(2)));
  ASSERT_TRUE(registration_service->register_service(
      "test.test2", reinterpret_cast<my_h_service_imp *>(3)));
  {
    my_service<SERVICE_TYPE(registry)> service("test", reg);
    ASSERT_FALSE(service);
    ASSERT_EQ(
        static_cast<my_h_service_imp *>(static_cast<my_h_service>(service)),
        reinterpret_cast<my_h_service_imp *>(1));
  }
  ASSERT_FALSE(registration_service->set_default("test.test2"));
  ASSERT_TRUE(registration_service->set_default("bad_name.test2"));
  {
    my_service<SERVICE_TYPE(registry)> service("test", reg);
    ASSERT_FALSE(service);
    ASSERT_EQ(
        static_cast<my_h_service_imp *>(static_cast<my_h_service>(service)),
        reinterpret_cast<my_h_service_imp *>(2));
  }
  ASSERT_FALSE(registration_service->unregister("test.test2"));
  {
    my_service<SERVICE_TYPE(registry)> service("test", reg);
    ASSERT_FALSE(service);
    ASSERT_EQ(
        static_cast<my_h_service_imp *>(static_cast<my_h_service>(service)),
        reinterpret_cast<my_h_service_imp *>(1));
  }
  ASSERT_FALSE(registration_service->unregister("test.test1"));
  {
    my_service<SERVICE_TYPE(registry)> service("test", reg);
    ASSERT_TRUE(service);
  }
}
TEST_F(registry, my_service) {
  my_h_service hreg;
  ASSERT_FALSE(reg->acquire("registry_query", &hreg));
  ASSERT_TRUE(hreg != NULL);

  {
    my_service<SERVICE_TYPE(registry_query)> service("registry_query", reg);
    ASSERT_FALSE(service);
    ASSERT_TRUE(hreg == service);
  }

  ASSERT_FALSE(reg->release(hreg));
  ASSERT_TRUE(reg->release(hreg));
}
TEST_F(registry, acquire_related) {
  my_service<SERVICE_TYPE(registry_registration)> registration_service(
      "registry_registration", reg);
  ASSERT_FALSE(registration_service);
  ASSERT_FALSE(registration_service->register_service(
      "test.component1", reinterpret_cast<my_h_service_imp *>(1)));
  ASSERT_FALSE(registration_service->register_service(
      "test.component2", reinterpret_cast<my_h_service_imp *>(2)));
  ASSERT_FALSE(registration_service->register_service(
      "test.component3", reinterpret_cast<my_h_service_imp *>(3)));
  ASSERT_FALSE(registration_service->register_service(
      "another_service.component1", reinterpret_cast<my_h_service_imp *>(11)));
  ASSERT_FALSE(registration_service->register_service(
      "another_service.component2", reinterpret_cast<my_h_service_imp *>(12)));

  {
    my_service<SERVICE_TYPE(registry)> service1("test", reg);
    ASSERT_FALSE(service1);
    ASSERT_EQ(
        static_cast<my_h_service_imp *>(static_cast<my_h_service>(service1)),
        reinterpret_cast<my_h_service_imp *>(1));

    my_service<SERVICE_TYPE(registry)> service2("test.component2", reg);
    ASSERT_FALSE(service2);
    ASSERT_EQ(
        static_cast<my_h_service_imp *>(static_cast<my_h_service>(service2)),
        reinterpret_cast<my_h_service_imp *>(2));

    my_service<SERVICE_TYPE(registry)> service3("test.component3", reg);
    ASSERT_FALSE(service3);
    ASSERT_EQ(
        static_cast<my_h_service_imp *>(static_cast<my_h_service>(service3)),
        reinterpret_cast<my_h_service_imp *>(3));

    my_service<SERVICE_TYPE(registry)> another_service1("another_service",
                                                        service1, reg);
    ASSERT_FALSE(another_service1);
    ASSERT_EQ(static_cast<my_h_service_imp *>(
                  static_cast<my_h_service>(another_service1)),
              reinterpret_cast<my_h_service_imp *>(11));

    my_service<SERVICE_TYPE(registry)> another_service2("another_service",
                                                        service2, reg);
    ASSERT_FALSE(another_service2);
    ASSERT_EQ(static_cast<my_h_service_imp *>(
                  static_cast<my_h_service>(another_service2)),
              reinterpret_cast<my_h_service_imp *>(12));

    my_service<SERVICE_TYPE(registry)> another_service3("another_service",
                                                        service3, reg);
    ASSERT_FALSE(another_service3);
    ASSERT_EQ(static_cast<my_h_service_imp *>(
                  static_cast<my_h_service>(another_service3)),
              reinterpret_cast<my_h_service_imp *>(11));
  }

  ASSERT_FALSE(registration_service->unregister("test.component1"));
  ASSERT_FALSE(registration_service->unregister("test.component2"));
  ASSERT_FALSE(registration_service->unregister("test.component3"));
  ASSERT_FALSE(registration_service->unregister("another_service.component1"));
  ASSERT_FALSE(registration_service->unregister("another_service.component2"));

  /* Bad service implementation pointer */
  ASSERT_TRUE(reg->acquire_related("bad_name", my_h_service{}, NULL));
  ASSERT_TRUE(reg->acquire_related("bad_name", (my_h_service)reg, NULL));
  ASSERT_TRUE(
      reg->acquire_related("bad_name.with_component", (my_h_service)reg, NULL));

  {
    my_service<SERVICE_TYPE(registry)> scheme_file_service(
        "dynamic_loader_scheme_file.mysql_server_path_filter", reg);
    ASSERT_FALSE(scheme_file_service);

    /*
      No other services implemented with that implementation name, should
      fallback to default.
    */
    my_service<SERVICE_TYPE(registry)> another_service(
        "registry", scheme_file_service, reg);
    ASSERT_FALSE(another_service);
    ASSERT_EQ(another_service, reg);
  }
}
TEST_F(registry, iteration) {
  my_service<SERVICE_TYPE(registry_query)> service("registry_query", reg);
  ASSERT_FALSE(service);

  my_h_service_iterator iterator;
  const char *name;
  int count = 0;
  bool registrator_found = false;

  ASSERT_TRUE(service->create("not_existing", &iterator));
  ASSERT_FALSE(service->create("", &iterator));

  auto guard = create_scope_guard(
      [&service, &iterator]() { service->release(iterator); });

  service->release(my_h_service_iterator{});
  ASSERT_TRUE(service->get(my_h_service_iterator{}, &name));
  ASSERT_TRUE(service->next(my_h_service_iterator{}));
  ASSERT_TRUE(service->is_valid(my_h_service_iterator{}));

  for (; !service->is_valid(iterator); service->next(iterator)) {
    ASSERT_FALSE(service->get(iterator, &name));

    count++;
    registrator_found |= !strcmp(name, "registry_registration.mysql_server");
  }
  ASSERT_TRUE(service->get(iterator, &name));
  ASSERT_TRUE(service->next(iterator));
  ASSERT_TRUE(service->is_valid(iterator));

  ASSERT_GE(count, 2); /* there should be at least 2 services in registry. */
  ASSERT_TRUE(registrator_found);
}
}  // namespace registry_unittest

/* mandatory main function */
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
