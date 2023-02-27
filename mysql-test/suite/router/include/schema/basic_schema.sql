# -----------------------------------------------------
# Schema basic_schema
# -----------------------------------------------------
# Create schema that contains each basic MRS object type
--disable_query_log
--disable_result_log
DROP SCHEMA IF EXISTS `basic_schema` ;

CREATE SCHEMA IF NOT EXISTS `basic_schema`;
USE `basic_schema`;

CREATE TABLE IF NOT EXISTS `basic_schema`.`table1` (
  `id` INTEGER NOT NULL AUTO_INCREMENT,
  PRIMARY KEY (`id`));
  
INSERT INTO`basic_schema`.`table1` (`id`)
  VALUES(1),(20),(30),(31),(50),(100);

CREATE TABLE IF NOT EXISTS `basic_schema`.`table2` (
  `id` INTEGER NOT NULL AUTO_INCREMENT,
  `name` VARCHAR(255) NOT NULL,
  `comments` VARCHAR(512) NULL,
  `date` DATETIME NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE INDEX `name_UNIQUE` (`name` ASC) VISIBLE);
  
INSERT INTO `basic_schema`.`table2` (`name`, `comments`, `date`)
  VALUES("First row", "First comment", "1977-01-21"),
  ("Second row", NULL, "1977-01-21"),
  ("Thrid row", NULL, "1977-02-21"),
  ("4th row", "This row was inserted as forth row.", "1977-01-21"),
  ("5th row", "", "2021-03-01"),
  ("6th row", "...", "2023-01-21");

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

--enable_query_log
--enable_result_log

--echo # DB `basic_schema` - created 

DELIMITER ;$$
