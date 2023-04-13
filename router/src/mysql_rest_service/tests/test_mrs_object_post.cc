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
#include "mrs/database/helper/object_insert.h"
#include "mrs/database/helper/object_query.h"
#include "test_mrs_object_utils.h"

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

TEST(MrsObjectPost, bad_metadata) {
  return;  // temporary skip
  // no columns in the root object
  {
    auto country = make_table("sakila", "country");
    auto city = make_join("sakila", "city", 1, {{"country_id", "country_id"}},
                          true, false);

    auto root = make_object({}, {country});

    auto nested = make_object(root, {city});
    add_field(nested, city, "city", "city");
    add_field(nested, city, "city_id", "city_id");

    add_object_field(root, city, "cities", nested);

    rapidjson::Document doc;
    ASSERT_TRUE(helper::json::text_to(&doc, R"*({
    "cities": [
      {"city": "MyCity"},
      {"city": "New MyCity"},
      {"city": "West MyCity"}
    ]
  })*"));

    {
      mrs::database::JsonInsertBuilder ib(root);
      EXPECT_THROW_MSG(ib.process(doc), std::invalid_argument,
                       "Object metadata has no PRIMARY KEY columns");
    }
  }
  // no PK in the root object
  {
    auto country = make_table("sakila", "country");
    auto city = make_join("sakila", "city", 1, {{"country_id", "country_id"}},
                          true, false);

    auto root = make_object({}, {country});

    add_field(root, country, "country", "country");

    auto nested = make_object(root, {city});
    add_field(nested, city, "city", "city");
    add_field(nested, city, "city_id", "city_id");

    add_object_field(root, city, "cities", nested);

    rapidjson::Document doc;
    ASSERT_TRUE(helper::json::text_to(&doc, R"*({
    "country": "MyCountry",
    "cities": [
      {"city": "MyCity"},
      {"city": "New MyCity"},
      {"city": "West MyCity"}
    ]
  })*"));

    {
      mrs::database::JsonInsertBuilder ib(root);
      EXPECT_THROW_MSG(ib.process(doc), std::invalid_argument,
                       "Object metadata has no PRIMARY KEY columns");
    }
  }
}

TEST(MrsObjectPost, bad_document) {}

TEST(MrsObjectPost, plain) {
  auto actor = make_table("sakila", "actor");

  auto root = make_object({}, {actor});

  set_primary(set_auto_inc(add_field(root, actor, "actor_id", "actor_id")));
  add_field(root, actor, "first_name", "first_name");
  add_field(root, actor, "last_name", "last_name");
  add_field(root, actor, "age", "age");

  // INSERT
  {
    rapidjson::Document doc;

    helper::json::text_to(&doc, R"*({
    "first_name": "Arnold",
    "last_name": "Smith"
  })*");

    mrs::database::JsonInsertBuilder ib(root);
    ib.process(doc);

    auto sql = ib.insert();
    EXPECT_EQ(
        "INSERT INTO `sakila`.`actor` (`first_name`, `last_name`) VALUES "
        "('Arnold', 'Smith')",
        sql.str());

    auto extra_sql = ib.additional_inserts({});
    EXPECT_EQ(0, extra_sql.size());
  }
}

// unnested n:1 reference in base object
TEST(MrsObjectPost, unnested_n1_base) {
  auto city = make_table("sakila", "city");
  auto country = make_join("sakila", "country", 1,
                           {{"country_id", "country_id"}}, false, true);

  auto root = make_object({}, {city});

  add_field(root, city, "city", "city");
  set_auto_inc(set_primary(add_field(root, city, "city_id", "city_id")));
  add_field(root, country, "country", "country");
  set_auto_inc(
      set_primary(add_field(root, country, "country_id", "country_id")));

  if (0) {  // trying to insert nested object to unnested definition
    rapidjson::Document doc;

    // insert a new city to an existing country
    helper::json::text_to(&doc, R"*({
    "city": "Porto Alegre",
    "country": {
      "country_id": 15
    }
  })*");
    // auto sql = mrs::database::build_insert_json_object(root, doc);
    // EXPECT_EQ(2, sql.size());
    // EXPECT_EQ(
    //     "INSERT INTO `sakila`.`city` (`first_name`, `last_name`) VALUES "
    //     "('Arnold', 'Smith')",
    //     sql[0].str());
    // EXPECT_EQ(
    //     "INSERT INTO `sakila`.`city` (`first_name`, `last_name`) VALUES "
    //     "('Arnold', 'Smith')",
    //     sql[1].str());
  }
}

