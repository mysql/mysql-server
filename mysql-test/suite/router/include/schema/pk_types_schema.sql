# -----------------------------------------------------
# Schema pk_types_schema
# -----------------------------------------------------
# Create schema that contains each basic MRS object type
--disable_query_log
--disable_result_log
DROP SCHEMA IF EXISTS `pk_types_schema` ;

CREATE SCHEMA IF NOT EXISTS `pk_types_schema`;
USE `pk_types_schema`;

# Numeric PKs
CREATE TABLE IF NOT EXISTS `t_int` (
  `id` INTEGER NOT NULL AUTO_INCREMENT, comment VARCHAR(255),
  PRIMARY KEY (`id`));

CREATE TABLE IF NOT EXISTS `t_tinyint` (
  `id` TINYINT NOT NULL AUTO_INCREMENT, comment VARCHAR(255),
  PRIMARY KEY (`id`));

CREATE TABLE IF NOT EXISTS `t_smallint` (
  `id` SMALLINT NOT NULL AUTO_INCREMENT, comment VARCHAR(255),
  PRIMARY KEY (`id`));

CREATE TABLE IF NOT EXISTS `t_mediumint` (
  `id` MEDIUMINT NOT NULL AUTO_INCREMENT, comment VARCHAR(255),
  PRIMARY KEY (`id`));

CREATE TABLE IF NOT EXISTS `t_bigint` (
  `id` BIGINT NOT NULL AUTO_INCREMENT, comment VARCHAR(255),
  PRIMARY KEY (`id`));
  
CREATE TABLE IF NOT EXISTS `t_decimal` (
  `id` DECIMAL NOT NULL AUTO_INCREMENT, comment VARCHAR(255),
  PRIMARY KEY (`id`));

CREATE TABLE IF NOT EXISTS `t_float` (
  `id` FLOAT NOT NULL AUTO_INCREMENT, comment VARCHAR(255),
  PRIMARY KEY (`id`));
  
CREATE TABLE IF NOT EXISTS `t_double` (
  `id` DOUBLE NOT NULL AUTO_INCREMENT, comment VARCHAR(255),
  PRIMARY KEY (`id`));

CREATE TABLE IF NOT EXISTS `t_bit` (
  `id` BIT NOT NULL AUTO_INCREMENT, comment VARCHAR(255),
  PRIMARY KEY (`id`));

--enable_query_log
--enable_result_log

--echo # DB `pk_types_schema` - created

