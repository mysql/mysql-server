/* Copyright (c) 2015 Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#include <vector>
#include <algorithm>
#include <typeinfo>

#include "my_config.h"
#include <gtest/gtest.h>
#include "test_utils.h"
#include "mdl.h"
#include "test_mdl_context_owner.h"

#include "dd.h"

// Avoid warning about deleting ptr to incomplete type on Win
#include "dd/properties.h"

#include "dd/dd.h"
#include "dd/cache/element_map.h"
#include "dd/cache/dictionary_client.h"

#include "dd/impl/cache/free_list.h"
#include "dd/impl/cache/cache_element.h"
#include "dd/impl/cache/storage_adapter.h"
#include "dd/impl/cache/shared_dictionary_cache.h"

#include "dd/impl/types/charset_impl.h"
#include "dd/impl/types/collation_impl.h"
#include "dd/impl/types/schema_impl.h"
#include "dd/impl/types/table_impl.h"
#include "dd/impl/types/tablespace_impl.h"
#include "dd/impl/types/view_impl.h"


namespace dd {
bool operator==(const Weak_object &a, const Weak_object &b)
{
  std::string arep, brep;
  a.debug_print(arep);
  b.debug_print(brep);

  typedef std::string::iterator i_type;
  typedef std::pair<i_type, i_type> mismatch_type;

  bool arep_largest= (arep.size() > brep.size());

  std::string &largest= arep_largest ? arep : brep;
  std::string &smallest= arep_largest ? brep : arep;

  mismatch_type mismatch= std::mismatch(largest.begin(), largest.end(),
                                        smallest.begin());
  if (mismatch.first == largest.end())
  {
    return true;
  }

  std::string largediff= std::string(mismatch.first, largest.end());
  std::string smalldiff= std::string(mismatch.second, smallest.end());
  std::cout << "Debug representation not equal:\n"
            << std::string(largest.begin(), mismatch.first)
            << "\n<<<\n";
  if (arep_largest)
  {
    std::cout << largediff
              << "\n===\n"
              << smalldiff;
  }
  else
  {
    std::cout << smalldiff
              << "\n===\n"
              << largediff;
  }
  std::cout << "\n>>>\n" << std::endl;
  return false;
}

}

using dd_unittest::nullp;

namespace dd_cache_unittest {

class CacheStorageTest: public ::testing::Test, public Test_MDL_context_owner
{
public:
  dd::Schema_impl *mysql;

  void lock_object(const dd::Entity_object &eo)
  {
    MDL_request mdl_request;
    MDL_REQUEST_INIT(&mdl_request, MDL_key::TABLE,
                     MYSQL_SCHEMA_NAME.str, eo.name().c_str(),
                     MDL_EXCLUSIVE, MDL_TRANSACTION);
    EXPECT_FALSE(m_mdl_context.
                 acquire_lock(&mdl_request,
                              thd()->variables.lock_wait_timeout));
  }

protected:
    CacheStorageTest()
      : mysql(NULL)
  {
  }

  static void TearDownTestCase()
  {
    dd::cache::Shared_dictionary_cache::shutdown();
  }

  virtual void SetUp()
  {
    m_init.SetUp();
#ifndef DBUG_OFF
    dd::cache::Storage_adapter::s_use_fake_storage= true;
#endif /* !DBUG_OFF */
    mysql= new dd::Schema_impl();
    mysql->set_name("mysql");
    EXPECT_FALSE(thd()->dd_client()->store<dd::Schema>(mysql));
    EXPECT_LT(9999u, mysql->id());

    mdl_locks_unused_locks_low_water= MDL_LOCKS_UNUSED_LOCKS_LOW_WATER_DEFAULT;
    max_write_lock_count= ULONG_MAX;
    mdl_init();
    m_mdl_context.init(this);
    EXPECT_FALSE(m_mdl_context.has_locks());
  }

  virtual void TearDown()
  {
    /*
      Explicit scope + auto releaser to make sure acquired objects are
      released before teardown of the thd.
    */
    {
      const dd::Schema *acquired_mysql= NULL;
      dd::cache::Dictionary_client::Auto_releaser releaser(thd()->dd_client());
      EXPECT_FALSE(thd()->dd_client()->acquire<dd::Schema>(mysql->id(), &acquired_mysql));
      EXPECT_NE(nullp<const dd::Schema>(), acquired_mysql);
      EXPECT_FALSE(thd()->dd_client()->drop<dd::Schema>(const_cast<dd::Schema*>(acquired_mysql)));
    }
    delete mysql;
    m_mdl_context.release_transactional_locks();
    m_mdl_context.destroy();
    mdl_destroy();
#ifndef DBUG_OFF
    dd::cache::Storage_adapter::s_use_fake_storage= false;
#endif /* !DBUG_OFF */
    m_init.TearDown();
  }

  virtual void notify_shared_lock(MDL_context_owner *in_use,
                                  bool needs_thr_lock_abort)
  {
    in_use->notify_shared_lock(NULL, needs_thr_lock_abort);
  }


  // Return dummy thd.
  THD *thd()
  {
    return m_init.thd();
  }

  my_testing::Server_initializer m_init; // Server initializer.

  MDL_context        m_mdl_context;
  MDL_request        m_request;