TEST(MrsObjectPost, nested_1n_base_aipk) {
  return;  // temporary skip
  auto country = make_table("sakila", "country");
  auto city = make_join("sakila", "city", 1, {{"country_id", "country_id"}},
                        true, false);

  auto root = make_object({}, {country});

  add_field(root, country, "country", "country");
  set_primary(
      set_auto_inc(add_field(root, country, "country_id", "country_id")));

  auto nested = make_object(root, {city});
  add_field(nested, city, "city", "city");
  set_primary(set_auto_inc(add_field(nested, city, "city_id", "city_id")));

  add_object_field(root, city, "cities", nested);

  // INSERT
  {
    rapidjson::Document doc;

    // insert a new country
    ASSERT_TRUE(helper::json::text_to(&doc, R"*({
    "country": "MyCountry"
  })*"));

    {
      mrs::database::JsonInsertBuilder ib(root);
      ib.process(doc);

      auto sql = ib.insert();
      EXPECT_EQ(
          "INSERT INTO `sakila`.`country` (`country`) VALUES "
          "('MyCountry')",
          sql.str());

      auto extra_sql = ib.additional_inserts({});
      EXPECT_EQ(0, extra_sql.size());
    }

    // insert new country and a few cities
    ASSERT_TRUE(helper::json::text_to(&doc, R"*({
    "country": "MyCountry",
    "cities": [
      {"city": "MyCity"},
      {"city": "New MyCity"},
      {"city": "West MyCity"}
    ]
  })*"));

    {
      mrs::database::JsonInsertBuilder ib(root);
      ib.process(doc);

      auto sql = ib.insert();
      EXPECT_EQ(
          "INSERT INTO `sakila`.`country` (`country`) VALUES ('MyCountry')",
          sql.str());

      EXPECT_EQ("country_id", ib.column_for_last_insert_id());
      EXPECT_EQ(0, ib.predefined_primary_key_values().size());
      auto auto_inc_value = mysqlrouter::sqlstring("42");

      auto extra_sql = ib.additional_inserts({{"country_id", auto_inc_value}});
      EXPECT_EQ(3, extra_sql.size());
      EXPECT_EQ(
          "INSERT INTO `sakila`.`city` (`city`, `country_id`) VALUES "
          "('MyCity', 42)",
          extra_sql[0].str());
      EXPECT_EQ(
          "INSERT INTO `sakila`.`city` (`city`, `country_id`) VALUES ('New "
          "MyCity', 42)",
          extra_sql[1].str());
      EXPECT_EQ(
          "INSERT INTO `sakila`.`city` (`city`, `country_id`) VALUES ('West "
          "MyCity', 42)",
          extra_sql[2].str());
    }
  }
}

TEST(MrsObjectPost, nested_1n_ref_base_aipk) {
  return;  // temporary skip
  auto country = make_table("sakila", "country");
  auto city = make_join("sakila", "city", 1, {{"country_id", "country_id"}},
                        true, false);
  auto city2 = make_join("sakila", "city", 2, {{"country_id", "country_id"}},
                         false, false);

  auto root = make_object({}, {country});

  add_field(root, country, "country", "country");
  set_primary(
      set_auto_inc(add_field(root, country, "country_id", "country_id")));

  auto nested = make_object(root, {city});
  add_field(nested, city, "city", "city");
  set_primary(set_auto_inc(add_field(nested, city, "city_id", "city_id")));

  auto capital = make_object(root, {city2});
  add_field(capital, city2, "city", "city");
  set_primary(set_auto_inc(add_field(capital, city2, "city_id", "city_id")));

  add_object_field(root, city, "cities", nested);
  add_object_field(root, city2, "capital", capital);

  {
    mrs::database::JsonQueryBuilder qb({});
    qb.process_object(root);

    EXPECT_EQ(
        "SELECT JSON_OBJECT('country', `t`.`country`, 'country_id', "
        "`t`.`country_id`, 'cities', (SELECT JSON_ARRAYAGG(JSON_OBJECT('city', "
        "`t1`.`city`, 'city_id', `t1`.`city_id`)) FROM `sakila`.`city` as `t1` "
        "WHERE `t`.`country_id` = `t1`.`country_id`), 'capital', (SELECT "
        "JSON_OBJECT('city', `t2`.`city`, 'city_id', `t2`.`city_id`) FROM "
        "`sakila`.`city` as `t2` WHERE `t`.`country_id` = `t2`.`country_id` "
        "LIMIT 1)) FROM `sakila`.`country` as `t`",
        qb.query().str());
  }

  // INSERT
  {
    rapidjson::Document doc;

    // insert a new country
    ASSERT_TRUE(helper::json::text_to(&doc, R"*({
    "country": "MyCountry"
  })*"));

    {
      mrs::database::JsonInsertBuilder ib(root);
      ib.process(doc);

      auto sql = ib.insert();
      EXPECT_EQ(
          "INSERT INTO `sakila`.`country` (`country`) VALUES "
          "('MyCountry')",
          sql.str());

      auto extra_sql = ib.additional_inserts({});
      EXPECT_EQ(0, extra_sql.size());
    }

    // insert new country, a few cities and assign the capital to one of them
    // unsupported: can't reference an auto-inc PK other than the root doc

    //   {
    //     "country": "MyCountry",
    //     "capital": {"city": "MyCity"}
    //     "cities": [
    //       {"city": "MyCity"},
    //       {"city": "New MyCity"},
    //       {"city": "West MyCity"}
    //     ]
    //   }
  }
}

