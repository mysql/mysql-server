/* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02111-1307  USA */

#include <example_services.h>
#include <gtest/gtest.h>
#include <m_ctype.h>
#include <my_sys.h>
#include <mysql/components/component_implementation.h>
#include <mysql/components/my_service.h>
#include <mysql/components/service.h>
#include <mysql/components/service_implementation.h>
#include <mysql/components/services/dynamic_loader.h>
#include <mysql/components/services/persistent_dynamic_loader.h>
#include <mysql/mysql_lex_string.h>
#include <auth/dynamic_privileges_impl.h>
#include <persistent_dynamic_loader.h>
#include <scope_guard.h>
#include <server_component.h>
#include <stddef.h>

#include "lex_string.h"
#include "my_inttypes.h"
#include "my_io.h"

extern mysql_component_t COMPONENT_REF(mysql_server);

struct mysql_component_t *mysql_builtin_components[]=
{
  &COMPONENT_REF(mysql_server),
  0
};

DEFINE_BOOL_METHOD(mysql_persistent_dynamic_loader_imp::load,
  (void* thd_ptr, const char *urns[], int component_count))
{
  return true;
}

DEFINE_BOOL_METHOD(mysql_persistent_dynamic_loader_imp::unload,
  (void* thd_ptr, const char *urns[], int component_count))
{
  return true;
}

DEFINE_BOOL_METHOD(dynamic_privilege_services_impl::register_privilege,
  (const char *privilege_str, size_t privilege_str_len))
{
  return true;
}

DEFINE_BOOL_METHOD(dynamic_privilege_services_impl::unregister_privilege,
  (const char *privilege_str, size_t privilege_str_len))
{
  return true;
}

DEFINE_BOOL_METHOD(dynamic_privilege_services_impl::has_global_grant,
  (Security_context_handle handle, const char *privilege_str,
   size_t privilege_str_len))
{
  return true;
}

  
/* TODO following code resembles symbols used in sql library, these should be
  some day extracted to be reused both in sql library and server component unit
  tests. */
typedef struct charset_info_st CHARSET_INFO;

extern "C"
{
  CHARSET_INFO *system_charset_info= &my_charset_latin1;
}
char opt_plugin_dir[FN_REFLEN];

bool check_string_char_length(const LEX_CSTRING &str, const char *err_msg,
  size_t max_char_length, const CHARSET_INFO *cs,
  bool no_error)
{
  return false;
}

bool check_valid_path(const char *path, size_t len)
{
  size_t prefix= my_strcspn(system_charset_info, path, path + len, FN_DIRSEP,
                            strlen(FN_DIRSEP));
  return  prefix < len;
}


namespace dynamic_loader_unittest {

  class dynamic_loader : public ::testing::Test
  {
  protected:
    virtual void SetUp()
    {
      my_getwd(opt_plugin_dir, FN_REFLEN, MYF(0));
      reg= NULL;
      loader= NULL;
      ASSERT_FALSE(mysql_services_bootstrap(&reg));
      ASSERT_FALSE(reg->acquire("dynamic_loader", (my_h_service*)&loader));
    }

    virtual void TearDown()
    {
      if (reg)
      {
        ASSERT_FALSE(reg->release((my_h_service)reg));
      }
      if (loader)
      {
        ASSERT_FALSE(reg->release((my_h_service)loader));
      }
      ASSERT_FALSE(mysql_services_shutdown());
    }
    SERVICE_TYPE(registry)* reg;
    SERVICE_TYPE(dynamic_loader)* loader;
  };

  TEST_F(dynamic_loader, bootstrap)
  {
    ASSERT_TRUE(loader != NULL);
  };

  TEST_F(dynamic_loader, try_load_component)
  {
    static const char* urns[]= { "file://component_example_component1" };
    ASSERT_FALSE(loader->load(urns, 1));
    ASSERT_FALSE(loader->unload(urns, 1));
  }

  TEST_F(dynamic_loader, try_unload_the_same_component_in_group)
  {
    static const char* urns[]= { "file://component_example_component1" };
    ASSERT_FALSE(loader->load(urns, 1));
    static const char* urns_bad[]= { "file://component_example_component1",
      "file://component_example_component1" };
    ASSERT_TRUE(loader->unload(urns_bad, 2));
    ASSERT_FALSE(loader->unload(urns, 1));
  }

  TEST_F(dynamic_loader, try_load_twice)
  {
    static const char* urns[]= { "file://component_example_component1" };
    ASSERT_FALSE(loader->load(urns, 1));
    ASSERT_TRUE(loader->load(urns, 1));
    {
      my_service<SERVICE_TYPE(example_math)> service("example_math", reg);
      ASSERT_FALSE((bool)service);
    }

    ASSERT_FALSE(loader->unload(urns, 1));
  }

