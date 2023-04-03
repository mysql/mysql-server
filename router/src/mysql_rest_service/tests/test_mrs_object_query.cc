/*
  Copyright (c) 2023, Oracle and/or its affiliates.

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

#include <gtest/gtest.h>
#include "helper/expect_throw_msg.h"
#include "helper/json/text_to.h"
#include "mrs/database/helper/object_query.h"
#include "test_mrs_object_utils.h"

using mrs::database::ObjectFieldFilter;

// TODO
// - composite keys
// - nested join
// - s/base/nested/
// - 1:1
// - 1:n
// - n:m
// - reduce with value
// - 2 subqueries
// - 2 joins
// - allowed crud operation check

// inserts
// - PK - auto-inc / single / composite

namespace {
std::string build_select_json_object(
    std::shared_ptr<Object> object,
    const mrs::database::ObjectFieldFilter &filter) {
  mrs::database::JsonQueryBuilder qb(filter);
  qb.process_object(object);

  return qb.query().str();
}
}  // namespace

TEST(MrsObjectQuery, bad_metadata) {
  // no columns
}

TEST(MrsObjectQuery, plain) {
  auto actor = make_table("sakila", "actor");

  auto root = make_object({}, {actor});

  add_field(root, actor, "first_name", "first_name");
  add_field(root, actor, "last_name", "last_name");
  add_field(root, actor, "age", "age");

  {
    auto query = build_select_json_object(root, {});
    EXPECT_EQ(
        "SELECT JSON_OBJECT('first_name', `t`.`first_name`, 'last_name', "
        "`t`.`last_name`, 'age', `t`.`age`) FROM `sakila`.`actor` as `t`",
        query);
  }
}

// unnested n:1 reference in base object
TEST(MrsObjectQuery, unnested_n1_base) {
  auto city = make_table("sakila", "city");
  auto country = make_join("sakila", "country", 1,
                           {{"country_id", "country_id"}}, false, true);

  auto root = make_object({}, {city});

  add_field(root, city, "city", "city");
  add_field(root, city, "city_id", "city_id");
  add_field(root, country, "country", "country");
  add_field(root, country, "country_id", "country_id");

  {
    auto query = build_select_json_object(root, {});
    EXPECT_EQ(
        "SELECT JSON_OBJECT('city', `t`.`city`, 'city_id', `t`.`city_id`, "
        "'country', `t1`.`country`, 'country_id', `t1`.`country_id`) FROM "
        "`sakila`.`city` as `t` LEFT JOIN `sakila`.`country` as `t1` ON "
        "`t`.`country_id` = `t1`.`country_id`",
        query);
  }
}

// unnested n:1 reference in base object (composite key)
TEST(MrsObjectQuery, unnested_n1c_base) {
  auto actor = make_table("sakila", "actor");
  auto department = make_join("sakila", "department", 1,
                              {{"department_id", "department_id"},
                               {"business_unit_id", "business_unit_id"}},
                              false, true);

  auto root = make_object({}, {actor});

  add_field(root, actor, "first_name", "first_name");
  add_field(root, actor, "age", "age");
  add_field(root, department, "department", "name");
  add_field(root, department, "department_id", "department_id");
  add_field(root, department, "business_unit_id", "business_unit_id");

  // SELECT
  {
    auto query = build_select_json_object(root, {});
    EXPECT_EQ(
        "SELECT JSON_OBJECT('first_name', `t`.`first_name`, 'age', `t`.`age`, "
        "'department', `t1`.`name`, 'department_id', `t1`.`department_id`, "
        "'business_unit_id', `t1`.`business_unit_id`) FROM `sakila`.`actor` as "
        "`t` LEFT JOIN `sakila`.`department` as `t1` ON `t`.`department_id` = "
        "`t1`.`department_id` AND `t`.`business_unit_id` = "
        "`t1`.`business_unit_id`",
        query);
  }
}

// nested n:1 reference in base object
TEST(MrsObjectQuery, nested_n1_base) {
  auto city = make_table("sakila", "city");
  auto country = make_join("sakila", "country", 1,
                           {{"country_id", "country_id"}}, false, false);

  auto root = make_object({}, {city});

  add_field(root, city, "city", "city");
  add_field(root, city, "city_id", "city_id");
  add_field(root, city, "country_id", "country_id");

  auto nested = make_object(root, {country});
  add_field(nested, country, "country", "country");
  add_field(nested, country, "country_id", "country_id");

  add_object_field(root, country, "country", nested);

  {
    auto query = build_select_json_object(root, {});
    EXPECT_EQ(
        "SELECT JSON_OBJECT('city', `t`.`city`, 'city_id', `t`.`city_id`, "
        "'country_id', `t`.`country_id`, 'country', (SELECT "
        "JSON_OBJECT('country', `t1`.`country`, 'country_id', "
        "`t1`.`country_id`) FROM `sakila`.`country` as `t1` WHERE "
        "`t`.`country_id` = `t1`.`country_id` LIMIT 1)) FROM `sakila`.`city` "
        "as `t`",
        query);
  }
}

// nested 1:1 reference in base object (composite key)
TEST(MrsObjectQuery, nested_n1c_base) {
  auto actor = make_table("sakila", "actor");
  auto department = make_join("sakila", "department", 1,
                              {{"department_id", "department_id"},
                               {"business_unit_id", "business_unit_id"}},
                              false, false);

  auto root = make_object({}, {actor});

  add_field(root, actor, "first_name", "first_name");
  add_field(root, actor, "age", "age");

  auto nested = make_object(root, {department});
  add_field(nested, department, "name", "name");
  add_field(nested, department, "department_id", "department_id");
  add_field(nested, department, "business_unit_id", "business_unit_id");

  add_object_field(root, department, "department", nested);

  auto query = build_select_json_object(root, {});
  EXPECT_EQ(
      "SELECT JSON_OBJECT('first_name', `t`.`first_name`, 'age', `t`.`age`, "
      "'department', (SELECT JSON_OBJECT('name', `t1`.`name`, 'department_id', "
      "`t1`.`department_id`, 'business_unit_id', `t1`.`business_unit_id`) "
      "FROM `sakila`.`department` as `t1` WHERE "
      "`t`.`department_id` = `t1`.`department_id` AND `t`.`business_unit_id` "
      "= `t1`.`business_unit_id` LIMIT 1)) FROM `sakila`.`actor` as `t`",
      query);
}

// unnested 1:n reference in base object - invalid
TEST(MrsObjectQuery, unnested_1n_base) {
  // skip - validation done when querying metadata
}

// nested 1:n reference in base object
TEST(MrsObjectQuery, nested_1n_base) {
  auto country = make_table("sakila", "country");
  auto city = make_join("sakila", "city", 1, {{"country_id", "country_id"}},
                        true, false);

  auto root = make_object({}, {country});

  add_field(root, country, "country", "country");

  auto nested = make_object(root, {city});
  add_field(nested, city, "city", "city");
  add_field(nested, city, "city_id", "city_id");

  add_object_field(root, city, "cities", nested);
  {
    auto query = build_select_json_object(root, {});
    EXPECT_EQ(
        "SELECT JSON_OBJECT('country', `t`.`country`, 'cities', (SELECT "
        "JSON_ARRAYAGG(JSON_OBJECT('city', `t1`.`city`, 'city_id', "
        "`t1`.`city_id`)) FROM `sakila`.`city` as `t1` WHERE `t`.`country_id` "
        "= `t1`.`country_id`)) FROM `sakila`.`country` as `t`",
        query);
  }
}

// nested 1:n reference in base object (composite key)
TEST(MrsObjectQuery, nested_1nc_base) {
  auto actor = make_table("sakila", "actor");
  auto department = make_join("sakila", "department", 1,
                              {{"department_id", "department_id"},
                               {"business_unit_id", "business_unit_id"}},
                              true, false);

  auto root = make_object({}, {actor});
  add_field(root, actor, "first_name", "first_name");
  add_field(root, actor, "age", "age");

  auto nested = make_object(root, {department});
  add_object_field(root, department, "department", nested);
  add_field(nested, department, "name", "name");
  add_field(nested, department, "department_id", "department_id");
  add_field(nested, department, "business_unit_id", "business_unit_id");

  auto query = build_select_json_object(root, {});
  EXPECT_EQ(
      "SELECT JSON_OBJECT('first_name', `t`.`first_name`, 'age', `t`.`age`, "
      "'department', (SELECT JSON_ARRAYAGG(JSON_OBJECT('name', `t1`.`name`, "
      "'department_id', `t1`.`department_id`, 'business_unit_id', "
      "`t1`.`business_unit_id`)) FROM `sakila`.`department` as `t1` WHERE "
      "`t`.`department_id` = `t1`.`department_id` AND `t`.`business_unit_id` "
      "= `t1`.`business_unit_id`)) FROM `sakila`.`actor` as `t`",
      query);
}

// pure unnested n:m reference in base object - invalid
TEST(MrsObjectQuery, unnested_unnested_nm_base) {
  // skip - validation done when querying metadata
}

// nested+unnested n:m reference in base object
TEST(MrsObjectQuery, nested_unnested_nm_base) {
  auto actor = make_table("sakila", "actor");
  auto film_actor = make_join("sakila", "film_actor", 1,
                              {{"actor_id", "actor_id"}}, true, false);
  auto film =
      make_join("sakila", "film", 2, {{"film_id", "film_id"}}, true, true);

  auto root = make_object({}, {actor});
  add_field(root, actor, "first_name", "first_name");

  auto nested = make_object(root, {film_actor, film});
  add_object_field(root, film_actor, "films", nested);
  add_field(nested, film, "title", "title");
  add_field(nested, film, "description", "description");

  auto query = build_select_json_object(root, {});
  EXPECT_EQ(
      "SELECT JSON_OBJECT('first_name', `t`.`first_name`, 'films', (SELECT "
      "JSON_ARRAYAGG(JSON_OBJECT('title', `t2`.`title`, 'description', "
      "`t2`.`description`)) FROM `sakila`.`film_actor` as `t1` LEFT JOIN "
      "`sakila`.`film` as `t2` ON `t1`.`film_id` = `t2`.`film_id` WHERE "
      "`t`.`actor_id` = `t1`.`actor_id`)) FROM `sakila`.`actor` as `t`",
      query);
}

// nested+unnested n:m reference in base object + extra lookups, nested
// category
TEST(MrsObjectQuery, nested_unnested_nm_base_11) {
  auto actor = make_table("sakila", "actor");
  auto film_actor = make_join("sakila", "film_actor", 1,
                              {{"actor_id", "actor_id"}}, true, false);
  auto film =
      make_join("sakila", "film", 2, {{"film_id", "film_id"}}, true, true);
  auto lang = make_join("sakila", "language", 3,
                        {{"language_id", "language_id"}}, false, false);
  auto orig_lang =
      make_join("sakila", "language", 4,
                {{"original_language_id", "language_id"}}, false, false);
  auto film_category = make_join("sakila", "film_category", 5,
                                 {{"film_id", "film_id"}}, true, false);
  auto category = make_join("sakila", "category", 6,
                            {{"category_id", "category_id"}}, true, false);

  auto root = make_object({}, {actor});
  add_field(root, actor, "first_name", "first_name");

  auto nested = make_object(root, {film_actor, film});
  add_object_field(root, film_actor, "films", nested);
  add_field(nested, film, "title", "title");
  add_field(nested, film, "description", "description");

  auto langobj = make_object(nested, {lang});
  add_object_field(nested, lang, "language", langobj);

  add_field(langobj, lang, "name", "name");

  add_field(nested, orig_lang, "original_language", "name");

  auto catlist = make_object(nested, {film_category, category});
  add_object_field(nested, film_category, "categories", catlist);
  add_field(catlist, category, "category", "name");

  auto query = build_select_json_object(root, {});
  EXPECT_EQ(
      "SELECT JSON_OBJECT('first_name', `t`.`first_name`, 'films', (SELECT "
      "JSON_ARRAYAGG(JSON_OBJECT('title', `t2`.`title`, 'description', "
      "`t2`.`description`, 'language', (SELECT JSON_OBJECT('name', "
      "`t3`.`name`) FROM `sakila`.`language` as `t3` WHERE "
      "`t2`.`language_id` "
      "= `t3`.`language_id` LIMIT 1), 'original_language', `t4`.`name`, "
      "'categories', (SELECT JSON_ARRAYAGG(JSON_OBJECT('category', "
      "`t6`.`name`)) FROM `sakila`.`film_category` as `t5` LEFT JOIN "
      "`sakila`.`category` as `t6` ON `t5`.`category_id` = "
      "`t6`.`category_id` "
      "WHERE `t2`.`film_id` = `t5`.`film_id`))) FROM `sakila`.`film_actor` "
      "as "
      "`t1` LEFT JOIN `sakila`.`film` as `t2` ON `t1`.`film_id` = "
      "`t2`.`film_id` LEFT JOIN `sakila`.`language` as `t4` ON "
      "`t2`.`original_language_id` = `t4`.`language_id` WHERE `t`.`actor_id` "
      "= `t1`.`actor_id`)) FROM `sakila`.`actor` as `t`",
      query);
}

// nested+unnested n:m reference in base object + extra lookup, reduce
// category object to single value
TEST(MrsObjectQuery, nested_unnested_nm_base_11_embedded) {
  auto actor = make_table("sakila", "actor");
  auto film_actor = make_join("sakila", "film_actor", 1,
                              {{"actor_id", "actor_id"}}, true, false);
  auto film =
      make_join("sakila", "film", 2, {{"film_id", "film_id"}}, true, true);
  auto lang = make_join("sakila", "language", 3,
                        {{"language_id", "language_id"}}, false, false);
  auto orig_lang =
      make_join("sakila", "language", 4,
                {{"original_language_id", "language_id"}}, false, false);
  auto film_category = make_join("sakila", "film_category", 5,
                                 {{"film_id", "film_id"}}, true, false);
  auto category = make_join("sakila", "category", 6,
                            {{"category_id", "category_id"}}, true, false);

  auto root = make_object({}, {actor});
  add_field(root, actor, "first_name", "first_name");

  auto nested = make_object(root, {film_actor, film});
  add_object_field(root, film_actor, "films", nested);
  add_field(nested, film, "title", "title");
  add_field(nested, film, "description", "description");
  add_field(nested, lang, "language", "name");
  add_field(nested, orig_lang, "original_language", "name");

  auto catlist = make_object(nested, {film_category, category});
  add_object_field(nested, film_category, "categories", catlist);
  set_reduce_field(category, "name");

  auto query = build_select_json_object(root, {});
  EXPECT_EQ(
      "SELECT JSON_OBJECT('first_name', `t`.`first_name`, 'films', (SELECT "
      "JSON_ARRAYAGG(JSON_OBJECT('title', `t2`.`title`, 'description', "
      "`t2`.`description`, 'language', `t3`.`name`, 'original_language', "
      "`t4`.`name`, 'categories', (SELECT JSON_ARRAYAGG(`t6`.`name`) FROM "
      "`sakila`.`film_category` as `t5` LEFT JOIN `sakila`.`category` as "
      "`t6` "
      "ON `t5`.`category_id` = `t6`.`category_id` WHERE `t2`.`film_id` = "
      "`t5`.`film_id`))) FROM `sakila`.`film_actor` as `t1` LEFT JOIN "
      "`sakila`.`film` as `t2` ON `t1`.`film_id` = `t2`.`film_id` LEFT JOIN "
      "`sakila`.`language` as `t3` ON `t2`.`language_id` = "
      "`t3`.`language_id` "
      "LEFT JOIN `sakila`.`language` as `t4` ON `t2`.`original_language_id` "
      "= `t4`.`language_id` WHERE `t`.`actor_id` = `t1`.`actor_id`)) FROM "
      "`sakila`.`actor` as `t`",
      query);
}

// pure nested n:m reference in base object
TEST(MrsObjectQuery, nested_nm_base) {
  auto actor = make_table("sakila", "actor");
  auto film_actor = make_join("sakila", "film_actor", 1,
                              {{"actor_id", "actor_id"}}, true, false);
  auto film =
      make_join("sakila", "film", 2, {{"film_id", "film_id"}}, true, false);

  auto root = make_object({}, {actor});

  add_field(root, actor, "first_name", "first_name");

  auto nested_assoc = make_object(root, {film_actor});
  add_object_field(root, film_actor, "film_actor", nested_assoc);

  auto nested = make_object(nested_assoc, {film});
  add_object_field(nested_assoc, film, "film", nested);
  add_field(nested, film, "title", "title");
  add_field(nested, film, "description", "description");

  auto query = build_select_json_object(root, {});
  EXPECT_EQ(
      "SELECT JSON_OBJECT('first_name', `t`.`first_name`, 'film_actor', "
      "(SELECT JSON_ARRAYAGG(JSON_OBJECT('film', (SELECT "
      "JSON_ARRAYAGG(JSON_OBJECT('title', `t2`.`title`, 'description', "
      "`t2`.`description`)) FROM `sakila`.`film` as `t2` WHERE "
      "`t1`.`film_id` "
      "= `t2`.`film_id`))) FROM `sakila`.`film_actor` as `t1` WHERE "
      "`t`.`actor_id` = `t1`.`actor_id`)) FROM `sakila`.`actor` as `t`",
      query);
}

TEST(MrsObjectQuery, include_filter) {
  auto actor = make_table("sakila", "actor");
  auto film_actor = make_join("sakila", "film_actor", 1,
                              {{"actor_id", "actor_id"}}, true, false);
  auto film =
      make_join("sakila", "film", 2, {{"film_id", "film_id"}}, true, true);
  auto lang = make_join("sakila", "language", 3,
                        {{"language_id", "language_id"}}, false, false);
  auto orig_lang =
      make_join("sakila", "language", 4,
                {{"original_language_id", "language_id"}}, false, false);
  auto film_category = make_join("sakila", "film_category", 5,
                                 {{"film_id", "film_id"}}, true, false);
  auto category = make_join("sakila", "category", 6,
                            {{"category_id", "category_id"}}, true, false);

  auto root = make_object({}, {actor});
  add_field(root, actor, "first_name", "first_name");
  add_field(root, actor, "last_name", "last_name");

  auto nested = make_object(root, {film_actor, film});
  add_object_field(root, film_actor, "films", nested);
  add_field(nested, film, "title", "title");
  add_field(nested, film, "description", "description");
  add_field(nested, lang, "language", "name");
  add_field(nested, orig_lang, "original_language", "name");

  auto catlist = make_object(nested, {film_category, category});
  add_object_field(nested, film_category, "categories", catlist);
  set_reduce_field(category, "name");

  {
    auto filter = mrs::database::ObjectFieldFilter::from_url_filter(
        *root,
        {"first_name", "films.title", "films.language", "films.categories"});

    auto query = build_select_json_object(root, filter);
    EXPECT_EQ(
        "SELECT JSON_OBJECT('first_name', `t`.`first_name`, 'films', (SELECT "
        "JSON_ARRAYAGG(JSON_OBJECT('title', `t2`.`title`, 'language', "
        "`t3`.`name`, 'categories', (SELECT JSON_ARRAYAGG(`t6`.`name`) FROM "
        "`sakila`.`film_category` as `t5` LEFT JOIN `sakila`.`category` as "
        "`t6` ON `t5`.`category_id` = `t6`.`category_id` WHERE "
        "`t2`.`film_id` "
        "= `t5`.`film_id`))) FROM `sakila`.`film_actor` as `t1` LEFT JOIN "
        "`sakila`.`film` as `t2` ON `t1`.`film_id` = `t2`.`film_id` LEFT "
        "JOIN "
        "`sakila`.`language` as `t3` ON `t2`.`language_id` = "
        "`t3`.`language_id` WHERE `t`.`actor_id` = `t1`.`actor_id`)) FROM "
        "`sakila`.`actor` as `t`",
        query);
  }
  {
    auto filter =
        mrs::database::ObjectFieldFilter::from_url_filter(*root, {"films"});

    auto query = build_select_json_object(root, filter);
    EXPECT_EQ(
        "SELECT JSON_OBJECT('films', (SELECT "
        "JSON_ARRAYAGG(JSON_OBJECT('title', `t2`.`title`, 'description', "
        "`t2`.`description`, 'language', `t3`.`name`, 'original_language', "
        "`t4`.`name`, 'categories', (SELECT JSON_ARRAYAGG(`t6`.`name`) FROM "
        "`sakila`.`film_category` as `t5` LEFT JOIN `sakila`.`category` as "
        "`t6` ON `t5`.`category_id` = `t6`.`category_id` WHERE "
        "`t2`.`film_id` "
        "= `t5`.`film_id`))) FROM `sakila`.`film_actor` as `t1` LEFT JOIN "
        "`sakila`.`film` as `t2` ON `t1`.`film_id` = `t2`.`film_id` LEFT "
        "JOIN "
        "`sakila`.`language` as `t3` ON `t2`.`language_id` = "
        "`t3`.`language_id` LEFT JOIN `sakila`.`language` as `t4` ON "
        "`t2`.`original_language_id` = `t4`.`language_id` WHERE "
        "`t`.`actor_id` "
        "= `t1`.`actor_id`)) FROM `sakila`.`actor` as `t`",
        query);
  }
  {
    auto filter = mrs::database::ObjectFieldFilter::from_url_filter(
        *root, {"films.title"});

    auto query = build_select_json_object(root, filter);
    EXPECT_EQ(
        "SELECT JSON_OBJECT('films', (SELECT "
        "JSON_ARRAYAGG(JSON_OBJECT('title', `t2`.`title`)) FROM "
        "`sakila`.`film_actor` as `t1` LEFT JOIN `sakila`.`film` as `t2` ON "
        "`t1`.`film_id` = `t2`.`film_id` WHERE `t`.`actor_id` = "
        "`t1`.`actor_id`)) FROM `sakila`.`actor` as `t`",
        query);
  }
  {
    auto filter = mrs::database::ObjectFieldFilter::from_url_filter(
        *root, {"films.categories"});

    auto query = build_select_json_object(root, filter);
    EXPECT_EQ(
        "SELECT JSON_OBJECT('films', (SELECT "
        "JSON_ARRAYAGG(JSON_OBJECT('categories', (SELECT "
        "JSON_ARRAYAGG(`t6`.`name`) FROM `sakila`.`film_category` as `t5` "
        "LEFT "
        "JOIN `sakila`.`category` as `t6` ON `t5`.`category_id` = "
        "`t6`.`category_id` WHERE `t2`.`film_id` = `t5`.`film_id`))) FROM "
        "`sakila`.`film_actor` as `t1` LEFT JOIN `sakila`.`film` as `t2` ON "
        "`t1`.`film_id` = `t2`.`film_id` WHERE `t`.`actor_id` = "
        "`t1`.`actor_id`)) FROM `sakila`.`actor` as `t`",
        query);
  }
  {
    auto filter = mrs::database::ObjectFieldFilter::from_url_filter(
        *root, {"films.language"});

    auto query = build_select_json_object(root, filter);
    EXPECT_EQ(
        "SELECT JSON_OBJECT('films', (SELECT "
        "JSON_ARRAYAGG(JSON_OBJECT('language', `t3`.`name`)) FROM "
        "`sakila`.`film_actor` as `t1` LEFT JOIN `sakila`.`film` as `t2` ON "
        "`t1`.`film_id` = `t2`.`film_id` LEFT JOIN `sakila`.`language` as "
        "`t3` "
        "ON `t2`.`language_id` = `t3`.`language_id` WHERE `t`.`actor_id` = "
        "`t1`.`actor_id`)) FROM `sakila`.`actor` as `t`",
        query);
  }
  {
    auto filter = mrs::database::ObjectFieldFilter::from_url_filter(
        *root, {"films.original_language", "films.title"});

    auto query = build_select_json_object(root, filter);
    EXPECT_EQ(
        "SELECT JSON_OBJECT('films', (SELECT "
        "JSON_ARRAYAGG(JSON_OBJECT('title', `t2`.`title`, "
        "'original_language', "
        "`t4`.`name`)) FROM `sakila`.`film_actor` as `t1` LEFT JOIN "
        "`sakila`.`film` as `t2` ON `t1`.`film_id` = `t2`.`film_id` LEFT "
        "JOIN "
        "`sakila`.`language` as `t4` ON `t2`.`original_language_id` = "
        "`t4`.`language_id` WHERE `t`.`actor_id` = `t1`.`actor_id`)) FROM "
        "`sakila`.`actor` as `t`",
        query);
  }
}

TEST(MrsObjectQuery, include_filter_reduce_field) {
  auto actor = make_table("sakila", "actor");
  auto film_actor = make_join("sakila", "film_actor", 1,
                              {{"actor_id", "actor_id"}}, true, false);
  auto film =
      make_join("sakila", "film", 2, {{"film_id", "film_id"}}, true, true);
  auto lang = make_join("sakila", "language", 3,
                        {{"language_id", "language_id"}}, false, false);
  auto orig_lang =
      make_join("sakila", "language", 4,
                {{"original_language_id", "language_id"}}, false, false);
  auto film_category = make_join("sakila", "film_category", 5,
                                 {{"film_id", "film_id"}}, true, false);
  auto category = make_join("sakila", "category", 6,
                            {{"category_id", "category_id"}}, true, false);

  auto root = make_object({}, {actor});
  add_field(root, actor, "first_name", "first_name");

  auto nested = make_object(root, {film_actor, film});
  add_object_field(root, film_actor, "films", nested);
  add_field(nested, film, "title", "title");
  add_field(nested, film, "description", "description");
  add_field(nested, lang, "language", "name");
  add_field(nested, orig_lang, "original_language", "name");

  auto catlist = make_object(nested, {film_category, category});
  add_object_field(nested, film_category, "categories", catlist);
  set_reduce_field(category, "name");

  auto query = build_select_json_object(root, {});
  // TODO
  // // EXPECT_EQ(
  //     "SELECT JSON_OBJECT('first_name', `t`.`first_name`, 'films', (SELECT
  //     " "JSON_ARRAYAGG(JSON_OBJECT('title', `t2`.`title`, 'description', "
  //     "`t2`.`description`, 'language', `t3`.`name`, 'original_language', "
  //     "`t4`.`name`, 'categories', (SELECT JSON_ARRAYAGG(`t6`.`name`) FROM "
  //     "`sakila`.`film_category` as `t5` LEFT JOIN `sakila`.`category` as
  //     `t6` " "ON `t5`.`category_id` = `t6`.`category_id` WHERE
  //     `t2`.`film_id` = "
  //     "`t5`.`film_id`))) FROM `sakila`.`film_actor` as `t1` LEFT JOIN "
  //     "`sakila`.`film` as `t2` ON `t1`.`film_id` = `t2`.`film_id` LEFT JOIN
  //     "
  //     "`sakila`.`language` as `t3` ON `t2`.`language_id` =
  //     `t3`.`language_id` " "LEFT JOIN `sakila`.`language` as `t4` ON
  //     `t2`.`original_language_id` = "
  //     "`t4`.`language_id` WHERE `t`.`actor_id` = `t1`.`actor_id`)) FROM "
  //     "`sakila`.`actor` as `t`",
  //     query);
}

// TODO exclude filters, mixed