TEST(MrsObjectPost, nested_n1_base) {
  auto city = make_table("sakila", "city");
  auto country = make_join("sakila", "country", 1,
                           {{"country_id", "country_id"}}, false, false);

  auto root = make_object({}, {city});

  add_field(root, city, "city", "city");
  set_primary(set_auto_inc(add_field(root, city, "city_id", "city_id")));
  add_field(root, city, "country_id", "country_id");

  auto nested = make_object(root, {country});
  add_field(nested, country, "country", "country");
  set_primary(
      set_auto_inc(add_field(nested, country, "country_id", "country_id")));

  add_object_field(root, country, "country", nested);

  {
    rapidjson::Document doc;

    // insert a new city (direct fk)
    helper::json::text_to(&doc, R"*({
    "city": "Porto Alegre",
    "country_id": 15
  })*");

    {
      mrs::database::JsonInsertBuilder ib(root);
      ib.process(doc);

      auto sql = ib.insert();
      EXPECT_EQ(
          "INSERT INTO `sakila`.`city` (`city`, `country_id`) VALUES "
          "('Porto Alegre', 15)",
          sql.str());

      auto extra_sql = ib.additional_inserts({});
      EXPECT_EQ(0, extra_sql.size());
    }

    // insert a new city to an existing country through a fk specified in the
    // nested object
    helper::json::text_to(&doc, R"*({
    "city": "Porto Alegre",
    "country": {
      "country_id": 15
    }
  })*");
    if (0) {  // TODO
      mrs::database::JsonInsertBuilder ib(root);
      ib.process(doc);

      auto sql = ib.insert();
      EXPECT_EQ(
          "INSERT INTO `sakila`.`city` (`city`, `country_id`) VALUES "
          "('Porto Alegre', 15)",
          sql.str());

      auto extra_sql = ib.additional_inserts({});
      EXPECT_EQ(0, extra_sql.size());
    }
  }
}

// pure nested n:m reference in base object
TEST(MrsObjectPost, nested_nm_base) {
  return;  // temporary skip
  auto actor = make_table("sakila", "actor");
  auto film_actor = make_join("sakila", "film_actor", 1,
                              {{"actor_id", "actor_id"}}, true, false);
  auto film =
      make_join("sakila", "film", 2, {{"film_id", "film_id"}}, true, false);

  auto root = make_object({}, {actor});

  set_auto_inc(set_primary(add_field(root, actor, "actor_id", "actor_id")));
  add_field(root, actor, "first_name", "first_name");

  auto nested_assoc = make_object(root, {film_actor});
  add_object_field(root, film_actor, "film_actor", nested_assoc);

  set_primary(add_field(nested_assoc, film_actor, "actor_id", "actor_id"));
  set_primary(add_field(nested_assoc, film_actor, "film_id", "film_id"));

  auto nested = make_object(nested_assoc, {film});
  add_object_field(nested_assoc, film, "film", nested);
  set_primary(set_auto_inc(add_field(nested, film, "film_id", "film_id")));
  add_field(nested, film, "title", "title");
  add_field(nested, film, "description", "description");

  {
    rapidjson::Document doc;

    // insert a new city (direct fk)
    helper::json::text_to(&doc, R"*({
    "first_name": "Jane",
    "film_actor": [
        {"film_id": 10},
        {"film_id": 15},
        {"film_id": 20}
    ]
  })*");

    {
      mrs::database::JsonInsertBuilder ib(root);
      ib.process(doc);

      auto sql = ib.insert();
      EXPECT_EQ("INSERT INTO `sakila`.`actor` (`first_name`) VALUES ('Jane')",
                sql.str());

      EXPECT_EQ("actor_id", ib.column_for_last_insert_id());
      EXPECT_EQ(0, ib.predefined_primary_key_values().size());
      auto auto_inc_value = mysqlrouter::sqlstring("42");

      auto extra_sql = ib.additional_inserts({{"actor_id", auto_inc_value}});
      EXPECT_EQ(3, extra_sql.size());

      EXPECT_EQ(
          "INSERT INTO `sakila`.`film_actor` (`film_id`, `actor_id`) VALUES "
          "(10, 42)",
          extra_sql[0].str());
      EXPECT_EQ(
          "INSERT INTO `sakila`.`film_actor` (`film_id`, `actor_id`) VALUES "
          "(15, 42)",
          extra_sql[1].str());
      EXPECT_EQ(
          "INSERT INTO `sakila`.`film_actor` (`film_id`, `actor_id`) VALUES "
          "(20, 42)",
          extra_sql[2].str());
    }
  }
}