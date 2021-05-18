CREATE DATABASE xtest;

## 1. Verify SQL preparation in performance_schema
## 2. Verify Crud.Find preparation in performance_schema
#   a. check find on a table
#   b. check find on a collection
## 3. Verify Crud.Insert preparation in performance_schema
#   a. success for execute with one parameter
#   b. success for execute with two parameters (more than needed)

Setup database
==============

command ok

 1. Verify SQL
==============
# Prepare stmt
Verify ["SELECT sql_text FROM performance_schema.prepared_statements_instances WHERE `STATEMENT_ID`=mysqlx_get_prepared_statement_id(42)" returns in "sql_text" column, following value SELECT 1]
# Deallocate the stmt
Verify ["SELECT count(sql_text) AS count_sql_text FROM performance_schema.prepared_statements_instances" returns in "count_sql_text" column, following value 0]

 2.a. Verify Crud.Find on a table
=================================
# Prepare stmt
Verify ["SELECT sql_text FROM performance_schema.prepared_statements_instances WHERE `STATEMENT_ID`=mysqlx_get_prepared_statement_id(42)" returns in "sql_text" column, following value SELECT '42' FROM `mysql`.`user`]
# Deallocate the stmt
Verify ["SELECT count(sql_text) AS count_sql_text FROM performance_schema.prepared_statements_instances" returns in "count_sql_text" column, following value 0]

 2.b. Verify Crud.Find on a collection
======================================
# Prepare stmt
Verify ["SELECT sql_text FROM performance_schema.prepared_statements_instances WHERE `STATEMENT_ID`=mysqlx_get_prepared_statement_id(42)" returns in "sql_text" column, following value SELECT doc FROM `coll`]
# Deallocate the stmt
Verify ["SELECT count(sql_text) AS count_sql_text FROM performance_schema.prepared_statements_instances" returns in "count_sql_text" column, following value 0]

 3.a. Verify Crud.Insert on a table with literal
================================================
# Prepare stmt
Verify ["SELECT sql_text FROM performance_schema.prepared_statements_instances WHERE `STATEMENT_ID`=mysqlx_get_prepared_statement_id(42)" returns in "sql_text" column, following value INSERT INTO `coll` (`doc`) VALUES ('{\"_id\":\"1\", \"pupil\":\"Alice\", \"pet\":\"cat\"}')]
# Deallocate the stmt
Verify ["SELECT count(sql_text) AS count_sql_text FROM performance_schema.prepared_statements_instances" returns in "count_sql_text" column, following value 0]

 3.b. Verify Crud.Insert on a collection with literal
=====================================================
# Prepare stmt
Verify ["SELECT sql_text FROM performance_schema.prepared_statements_instances WHERE `STATEMENT_ID`=mysqlx_get_prepared_statement_id(42)" returns in "sql_text" column, following value INSERT INTO `coll` (doc) VALUES ((SELECT JSON_INSERT(`_DERIVED_TABLE_`.`value`,'$._id',CONVERT(MYSQLX_GENERATE_DOCUMENT_ID(@@AUTO_INCREMENT_OFFSET,@@AUTO_INCREMENT_INCREMENT,JSON_CONTAINS_PATH(`_DERIVED_TABLE_`.`value`,'one','$._id')) USING utf8mb4)) FROM (SELECT '{\"_id\":\"1\", \"pupil\":\"Alice\", \"pet\":\"cat\"}' AS `value`) AS `_DERIVED_TABLE_`))]
# Deallocate the stmt
Verify ["SELECT count(sql_text) AS count_sql_text FROM performance_schema.prepared_statements_instances" returns in "count_sql_text" column, following value 0]

 3.c. Verify Crud.Insert on a collection with object
====================================================
# Prepare stmt
Verify ["SELECT sql_text FROM performance_schema.prepared_statements_instances WHERE `STATEMENT_ID`=mysqlx_get_prepared_statement_id(42)" returns in "sql_text" column, following value INSERT INTO `coll` (doc) VALUES ((SELECT JSON_INSERT(`_DERIVED_TABLE_`.`value`,'$._id',CONVERT(MYSQLX_GENERATE_DOCUMENT_ID(@@AUTO_INCREMENT_OFFSET,@@AUTO_INCREMENT_INCREMENT,JSON_CONTAINS_PATH(`_DERIVED_TABLE_`.`value`,'one','$._id')) USING utf8mb4)) FROM (SELECT JSON_OBJECT('pupil',?,'pet',?) AS `value`) AS `_DERIVED_TABLE_`))]
# Deallocate the stmt
Verify ["SELECT count(sql_text) AS count_sql_text FROM performance_schema.prepared_statements_instances" returns in "count_sql_text" column, following value 0]
Mysqlx.Ok {
  msg: "bye!"
}
ok