private:
  GTEST_DISALLOW_COPY_AND_ASSIGN_(CacheStorageTest);
};

template <typename T>
class CacheTest: public ::testing::Test
{
protected:
  virtual void SetUp()
  { }

  virtual void TearDown()
  { }

};

typedef ::testing::Types
<
  dd::Charset_impl,
  dd::Collation_impl,
  dd::Schema_impl,
  dd::Table_impl,
  dd::Tablespace_impl,
  dd::View_impl
> DDTypes;
TYPED_TEST_CASE(CacheTest, DDTypes);

// Helper class to create objects and wrap them in elements.
class CacheTestHelper
{
public:
  template <typename T>
  static
  std::vector<dd::cache::Cache_element<typename T::cache_partition_type>*>
    *create_elements(uint num)
  {
    char name[2]= {'a', '\0'};
    std::vector<dd::cache::Cache_element<typename T::cache_partition_type>*>
      *objects= new std::vector<
        dd::cache::Cache_element<typename T::cache_partition_type>*>();
    for (uint id= 1; id <= num; id++, name[0]++)
    {
      T *object= new T();
      object->set_id(id);
      object->set_name(std::string(name));
      dd::cache::Cache_element<typename T::cache_partition_type> *element=
        new dd::cache::Cache_element<typename T::cache_partition_type>();
      element->set_object(object);
      element->recreate_keys();
      objects->push_back(element);
    }
    return objects;
  }

  template <typename T>
  static
  void delete_elements(std::vector<dd::cache::Cache_element
                       <typename T::cache_partition_type>*> *objects)
  {
    // Delete the objects and elements.
    for (typename std::vector<dd::cache::Cache_element
           <typename T::cache_partition_type>*>::iterator it=
             objects->begin();
         it != objects->end();)
    {
      // Careful about iterator invalidation.
      dd::cache::Cache_element
           <typename T::cache_partition_type> *element= *it;
      it++;
      delete(element->object());
      delete(element);
    }

    // Delete the vector with the elements pointer.
    objects->clear();
  }
};

