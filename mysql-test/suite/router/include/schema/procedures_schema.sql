# -----------------------------------------------------
# Schema basic_schema
# -----------------------------------------------------
# Create schema that contains each basic MRS object type
--disable_query_log
--disable_result_log
DROP SCHEMA IF EXISTS `proc_schema` ;

--let $router_test_schema=proc_schema
CREATE SCHEMA IF NOT EXISTS `proc_schema`;
USE `proc_schema`;


CREATE TABLE IF NOT EXISTS `proc_schema`.`dummy_data` (
  `id` INTEGER NOT NULL AUTO_INCREMENT,
  `name` VARCHAR(255) NOT NULL,
  `comments` VARCHAR(512) NULL,
  `date` DATETIME NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE INDEX `name_UNIQUE` (`name` ASC) VISIBLE);
  
INSERT INTO `proc_schema`.`dummy_data` (`name`, `comments`, `date`)
  VALUES("First row", "First comment", "1977-01-21"),
  ("Second row", NULL, "1977-01-21"),
  ("Thrid row", NULL, "1977-02-21"),
  ("4th row", "This row was inserted as forth row.", "1977-01-21"),
  ("5th row", "", "2021-03-01"),
  ("6th row", "", "2023-01-21"),
  ("7th row", "Not empty string", "2023-02-21"),
  ("8th row", "Next entry", "2023-03-21"),
  ("9th row", "", "2023-04-21"),
  ("10th row", "...", "2023-05-21"),
  ("11th row", "...", "2023-06-21"),
  ("12th row", "...", "2023-07-21"),
  ("13th row", "...", "2023-08-21"),
  ("14th row", "this is fourteenth row", "2023-09-21"),
  ("15th row", "...", "2023-10-21"),
  ("16th row", "New entry in this month", "2023-11-02"),
  ("17th row", "Second in this month", "2023-11-04"),
  ("18th row", "Next one", "2023-11-05"),
  ("19th row", "...", "2023-11-06"),
  ("20th row", "...", "2023-11-07"),
  ("21th row", "New customer", "2023-11-08"),
  ("22th row", "...", "2023-11-09"),
  ("23th row", "...", "2023-11-10"),
  ("24th row", "...", "2023-11-11"),
  ("25th row", "...", "2023-11-12"),
  ("26th row", "...", "2023-11-12"),
  ("27th row", "...", "2023-11-12");

DROP procedure IF EXISTS `on_resultset`;

DELIMITER $$;
CREATE PROCEDURE `proc_schema`.`one_resultset` ()
BEGIN
   select * from `proc_schema`.`dummy_data`;
END;$$

CREATE PROCEDURE `proc_schema`.`two_resultsets` ()
BEGIN
   select * from `proc_schema`.`dummy_data` WHERE id > 1 and id < 5;
   select count(*) from `proc_schema`.`dummy_data`;
END;$$

CREATE PROCEDURE `proc_schema`.`three_resultsets` ()
BEGIN
    select min(id) from `proc_schema`.`dummy_data`;
    select max(id) from `proc_schema`.`dummy_data`;
    select * from `proc_schema`.`dummy_data` where id=(select max(id) from `proc_schema`.`dummy_data`);
END;$$

CREATE PROCEDURE `proc_schema`.`no_resultset_out_param` (OUT v integer)
BEGIN
    SET v=(select min(id) from `proc_schema`.`dummy_data`);
END;$$

CREATE PROCEDURE `proc_schema`.`one_resultset_out_param` (OUT v integer)
BEGIN
    select * from `proc_schema`.`dummy_data` where id  = (select max(id) from `proc_schema`.`dummy_data`);
    SET v=(select min(id) from `proc_schema`.`dummy_data`);
END;$$

CREATE PROCEDURE `proc_schema`.`proc_do_nothing` ()
BEGIN
END;$$


CREATE PROCEDURE `proc_schema`.`proc_sum` (a integer, b integer)
BEGIN
  SELECT a + b as result;
END;$$

CREATE PROCEDURE `proc_schema`.`proc_concat` (a TEXT, b TEXT)
BEGIN
  SELECT CONCAT(a,b) as result;
END;$$

CREATE PROCEDURE `proc_schema`.`proc_sum_out` (a integer, b integer, OUT result INTEGER)
BEGIN
  SET result = (SELECT a + b as result);
END;$$

CREATE PROCEDURE `proc_schema`.`proc_concat_out` (a TEXT, b TEXT, OUT result TEXT)
BEGIN
  SET result = (SELECT CONCAT(a,b) as result);
END;$$

CREATE PROCEDURE `proc_schema`.`move_char` (a VARCHAR(20), OUT result VARCHAR(20))
BEGIN
  SET result = CONCAT(a," appended");
END;$$

CREATE PROCEDURE `proc_schema`.`move_date` (a DATE, OUT result DATE)
BEGIN
  SET result = a;
END;$$

CREATE PROCEDURE `proc_schema`.`move_year` (a YEAR, OUT result YEAR)
BEGIN
  SET result = a;
END;$$

CREATE PROCEDURE `proc_schema`.`move_time` (a TIME, OUT result TIME)
BEGIN
  SET result = a;
END;$$

CREATE PROCEDURE `proc_schema`.`move_bit` (a BIT(1), OUT result BIT(1))
BEGIN
  SET result = a;
END;$$

CREATE PROCEDURE `proc_schema`.`move_tinyint1` (a TINYINT(1), OUT result TINYINT(1))
BEGIN
  SET result = a+1;
END;$$

CREATE PROCEDURE `proc_schema`.`move_tinyint8` (a TINYINT(8), OUT result TINYINT(8))
BEGIN
  SET result = a+1;
END;$$

CREATE PROCEDURE `proc_schema`.`move_decimal` (a DECIMAL, OUT result DECIMAL)
BEGIN
  SET result = a+1;
END;$$

CREATE PROCEDURE `proc_schema`.`move_float` (a FLOAT, OUT result FLOAT)
BEGIN
  SET result = a+1;
END;$$

CREATE PROCEDURE `proc_schema`.`move_double` (a DOUBLE, OUT result DOUBLE)
BEGIN
  SET result = a+1;
END;$$


CREATE PROCEDURE `proc_schema`.`report_back_mysql_error1` (mysql_error INTEGER)
BEGIN
  SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'This stored procedure signaled an error.', MYSQL_ERRNO = mysql_error;
END;$$

CREATE PROCEDURE `proc_schema`.`report_back_mysql_error2` (mysql_error INTEGER, mysql_message TEXT)
BEGIN
  SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = mysql_message, MYSQL_ERRNO = mysql_error;
END;$$

CREATE PROCEDURE `proc_schema`.`report_back_mysql_error_if`(IN error_out BOOLEAN)
BEGIN
    SELECT 1;
    IF (error_out = 1) THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'This stored procedured signaled an error.', MYSQL_ERRNO = 5511;
    ELSE
        SELECT 2;
    END IF;
END$$

--enable_query_log
--enable_result_log

--echo # DB `proc_schema` - created

DELIMITER ;$$
