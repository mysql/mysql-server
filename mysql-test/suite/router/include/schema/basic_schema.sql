# -----------------------------------------------------
# Schema basic_schema
# -----------------------------------------------------
# Create schema that contains each basic MRS object type
--disable_query_log
--disable_result_log
DROP SCHEMA IF EXISTS `basic_schema` ;

--let $router_test_schema=basic_schema
CREATE SCHEMA IF NOT EXISTS `basic_schema`;
USE `basic_schema`;

CREATE TABLE IF NOT EXISTS `basic_schema`.`table1` (
  `id` INTEGER NOT NULL AUTO_INCREMENT,
  PRIMARY KEY (`id`));
  
INSERT INTO `basic_schema`.`table1` (`id`)
  VALUES(1),(20),(30),(31),(50),(100);

CREATE TABLE IF NOT EXISTS `basic_schema`.`table2` (
  `id` INTEGER NOT NULL AUTO_INCREMENT,
  `name` VARCHAR(255) NOT NULL,
  `comments` VARCHAR(512) NULL,
  `date` DATETIME NOT NULL DEFAULT "2024-07-16 14:43:22.000000",
  PRIMARY KEY (`id`),
  UNIQUE INDEX `name_UNIQUE` (`name` ASC) VISIBLE);

INSERT INTO `basic_schema`.`table2` (`id`, `name`, `comments`, `date`)
  VALUES(1, "First row", "First comment", "1977-01-21"),
  (2, "Second row", NULL, "1977-01-21"),
  (3, "Thrid row", NULL, "1977-02-21"),
  (4, "4th row", "This row was inserted as forth row.", "1977-01-21"),
  (5, "5th row", "", "2021-03-01"),
  (6, "6th row", "...", "2023-01-21");
  
CREATE TABLE IF NOT EXISTS `basic_schema`.`table3` (
  `id` INTEGER NOT NULL AUTO_INCREMENT,
  `cvarchar` VARCHAR(255) NOT NULL,
  `ctext` TEXT NULL,
  `cdatetime` DATETIME NOT NULL,
  `ctimestamp` TIMESTAMP NOT NULL,
  `cdate` DATE NOT NULL,
  `ctime` TIME NOT NULL,
  `cyear` YEAR NOT NULL,
  `csmallint` SMALLINT,
  `cbigint` BIGINT,
  `cbin` BINARY(3),
  `cfloat` FLOAT,
  `cdouble` DOUBLE,
  `cdecimal` DECIMAL(5,2),
  `cenum` ENUM('value1','value2','value3'),
  `cset` SET('value1','value2','value3'),
  `cbit` BIT,
  `cbit1` BIT(1),
  `cbit10` BIT(10),
  `ctinyint` TINYINT,
  `cmediumint` MEDIUMINT,
  `cblob` BLOB,
  `geo0` GEOMETRY NOT NULL SRID 0,
  `geo4326` GEOMETRY NOT NULL SRID 4326,
  PRIMARY KEY (`id`));

INSERT INTO `basic_schema`.`table3` 
  VALUES(1, "first row", "numeric zeros", "2020-12-01 12:01:50", "2020-12-01 12:01:50",
  "2020-12-01", "12:01:50", "2020",
  0, 0, 0, 0, 0, 0, "value1", "", b'0', b'0', b'0000000000',0,0, "", ST_GeomFromText('POINT(0 0)'), ST_GeomFromText('POINT(0 0)', 4326)),
  (2, "second row", "numeric min", "2020-12-02 12:02:50", "2020-12-02 12:02:50",
  "2020-12-02", "12:02:50", "2021",
    -32768, -9223372036854775808, 0, -999.9999, -999.9999, -999.99, "value1", "value1",
    b'0', b'0', b'0000000000',-128,-8388608, "1", ST_GeomFromText('POINT(-100 -110)'), ST_GeomFromText('POINT(-90 -90)', 4326)),
  (3, "thrid row", "numeric max", "2020-12-03 12:03:50", "2020-12-03 12:03:50",
  "2020-12-03", "12:03:50", "2022",
    32767, 9223372036854775807, b'111', 999.9999, 999.9999, 999.99, "value3", "value1,value2,value3"
    , b'1', b'1', b'1111111111',127, 8388607, x'0011ab0000122333', ST_GeomFromText('POINT(110 100)'), ST_GeomFromText('POINT(90 90)', 4326));