// Test the free list.
TYPED_TEST(CacheTest, FreeList)
{
  // Create a free list and assert that it is empty.
  dd::cache::Free_list<dd::cache::Cache_element
    <typename TypeParam::cache_partition_type> > free_list;
  ASSERT_EQ(0U, free_list.length());

  // Create objects and wrap them in elements, add to vector of pointers.
  std::vector<dd::cache::Cache_element
    <typename TypeParam::cache_partition_type>*>
      *objects= CacheTestHelper::create_elements<TypeParam>(4);

  // Add the elements to the free list.
  for (typename std::vector<dd::cache::Cache_element
         <typename TypeParam::cache_partition_type>*>::iterator it=
           objects->begin();
       it != objects->end(); ++it)
    free_list.add_last(*it);

  // Now, length should be 4.
  ASSERT_EQ(4U, free_list.length());

  // Retrieving the least recently used element should return the first one.
  dd::cache::Cache_element<typename TypeParam::cache_partition_type>
          *element= free_list.get_lru();
  free_list.remove(element);
  ASSERT_EQ(3U, free_list.length());
  ASSERT_EQ(1U,    element->object()->id());
  ASSERT_EQ(std::string("a"), element->object()->name());

  // Now let us remove the middle of the remaining elements.
  free_list.remove(objects->at(2));
  ASSERT_EQ(2U, free_list.length());

  // Get the two last elements and verify they are what we expect.
  element= free_list.get_lru();
  free_list.remove(element);
  ASSERT_EQ(1U, free_list.length());
  ASSERT_EQ(2U,    element->object()->id());
  ASSERT_EQ(std::string("b"), element->object()->name());

  element= free_list.get_lru();
  free_list.remove(element);
  ASSERT_EQ(0U, free_list.length());
  ASSERT_EQ(4U,    element->object()->id());
  ASSERT_EQ(std::string("d"), element->object()->name());

  // Cleanup.
  CacheTestHelper::delete_elements<TypeParam>(objects);
  delete objects;
}

// Test the element map. Use a template function to do this for each
// of the key types, since we have already used the template support in
// gtest to handle the different object types.
template <typename T, typename K>
void element_map_test()
{
  // Create an element map and assert that it is empty.
  dd::cache::Element_map<K, dd::cache::Cache_element
                              <typename T::cache_partition_type> >
    element_map;
  ASSERT_EQ(0U, element_map.size());

  // Create objects and wrap them in elements.
  std::vector<dd::cache::Cache_element
    <typename T::cache_partition_type>*>
      *objects= CacheTestHelper::create_elements<T>(4);

  // Add the elements to the map.
  for (typename std::vector<dd::cache::Cache_element
         <typename T::cache_partition_type>*>::iterator it=
           objects->begin();
       it != objects->end(); ++it)
  {
    // Template disambiguator necessary.
    const K *key= (*it)->template get_key<K>();
    if (key)
      element_map.put(*key, *it);
  }

  // Now, the map should contain 4 elements.
  ASSERT_EQ(4U, element_map.size());

  // For each of the elements, make sure they are present, and that we
  // get the same element in return.
  // Add the elements to the map.
  for (typename std::vector<dd::cache::Cache_element
         <typename T::cache_partition_type>*>::iterator it=
           objects->begin();
       it != objects->end(); ++it)
  {
    // Template disambiguator necessary.
    const K *key= (*it)->template get_key<K>();
    if (key)
      ASSERT_TRUE(element_map.is_present(*key));
  }

  // Remove an element, and make sure the key

  // Delete the array and the objects.
  CacheTestHelper::delete_elements<T>(objects);
  delete objects;
}

TYPED_TEST(CacheTest, Element_map_reverse)
{
  element_map_test<TypeParam,
                   const typename TypeParam::cache_partition_type*>();
}

TYPED_TEST(CacheTest, Element_map_id_key)
{
  element_map_test<TypeParam, typename TypeParam::id_key_type>();
}

TYPED_TEST(CacheTest, Element_map_name_key)
{
  element_map_test<TypeParam, typename TypeParam::name_key_type>();
}

TYPED_TEST(CacheTest, Element_map_aux_key)
{
  // The aux key behavior is not uniform, and this test is therefore omitted.
}