  TEST_F(dynamic_loader, try_load_not_existing)
  {
    static const char* urns[]= { "file://component_example_component0" };
    ASSERT_TRUE(loader->load(urns, 1));
  }

  TEST_F(dynamic_loader, try_load_with_unsatisfied_dependencies)
  {
    static const char* urns[]= { "file://component_example_component3" };
    ASSERT_TRUE(loader->load(urns, 1));
  }

  TEST_F(dynamic_loader, try_load_and_forget)
  {
    static const char* urns[]= { "file://component_example_component1" };
    ASSERT_FALSE(loader->load(urns, 1));
  }

  TEST_F(dynamic_loader, try_unload_not_existing)
  {
    static const char* urns[]= { "file://component_example_component0" };
    ASSERT_TRUE(loader->unload(urns, 1));
  }

  TEST_F(dynamic_loader, load_different_components)
  {
    static const char* urns1[]= { "file://component_example_component1" };
    static const char* urns2[]= { "file://component_example_component2",
      "file://component_example_component3" };
    {
      my_service<SERVICE_TYPE(example_math)> service("example_math", reg);
      ASSERT_TRUE((bool)service);
    }
    ASSERT_FALSE(loader->load(urns1, 1));
    {
      my_service<SERVICE_TYPE(example_math)> service("example_math", reg);
      ASSERT_FALSE((bool)service);
    }
    ASSERT_FALSE(loader->unload(urns1, 1));
    ASSERT_FALSE(loader->load(urns2, 2));
    {
      my_service<SERVICE_TYPE(example_math)> service("example_math", reg);
      ASSERT_FALSE((bool)service);
    }
    ASSERT_FALSE(loader->unload(urns2, 2));
    {
      my_service<SERVICE_TYPE(example_math)> service("example_math", reg);
      ASSERT_TRUE((bool)service);
    }
  }

  TEST_F(dynamic_loader, dependencies)
  {
    static const char* urns1[]= { "file://component_example_component3" };
    static const char* urns2[]= { "file://component_example_component1",
      "file://component_example_component3" };
    {
      my_service<SERVICE_TYPE(example_math)> service("example_math", reg);
      ASSERT_TRUE((bool)service);
    }
    ASSERT_TRUE(loader->load(urns1, 1));
    {
      my_service<SERVICE_TYPE(example_math)> service("example_math", reg);
      ASSERT_TRUE((bool)service);
    }
    ASSERT_FALSE(loader->load(urns2, 2));
    ASSERT_FALSE(loader->unload(urns2, 2));
    {
      my_service<SERVICE_TYPE(example_math)> service("example_math", reg);
      ASSERT_TRUE((bool)service);
    }
  }

  TEST_F(dynamic_loader, cyclic_dependencies)
  {
    static const char* urns_self_depends[]=
      { "file://component_self_required_test_component" };
    static const char* urns_cyclic_depends_broken1[]=
      { "file://component_cyclic_dependency_test_component_1" };
    static const char* urns_cyclic_depends_broken2[]=
    { "file://component_cyclic_dependency_test_component_2" };
    static const char* urns_cyclic_depends[]=
    { "file://component_cyclic_dependency_test_component_1",
      "file://component_cyclic_dependency_test_component_2" };

    /* Self-provided requirements should pass. */
    ASSERT_FALSE(loader->load(urns_self_depends, 1));
    ASSERT_FALSE(loader->unload(urns_self_depends, 1));

    /* Broken cyclic dependency. */
    ASSERT_TRUE(loader->load(urns_cyclic_depends_broken1, 1));
    ASSERT_TRUE(loader->load(urns_cyclic_depends_broken2, 1));

    /* Correct cyclic dependency.*/
    ASSERT_FALSE(loader->load(urns_cyclic_depends, 2));
    ASSERT_FALSE(loader->unload(urns_cyclic_depends, 2));
  }

  TEST_F(dynamic_loader, first_dependency)
  {
    static const char* urn1[]= { "file://component_example_component1" };
    static const char* urn2[]= { "file://component_example_component2" };
    static const char* urn3[]= { "file://component_example_component3" };
    ASSERT_TRUE(loader->load(urn3, 1));
    ASSERT_FALSE(loader->load(urn1, 1));
    ASSERT_FALSE(loader->load(urn3, 1));
    ASSERT_FALSE(loader->load(urn2, 1));
    /*
      lib2 would be sufficient for lib3 to satisfy its dependencies, but lib3 is
      already using actively dependency on lib1, so we can't unload it here.
    */
    ASSERT_TRUE(loader->unload(urn1, 1));
  }

