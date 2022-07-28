/* Copyright (c) 2020, 2022, Oracle and/or its affiliates.

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
#include <mysql/components/minimal_chassis.h>
#include <mysql/components/services/reference_caching.h>
#include <mysql/udf_registration_types.h>
#include <string>
#include "my_io.h"
#include "my_sys.h"
#include "test_reference_cache.h"
#include "unit_test_common.h"

using registry_type_t = SERVICE_TYPE_NO_CONST(registry);
using loader_type_t = SERVICE_TYPE_NO_CONST(dynamic_loader);
using ref_cache_producer_type_t =
    SERVICE_TYPE_NO_CONST(test_ref_cache_producer);
using ref_cache_consumer_type_t =
    SERVICE_TYPE_NO_CONST(test_ref_cache_consumer);
SERVICE_TYPE(test_ref_cache_producer) * ref_cache_producer;
SERVICE_TYPE(test_ref_cache_consumer) * ref_cache_consumer;

extern mysql_component_t mysql_component_mysql_server;
class reference_cache : public ::testing::Test {
 protected:
  void SetUp() override {
    reg = nullptr;
    loader = nullptr;
    ref_cache_producer = nullptr;
    ref_cache_consumer = nullptr;
    ASSERT_FALSE(minimal_chassis_init((&reg), &COMPONENT_REF(mysql_server)));
    ASSERT_FALSE(reg->acquire("dynamic_loader",
                              reinterpret_cast<my_h_service *>(
                                  const_cast<loader_type_t **>(&loader))));
  }

  void TearDown() override {
    if (loader) {
      ASSERT_FALSE(reg->release(
          reinterpret_cast<my_h_service>(const_cast<loader_type_t *>(loader))));
    }
    ASSERT_FALSE(minimal_chassis_deinit(reg, &COMPONENT_REF(mysql_server)));
  }
  SERVICE_TYPE_NO_CONST(registry) * reg;
  SERVICE_TYPE(dynamic_loader) * loader;
};

TEST_F(reference_cache, try_ref_cache_load_unload) {
  static const char *urns[] = {"file://component_reference_cache"};
  std::string path;
  const char *urn;
  make_absolute_urn(*urns, &path);
  urn = path.c_str();
  ASSERT_FALSE(loader->load(&urn, 1));
  ASSERT_FALSE(loader->unload(&urn, 1));
}

TEST_F(reference_cache, ref_cache_components_load_unload) {
  static const char *urns[] = {"file://component_reference_cache",
                               "file://component_test_reference_cache"};

  std::string path;
  const char *absolute_urns[2];
  for (int i = 0; i < 2; i++)
    absolute_urns[i] = (char *)malloc(2046 * sizeof(char));
  make_absolute_urn(urns[0], &path);
  strcpy(const_cast<char *>(absolute_urns[0]), path.c_str());
  make_absolute_urn(urns[1], &path);
  strcpy(const_cast<char *>(absolute_urns[1]), path.c_str());

  ASSERT_FALSE(loader->load(absolute_urns, 2));
  ASSERT_FALSE(reg->acquire(
      "test_ref_cache_producer",
      reinterpret_cast<my_h_service *>(
          const_cast<ref_cache_producer_type_t **>(&ref_cache_producer))));

  ASSERT_FALSE(reg->acquire(
      "test_ref_cache_consumer",
      reinterpret_cast<my_h_service *>(
          const_cast<ref_cache_consumer_type_t **>(&ref_cache_consumer))));

  ASSERT_FALSE(
      ref_cache_consumer->mysql_test_ref_cache_consumer_counter_reset());
  ASSERT_FALSE(ref_cache_consumer->mysql_test_ref_cache_consumer_counter_get());
  /* It will give one cache event, passing valid service_name_index */
  ASSERT_TRUE(ref_cache_producer->mysql_test_ref_cache_produce_event(0));
  /* It will give no cache event, passing invalid service_name_index */
  ASSERT_FALSE(ref_cache_producer->mysql_test_ref_cache_produce_event(1));
  ASSERT_FALSE(ref_cache_producer->mysql_test_ref_cache_flush());
  ASSERT_FALSE(ref_cache_producer->mysql_test_ref_cache_release_cache());
  ASSERT_FALSE(
      ref_cache_producer->mysql_test_ref_cache_benchmark_run(0, 0, 0, 0));

  if (ref_cache_producer) {
    ASSERT_FALSE(reg->release(reinterpret_cast<my_h_service>(
        const_cast<ref_cache_producer_type_t *>(ref_cache_producer))));
  }

  if (ref_cache_consumer) {
    ASSERT_FALSE(reg->release(reinterpret_cast<my_h_service>(
        const_cast<ref_cache_consumer_type_t *>(ref_cache_consumer))));
  }
  ASSERT_FALSE(loader->unload(absolute_urns, 2));
  for (int i = 0; i < 2; i++) free(const_cast<char *>(absolute_urns[i]));
}

/* mandatory main function */
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);

  MY_INIT(argv[0]);

  char realpath_buf[FN_REFLEN];
  char basedir_buf[FN_REFLEN];
  my_realpath(realpath_buf, my_progname, 0);
  size_t res_length;
  dirname_part(basedir_buf, realpath_buf, &res_length);
  if (res_length > 0) basedir_buf[res_length - 1] = '\0';
  my_setwd(basedir_buf, 0);

  int retval = RUN_ALL_TESTS();
  return retval;
}