#ifndef DBUG_OFF
template <typename Intrfc_type, typename Impl_type>
void test_basic_store_and_get(CacheStorageTest *tst, THD *thd)
{
  dd::cache::Dictionary_client *dc= thd->dd_client();
  dd::cache::Dictionary_client::Auto_releaser releaser(dc);

  std::unique_ptr<Impl_type> created(new Impl_type());
  Intrfc_type *icreated= created.get();
  dd_unittest::set_attributes(created.get(), "global_test_object");

  tst->lock_object(*created.get());
  EXPECT_FALSE(dc->store(icreated));
  EXPECT_LT(9999u, icreated->id());

  // Acquire by id.
  const Intrfc_type *acquired= NULL;
  EXPECT_FALSE(dc->acquire<Intrfc_type>(icreated->id(), &acquired));
  EXPECT_NE(nullp<Intrfc_type>(), acquired);
  EXPECT_NE(icreated, acquired);
  EXPECT_EQ(*icreated, *acquired);

  const Intrfc_type *name_acquired= NULL;
  EXPECT_FALSE(dc->acquire(icreated->name(), &name_acquired));
  EXPECT_NE(nullp<Intrfc_type>(), name_acquired);
  EXPECT_EQ(acquired, name_acquired);

  EXPECT_FALSE(dc->drop(const_cast<Intrfc_type*>(acquired)));
}

TEST_F(CacheStorageTest, BasicStoreAndGetCharset)
{
  test_basic_store_and_get<dd::Charset, dd::Charset_impl>(this, thd());
}

TEST_F(CacheStorageTest, BasicStoreAndGetCollation)
{
  test_basic_store_and_get<dd::Collation, dd::Collation_impl>(this, thd());
}

TEST_F(CacheStorageTest, BasicStoreAndGetSchema)
{
  test_basic_store_and_get<dd::Schema, dd::Schema_impl>(this, thd());
}

TEST_F(CacheStorageTest, BasicStoreAndGetTablespace)
{
  test_basic_store_and_get<dd::Tablespace, dd::Tablespace_impl>(this, thd());
}


template <typename Intrfc_type, typename Impl_type>
void test_basic_store_and_get_with_schema(CacheStorageTest *tst, THD *thd)
{
  dd::cache::Dictionary_client *dc= thd->dd_client();
  dd::cache::Dictionary_client::Auto_releaser releaser(dc);

  std::unique_ptr<Impl_type> created(new Impl_type());
  Intrfc_type *icreated= created.get();
  created->set_schema_id(tst->mysql->id());
  dd_unittest::set_attributes(created.get(), "schema_qualified_test_object",
                              *tst->mysql);

  tst->lock_object(*created.get());
  EXPECT_FALSE(dc->store(icreated));
  EXPECT_LT(9999u, icreated->id());

  // Acquire by id.
  const Intrfc_type *acquired= NULL;
  EXPECT_FALSE(thd->dd_client()->acquire<Intrfc_type>(icreated->id(),
                                                      &acquired));
  EXPECT_NE(nullp<Intrfc_type>(), acquired);
  EXPECT_NE(icreated, acquired);
  EXPECT_EQ(*icreated, *acquired);

  // Acquire by schema-qualified name.
  const Intrfc_type *name_acquired= NULL;
  EXPECT_FALSE(dc->acquire<Intrfc_type>(tst->mysql->name(), icreated->name(),
                                        &name_acquired));
  EXPECT_NE(nullp<Intrfc_type>(), name_acquired);
  EXPECT_EQ(acquired, name_acquired);

  EXPECT_FALSE(dc->drop(const_cast<Intrfc_type*>(acquired)));
}


TEST_F(CacheStorageTest, BasicStoreAndGetTable)
{
  test_basic_store_and_get_with_schema<dd::Table, dd::Table_impl>(this, thd());
}

TEST_F(CacheStorageTest, BasicStoreAndGetView)
{
  test_basic_store_and_get_with_schema<dd::View, dd::View_impl>(this, thd());
}