  TEST_F(dynamic_loader, iteration)
  {
    my_service<SERVICE_TYPE(dynamic_loader_query)>
      service("dynamic_loader_query", reg);
    ASSERT_FALSE(service);

    my_h_component_iterator iterator;
    const char* name;
    const char* urn;
    int count= 0;
    bool test_library_found= false;

    /* No components to iterate over. */
    ASSERT_TRUE(service->create(&iterator));

    static const char* urns[]= { "file://component_example_component1",
      "file://component_example_component2",
      "file://component_example_component3" };

    ASSERT_FALSE(loader->load(urns, 3));

    ASSERT_FALSE(service->create(&iterator));

    auto guard= create_scope_guard([&service, &iterator]()
    {
      service->release(iterator);
    });

    service->release(my_h_component_iterator{});
    ASSERT_TRUE(service->get(my_h_component_iterator{}, &name, &urn));
    ASSERT_TRUE(service->next(my_h_component_iterator{}));
    ASSERT_TRUE(service->is_valid(my_h_component_iterator{}));
    for (; !service->is_valid(iterator); service->next(iterator))
    {
      ASSERT_FALSE(service->get(iterator, &name, &urn));

      count++;
      test_library_found|= !strcmp(name, "mysql:example_component1")
        && !strcmp(urn, "file://component_example_component1");
    }
    ASSERT_TRUE(service->get(iterator, &name, &urn));
    ASSERT_TRUE(service->next(iterator));
    ASSERT_TRUE(service->is_valid(iterator));

    /* there should be at least 3 test components loaded. */
    ASSERT_GE(count, 3);
    ASSERT_TRUE(test_library_found);
  }

  TEST_F(dynamic_loader, metadata)
  {
    my_service<SERVICE_TYPE(dynamic_loader_query)>
      query_service("dynamic_loader_query", reg);
    ASSERT_FALSE(query_service);

    my_service<SERVICE_TYPE(dynamic_loader_metadata_enumerate)>
      metadata_service("dynamic_loader_metadata_enumerate", reg);
    ASSERT_FALSE(metadata_service);

    my_service<SERVICE_TYPE(dynamic_loader_metadata_query)>
      metadata_query_service("dynamic_loader_metadata_query", reg);
    ASSERT_FALSE(metadata_query_service);

    static const char* urns[]= { "file://component_example_component1",
      "file://component_example_component2",
      "file://component_example_component3" };

    ASSERT_FALSE(loader->load(urns, 3));

    my_h_component_iterator iterator;
    const char* name;
    const char* urn;
    const char* value;
    int count= 0;
    bool property_found= false;

    ASSERT_FALSE(query_service->create(&iterator));

    auto guard= create_scope_guard([&query_service, &iterator]()
    {
      query_service->release(iterator);
    });

    for (; !query_service->is_valid(iterator); query_service->next(iterator))
    {
      ASSERT_FALSE(query_service->get(iterator, &name, &urn));

      if (!strcmp(urn, "file://component_example_component1"))
      {
        ASSERT_FALSE(metadata_query_service->get_value(
          iterator, "mysql.author", &value));
        ASSERT_STREQ(value, "Oracle Corporation");
        ASSERT_FALSE(metadata_query_service->get_value(
          iterator, "mysql.license", &value));
        ASSERT_STREQ(value, "GPL");
        ASSERT_FALSE(metadata_query_service->get_value(
          iterator, "test_property", &value));
        ASSERT_TRUE(metadata_query_service->get_value(
          iterator, "non_existing_test_property", &value));

        my_h_component_metadata_iterator metadata_iterator;

        ASSERT_FALSE(metadata_service->create(iterator, &metadata_iterator));

        auto guard= create_scope_guard([&metadata_service, &metadata_iterator]()
        {
          metadata_service->release(metadata_iterator);
        });

        metadata_service->release(my_h_component_metadata_iterator{});
        ASSERT_TRUE(metadata_service->get(
          my_h_component_metadata_iterator{}, &name, &value));
        ASSERT_TRUE(metadata_service->next(
          my_h_component_metadata_iterator{}));
        ASSERT_TRUE(metadata_service->is_valid(
          my_h_component_metadata_iterator{}));
        for (; !metadata_service->is_valid(metadata_iterator);
          metadata_service->next(metadata_iterator))
        {
          ASSERT_FALSE(metadata_service->get(metadata_iterator, &name, &value));

          count++;
          property_found|= strcmp(name, "test_property");
        }
        ASSERT_TRUE(metadata_service->get(metadata_iterator, &name, &value));
        ASSERT_TRUE(metadata_service->next(metadata_iterator));
        ASSERT_TRUE(metadata_service->is_valid(metadata_iterator));

        /* there should be at least 3 properties. */
        ASSERT_GE(count, 3);
        ASSERT_TRUE(property_found);
      }
    }
  }
}

/* mandatory main function */
int main(int argc, char **argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