CREATE TABLE IF NOT EXISTS `basic_schema`.`table4`(
  `id` INTEGER NOT NULL AUTO_INCREMENT PRIMARY KEY,
  `first_name` TEXT,
  `last_name` TEXT,
  `table2_id` INTEGER DEFAULT NULL,
  FOREIGN KEY (table2_id) REFERENCES table2(id));

INSERT INTO `basic_schema`.`table4` VALUES
  (1,"Clarissa","Barton",NULL),
  (2,"Haley","White",2),
  (3,"George","Simmons",4);

DROP procedure IF EXISTS `procedure1`;

DELIMITER $$;
CREATE PROCEDURE `basic_schema`.`procedure1` ()
BEGIN
   select 20
   UNION
   select 22;
END;$$

CREATE PROCEDURE `basic_schema`.`procedure2` ()
BEGIN
   select 1, "aaa","BBB", CONVERT("1977-11-08 11:28",DATETIME)
   UNION
   select 3, "ccc","DDD", CONVERT("1977-11-08",DATETIME);
END;$$

CREATE PROCEDURE proc_int()
BEGIN
  SELECT id FROM `basic_schema`.`table3`;
END;$$

CREATE PROCEDURE proc_varchar()
BEGIN
  SELECT cvarchar FROM `basic_schema`.`table3`;
END;$$

CREATE PROCEDURE proc_text()
BEGIN
  SELECT ctext FROM `basic_schema`.`table3`;
END;$$

CREATE PROCEDURE proc_datetime()
BEGIN
  SELECT cdatetime FROM `basic_schema`.`table3`;
END;$$

CREATE PROCEDURE proc_timestamp()
BEGIN
  SELECT ctimestamp FROM `basic_schema`.`table3`;
END;$$

CREATE PROCEDURE proc_date()
BEGIN
  SELECT cdate FROM `basic_schema`.`table3`;
END;$$

CREATE PROCEDURE proc_time()
BEGIN
  SELECT ctime FROM `basic_schema`.`table3`;
END;$$

CREATE PROCEDURE proc_year()
BEGIN
  SELECT cyear FROM `basic_schema`.`table3`;
END;$$

CREATE PROCEDURE proc_smallint()
BEGIN
  SELECT csmallint FROM `basic_schema`.`table3`;
END;$$

CREATE PROCEDURE proc_bigint()
BEGIN
  SELECT cbigint FROM `basic_schema`.`table3`;
END;$$

CREATE PROCEDURE proc_bin()
BEGIN
  SELECT cbin FROM `basic_schema`.`table3`;
END;$$

CREATE PROCEDURE proc_float()
BEGIN
  SELECT cfloat FROM `basic_schema`.`table3`;
END;$$

CREATE PROCEDURE proc_double()
BEGIN
  SELECT cdouble FROM `basic_schema`.`table3`;
END;$$

CREATE PROCEDURE proc_decimal()
BEGIN
  SELECT cdecimal FROM `basic_schema`.`table3`;
END;$$

CREATE PROCEDURE proc_enum()
BEGIN
  SELECT cenum FROM `basic_schema`.`table3`;
END;$$

CREATE PROCEDURE proc_set()
BEGIN
  SELECT cset FROM `basic_schema`.`table3`;
END;$$

CREATE PROCEDURE proc_bit()
BEGIN
  SELECT cbit FROM `basic_schema`.`table3`;
END;$$

CREATE PROCEDURE proc_bit1()
BEGIN
  SELECT cbit FROM `basic_schema`.`table3`;
END;$$

CREATE PROCEDURE proc_bit10()
BEGIN
  SELECT cbit10 FROM `basic_schema`.`table3`;
END;$$

CREATE PROCEDURE proc_tinyint()
BEGIN
  SELECT ctinyint FROM `basic_schema`.`table3`;
END;$$

CREATE PROCEDURE proc_mediumint()
BEGIN
  SELECT cmediumint FROM `basic_schema`.`table3`;
END;$$

CREATE PROCEDURE proc_blob()
BEGIN
  SELECT cblob FROM `basic_schema`.`table3`;
END;$$


--enable_query_log
--enable_result_log

--echo # DB `basic_schema` - created

DELIMITER ;$$