TEST_F(CacheStorageTest, GetTableBySePrivateId)
{
  dd::cache::Dictionary_client *dc= thd()->dd_client();
  dd::cache::Dictionary_client::Auto_releaser releaser(dc);

  std::unique_ptr<dd::Table> obj(dd::create_object<dd::Table>());
  dd_unittest::set_attributes(obj.get(), "table_object", *mysql);
  obj->set_engine("innodb");
  obj->set_se_private_id(0xEEEE); // Storing some magic number

  dd::Partition *part_obj= obj->add_partition();
  part_obj->set_name("table_part2");
  part_obj->set_level(1);
  part_obj->set_se_private_id(0xAFFF);
  part_obj->set_engine("innodb");
  part_obj->set_number(3);
  part_obj->set_comment("Partition comment");
  part_obj->set_tablespace_id(1);

  lock_object(*obj.get());
  EXPECT_FALSE(dc->store(obj.get()));

  std::string schema_name;
  std::string table_name;

  EXPECT_FALSE(dc->get_table_name_by_se_private_id("innodb", 0xEEEE,
                                                   &schema_name, &table_name));
  EXPECT_EQ(std::string("mysql"), schema_name);
  EXPECT_EQ(obj->name(), table_name);

  // Get table object.
  const dd::Table *tab= NULL;

  EXPECT_FALSE(dc->acquire<dd::Table>(schema_name, table_name, &tab));
  EXPECT_NE(nullp<const dd::Table>(), tab);
  if (tab)
  {
    EXPECT_EQ(tab->name(), table_name);
    EXPECT_EQ(*obj, *tab);

    // Get partition by ID
    const dd::Partition *p= tab->get_partition_by_se_private_id(0xAFFF);
    EXPECT_EQ(0xAFFFu, p->se_private_id());

    const dd::Table *obj2= NULL;
    EXPECT_FALSE(dc->acquire<dd::Table>("mysql", obj->name(), &obj2));
    EXPECT_NE(nullp<const dd::Table>(), tab);
    EXPECT_EQ(*obj, *obj2);

    EXPECT_FALSE(dc->drop(const_cast<dd::Table*>(obj2)));
  }
}

TEST_F(CacheStorageTest, TestRename)
{
  dd::cache::Dictionary_client &dc= *thd()->dd_client();
  dd::cache::Dictionary_client::Auto_releaser releaser(&dc);

  std::unique_ptr<dd::Table> temp_table(dd::create_object<dd::Table>());
  dd_unittest::set_attributes(temp_table.get(), "temp_table", *mysql);

  lock_object(*temp_table.get());
  EXPECT_FALSE(dc.store(temp_table.get()));

  {
    // Disable foreign key checks, we need to set this before
    // call to open_tables().
    thd()->variables.option_bits|= OPTION_NO_FOREIGN_KEY_CHECKS;

    dd::cache::Dictionary_client::Auto_releaser releaser(&dc);

    // Get 'test.temp_table' dd object. Schema id for 'mysql' is 1.
    const dd::Schema *sch= NULL;
    EXPECT_FALSE(dc.acquire<dd::Schema>(mysql->id(), &sch));
    EXPECT_NE(nullp<const dd::Schema>(), sch);

    const dd::Table *t= NULL;
    EXPECT_FALSE(dc.acquire<dd::Table>(sch->name(), "temp_table", &t));
    EXPECT_NE(nullp<const dd::Table>(), t);
    if (t)
    {
      dd::Table *temp_table= const_cast<dd::Table*>(t);

      temp_table->set_name("updated_table_name");

      // Change name of columns and indexes
      std::unique_ptr<dd::Iterator<dd::Column> > it_col(temp_table->columns());
      dd::Column *c= it_col->next();
      while (c)
      {
        c->set_name(c->name() + "_changed");
        c= it_col->next();
      }
      std::unique_ptr<dd::Iterator<dd::Index> > it_idx(temp_table->indexes());
      dd::Index *i= it_idx->next();
      while (i)
      {
        i->set_name(i->name() + "_changed");
        i= it_idx->next();
      }

      // Store the object.
      lock_object(*temp_table);
      dc.update(temp_table);

      // Enable foreign key checks
      thd()->variables.option_bits&= ~OPTION_NO_FOREIGN_KEY_CHECKS;
    }
    {
      // Get id of original object to be modified
      const dd::Table *temp_table= NULL;
      EXPECT_FALSE(dc.acquire<dd::Table>("mysql", "updated_table_name",
                                         &temp_table));
      EXPECT_NE(nullp<const dd::Table>(), temp_table);
      if (temp_table)
      {
        EXPECT_FALSE(dc.drop(const_cast<dd::Table*>(temp_table)));
      }
    }
    if (t)
    {
      const dd::Table *t= NULL;
      EXPECT_FALSE(dc.acquire<dd::Table>(sch->name(), "temp_table", &t));
      EXPECT_NE(nullp<const dd::Table>(), t);
      EXPECT_FALSE(dc.drop(const_cast<dd::Table*>(t)));
    }
  }
}


