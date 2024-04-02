# -----------------------------------------------------
# Schema pk_types_schema
# -----------------------------------------------------
# Create schema that contains each basic MRS object type
--disable_query_log
--disable_result_log
DROP SCHEMA IF EXISTS `pk_types_schema` ;
--let $router_test_schema=pk_types_schema
CREATE SCHEMA IF NOT EXISTS `pk_types_schema`;
USE `pk_types_schema`;

# Numeric PKs
CREATE TABLE IF NOT EXISTS `t_int` (
  `id` INTEGER NOT NULL,
  PRIMARY KEY (`id`));
INSERT INTO t_int(id) VALUES(0), (-2147483648), (2147483647);

CREATE TABLE IF NOT EXISTS `t_tinyint` (
  `id` TINYINT NOT NULL,
  PRIMARY KEY (`id`));
INSERT INTO t_tinyint(id) VALUES(0), (-128), (127);

CREATE TABLE IF NOT EXISTS `t_smallint` (
  `id` SMALLINT NOT NULL,
  PRIMARY KEY (`id`));
INSERT INTO t_smallint(id) VALUES(0), (-32768), (32767);

CREATE TABLE IF NOT EXISTS `t_mediumint` (
  `id` MEDIUMINT NOT NULL,
  PRIMARY KEY (`id`));
INSERT INTO t_mediumint(id) VALUES(0), (-8388608), (8388607);

CREATE TABLE IF NOT EXISTS `t_bigint` (
  `id` BIGINT NOT NULL,
  PRIMARY KEY (`id`));
INSERT INTO t_bigint(id) VALUES(0), (-9223372036854775808), (9223372036854775807);

CREATE TABLE IF NOT EXISTS `t_decimal` (
  `id` DECIMAL(5,2) NOT NULL,
  PRIMARY KEY (`id`));
INSERT INTO t_decimal(id) VALUES(0), (-999.99), (999.99);

CREATE TABLE IF NOT EXISTS `t_float` (
  `id` FLOAT NOT NULL,
  PRIMARY KEY (`id`));
INSERT INTO t_float(id) VALUES(0), (-999.9999), (999.9999);

CREATE TABLE IF NOT EXISTS `t_double` (
  `id` DOUBLE NOT NULL,
  PRIMARY KEY (`id`));
INSERT INTO t_double(id) VALUES(0), (-999.9999), (999.9999);

CREATE TABLE IF NOT EXISTS `t_bit` (
  `id` BIT NOT NULL,
  PRIMARY KEY (`id`));
INSERT INTO t_bit(id) VALUES(b'0'),  (b'1');

CREATE TABLE IF NOT EXISTS `t_bit1` (
  `id` BIT(1) NOT NULL,
  PRIMARY KEY (`id`));
INSERT INTO t_bit1(id) VALUES(b'0'), (b'1');

CREATE TABLE IF NOT EXISTS `t_bit8` (
  `id` BIT(8) NOT NULL,
  PRIMARY KEY (`id`));
INSERT INTO t_bit8(id) VALUES(b'00000000'), (b'00000001'), (b'11111111');

CREATE TABLE IF NOT EXISTS `t_bin` (
  `id` BINARY(3) NOT NULL,
  PRIMARY KEY (`id`));
INSERT INTO t_bin(id) VALUES(0), (b'000'), (b'111');

CREATE TABLE IF NOT EXISTS `t_varchar20` (
  `id` VARCHAR(20) NOT NULL,
  PRIMARY KEY (`id`));
INSERT INTO t_varchar20(id) VALUES(""), ("123456"), ("CREATE TABLE IF NOT");

CREATE TABLE IF NOT EXISTS `t_enum` (
  `id` ENUM("value1","value2","value3") NOT NULL,
  PRIMARY KEY (`id`));
INSERT INTO t_enum(id) VALUES("value1"), ("value3");
  
CREATE TABLE IF NOT EXISTS `t_set` (
  `id` SET("value1","value2","value3") NOT NULL,
  PRIMARY KEY (`id`));
INSERT INTO t_set(id) VALUES(""), ("value1"), ("value1,value2,value3");


CREATE TABLE IF NOT EXISTS `t_datetime` (
  `id` DATETIME NOT NULL,
  PRIMARY KEY (`id`));
INSERT INTO t_datetime(id) VALUES("2020-12-01 12:01:50"), ("2020-12-02 12:01:50"), ("2020-12-03 12:01:50");

CREATE TABLE IF NOT EXISTS `t_timestamp` (
  `id` TIMESTAMP NOT NULL,
  PRIMARY KEY (`id`));
INSERT INTO t_timestamp(id) VALUES("2020-12-01 12:01:50"), ("2020-12-02 12:01:50"), ("2020-12-03 12:01:50");

CREATE TABLE IF NOT EXISTS `t_date` (
  `id` DATE NOT NULL,
  PRIMARY KEY (`id`));
INSERT INTO t_date(id) VALUES("2020-12-01"), ("2020-12-02"), ("2020-12-03");

CREATE TABLE IF NOT EXISTS `t_time` (
  `id` TIME NOT NULL,
  PRIMARY KEY (`id`));
INSERT INTO t_time(id) VALUES("12:01:50"), ("12:01:51"), ("12:01:52");

CREATE TABLE IF NOT EXISTS `t_year` (
  `id` YEAR NOT NULL,
  PRIMARY KEY (`id`));
INSERT INTO t_year(id) VALUES("2020"), ("2021"), ("2022");

--enable_query_log
--enable_result_log

--echo # DB `pk_types_schema` - created