TEST_F(CacheStorageTest, TestSchema)
{
  dd::cache::Dictionary_client &dc= *thd()->dd_client();
  dd::cache::Dictionary_client::Auto_releaser releaser(&dc);

  std::unique_ptr<dd::Schema_impl> s(new dd::Schema_impl());
  s->set_name("schema1");
  EXPECT_FALSE(dc.store<dd::Schema>(s.get()));
  EXPECT_LT(9999u, s->id());

  std::unique_ptr<dd::Table_impl> t(new dd::Table_impl());
  t->set_name("table1");
  t->set_schema_id(s->id());
  EXPECT_FALSE(dc.store<dd::Table>(t.get()));
  EXPECT_LT(9999u, t->id());

  s->set_name("schema2");
  EXPECT_FALSE(dc.store<dd::Schema>(s.get()));
  EXPECT_LT(9999u, s->id());

  {
    // Get Schema object for "schema1" and "schema2".
    const dd::Schema *s1= NULL;
    const dd::Schema *s2= NULL;

    EXPECT_FALSE(dc.acquire<dd::Schema>("schema1", &s1));
    EXPECT_FALSE(dc.acquire<dd::Schema>("schema2", &s2));

    if (s1 && s2)
    {
      MDL_REQUEST_INIT(&m_request, MDL_key::TABLE,
                       "schema1", "table1",
                       MDL_EXCLUSIVE,
                       MDL_TRANSACTION);

      m_mdl_context.acquire_lock(&m_request,
                                 thd()->variables.lock_wait_timeout);


      // Get "schema1.table1" table from cache.
      const dd::Table *s1_t1= NULL;
      EXPECT_FALSE(dc.acquire<dd::Table>("schema1", "table1", &s1_t1));
      EXPECT_NE(nullp<const dd::Table>(), s1_t1);

      // Try to get "schema2.table1"(non existing) table.
      const dd::Table *s2_t1= NULL;
      EXPECT_TRUE(dc.acquire<dd::Table>("schema2", "table1", &s2_t1));
      EXPECT_EQ(nullp<const dd::Table>(), s2_t1);

      EXPECT_FALSE(dc.drop(const_cast<dd::Table*>(s1_t1)));
      EXPECT_FALSE(dc.drop(const_cast<dd::Schema*>(s2)));
      EXPECT_FALSE(dc.drop(const_cast<dd::Schema*>(s1)));
    }
  }
}


// Testing Transaction::max_se_private_id() method.
// Tables
TEST_F(CacheStorageTest, TestTransactionMaxSePrivateId)
{
  dd::cache::Dictionary_client &dc= *thd()->dd_client();
  dd::cache::Dictionary_client::Auto_releaser releaser(&dc);

  std::unique_ptr<dd::Table> tab1(dd::create_object<dd::Table>());
  std::unique_ptr<dd::Table> tab2(dd::create_object<dd::Table>());
  std::unique_ptr<dd::Table> tab3(dd::create_object<dd::Table>());

  dd_unittest::set_attributes(tab1.get(), "table1", *mysql);
  tab1->set_se_private_id(5);
  tab1->set_engine("innodb");
  lock_object(*tab1.get());
  dc.store(tab1.get());

  dd_unittest::set_attributes(tab2.get(), "table2", *mysql);
  tab2->set_se_private_id(10);
  tab2->set_engine("innodb");
  lock_object(*tab2.get());
  EXPECT_FALSE(dc.store(tab2.get()));

  dd_unittest::set_attributes(tab3.get(), "table3", *mysql);
  tab3->set_se_private_id(20);
  tab3->set_engine("unknown");
  lock_object(*tab3.get());
  dc.store(tab3.get());

  // Needs working dd::get_dictionary()
  //dd::Object_id max_id;
  //EXPECT_FALSE(dc.get_tables_max_se_private_id("innodb", &max_id));
  //EXPECT_EQ(10u, max_id);
  //EXPECT_FALSE(dc.get_tables_max_se_private_id("unknown", &max_id));
  //EXPECT_EQ(20u, max_id);

  const dd::Table *tab1_new= NULL;
  EXPECT_FALSE(dc.acquire_uncached_table_by_se_private_id("innodb", 5, &tab1_new));
  EXPECT_NE(nullp<dd::Table>(), tab1_new);

  const dd::Table *tab2_new= NULL;
  EXPECT_FALSE(dc.acquire_uncached_table_by_se_private_id("innodb", 10, &tab2_new));
  EXPECT_NE(nullp<dd::Table>(), tab2_new);

  const dd::Table *tab3_new= NULL;
  EXPECT_FALSE(dc.acquire_uncached_table_by_se_private_id("unknown", 20, &tab3_new));
  EXPECT_NE(nullp<dd::Table>(), tab3_new);

  // The tables are acquired uncached, so we must delete them to avoid
  // a memory leak.
  delete tab1_new;
  delete tab2_new;
  delete tab3_new;

  // Drop the objects
  EXPECT_FALSE(dc.acquire<dd::Table>("mysql", "table1", &tab1_new));
  EXPECT_FALSE(dc.acquire<dd::Table>("mysql", "table2", &tab2_new));
  EXPECT_FALSE(dc.acquire<dd::Table>("mysql", "table3", &tab3_new));
  EXPECT_FALSE(dc.drop(const_cast<dd::Table*>(tab1_new)));
  EXPECT_FALSE(dc.drop(const_cast<dd::Table*>(tab2_new)));
  EXPECT_FALSE(dc.drop(const_cast<dd::Table*>(tab3_new)));
}


/*
  The following test cases have been commented out since they test
  Dictionary_client member functions which bypass Storage_adapter and
  access the dictionary tables directly.
 */

// Test fetching multiple objects.
// Dictionary_client::fetch_catalog_components()
// Needs working Dictionary_impl::instance()
// TEST_F(CacheStorageTest, TestFetchCatalogComponents)
// {
//   dd::cache::Dictionary_client &dc= *thd()->dd_client();
//   dd::cache::Dictionary_client::Auto_releaser releaser(&dc);

//   std::unique_ptr<dd::Iterator<const dd::Schema> > schemas;
//   EXPECT_FALSE(dc.fetch_catalog_components(&schemas));

//   while (true)
//   {
//     const dd::Schema *s= schemas->next();

//     if (!s)
//     {
//       break;
//     }
//   }
// }

//
// Dictionary_client::fetch_schema_components()
//
// TEST_F(CacheStorageTest, TestFetchSchemaComponents)
// {
//   dd::cache::Dictionary_client &dc= *thd()->dd_client();
//   dd::cache::Dictionary_client::Auto_releaser releaser(&dc);

//   const dd::Schema *s= NULL;
//   EXPECT_FALSE(dc.acquire<dd::Schema>("mysql", &s));
//   EXPECT_NE(nullp<dd::Schema>(), s);

//   std::unique_ptr<dd::Iterator<const dd::Table> > tables;
//   EXPECT_FALSE(dc.fetch_schema_components(s, &tables));

//   while (true)
//   {
//     const dd::Table *t= tables->next();

//     if (!t)
//       break;
//   }
// }


//
// Dictionary_client::fetch_global_components()
//
// TEST_F(CacheStorageTest, TestFetchGlobalComponents)
// {
//   dd::cache::Dictionary_client &dc= *thd()->dd_client();
//   dd::cache::Dictionary_client::Auto_releaser releaser(&dc);

//   // Create a new tablespace.
//   dd::Object_id tablespace_id __attribute__((unused));
//   {
//     std::unique_ptr<dd::Tablespace> obj(dd::create_object<dd::Tablespace>());
//     dd_unittest::set_attributes(obj.get(), "test_tablespace");

//     //lock_object(thd, obj);
//     EXPECT_FALSE(dc.store(obj.get()));
//     tablespace_id= obj->id();
//   }
//   ha_commit_trans(thd(), false, true);

//   // List all tablespaces
//   {
//     std::unique_ptr<dd::Iterator<const dd::Tablespace> > tablespaces;
//     EXPECT_FALSE(dc.fetch_global_components(&tablespaces));

//     while (true)
//     {
//       const dd::Tablespace *t= tablespaces->next();

//       if (!t)
//         break;
//     }
//   }

//   // Drop tablespace
//   {
//     MDL_request mdl_request;
//     MDL_REQUEST_INIT(&mdl_request, MDL_key::TABLESPACE,
//                      "test_tablespace", "",
//                      MDL_EXCLUSIVE,
//                      MDL_TRANSACTION);
//     (void) thd()->mdl_context.acquire_lock(&mdl_request,
//                                          thd()->variables.lock_wait_timeout);
//     const dd::Tablespace *obj= NULL;
//     EXPECT_FALSE(dc.acquire<dd::Tablespace>(tablespace_id, &obj));
//     EXPECT_NE(nullp<const dd::Tablespace>(), obj);
//     EXPECT_FALSE(dc.drop(const_cast<dd::Tablespace*>(obj)));
//   }
//   EXPECT_FALSE(ha_commit_trans(thd(), false, true));
// }


TEST_F(CacheStorageTest, TestCacheLookup)
{
  dd::cache::Dictionary_client &dc= *thd()->dd_client();
  dd::cache::Dictionary_client::Auto_releaser releaser(&dc);

  std::string obj_name= dd::Table::OBJECT_TABLE().name() +
    std::string("_cacheissue");
  dd::Object_id id __attribute__((unused));
  //
  // Create table object
  //
  {
    std::unique_ptr<dd::Table> obj(dd::create_object<dd::Table>());
    dd_unittest::set_attributes(obj.get(), obj_name, *mysql);

    obj->set_engine("innodb");
    obj->set_se_private_id(0xFFFA); // Storing some magic number

    lock_object(*obj.get());
    EXPECT_FALSE(dc.store(obj.get()));
  }

  //
  // Step 1:
  // Get Table object given se_private_id = 0xFFFA
  // This should release the object reference in cache.
  //
  {
    std::string sch_name, tab_name;
    EXPECT_FALSE(dc.get_table_name_by_se_private_id("innodb",
                                                    0xFFFA,
                                                    &sch_name,
                                                    &tab_name));
    EXPECT_LT(0u, sch_name.size());
    EXPECT_LT(0u, tab_name.size());

    const dd::Table *obj= NULL;
    EXPECT_FALSE(dc.acquire<dd::Table>(sch_name, tab_name, &obj));
    EXPECT_NE(nullp<const dd::Table>(), obj);
    id= obj->id();
  }

  //
  // Step 2:
  // Get Table object given id and drop it.
  // This should remove object from the cache and delete it.
  //

  //
  // Get from cache and then drop it.
  //
  {
    const dd::Table *obj= NULL;
    EXPECT_FALSE(dc.acquire<dd::Table>(id, &obj));
    EXPECT_NE(nullp<const dd::Table>(), obj);
    EXPECT_FALSE(dc.drop(const_cast<dd::Table*>(obj)));
  }

  //
  // Step 3:
  // Again, try to get Table object with se_private_id = 0xFFFA
  // This should not get object reference in cache which was stored
  // by Step 1. Make sure we get no object.
  //

  {
    std::string sch_name, tab_name;
    EXPECT_TRUE(dc.get_table_name_by_se_private_id("innodb",
                                                   0XFFFA,
                                                   &sch_name,
                                                   &tab_name));
    EXPECT_EQ(0u, sch_name.size());
    EXPECT_EQ(0u, tab_name.size());
  }
}
#endif /* !DBUG_OFF */
}
