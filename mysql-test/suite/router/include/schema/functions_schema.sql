# -----------------------------------------------------
# Schema basic_schema
# -----------------------------------------------------
# Create schema that contains each basic MRS object type
--disable_query_log
--disable_result_log
DROP SCHEMA IF EXISTS `func_schema` ;

CREATE SCHEMA IF NOT EXISTS `func_schema`;
USE `func_schema`;

DELIMITER $$;

CREATE FUNCTION `func_schema`.`nothing` () RETURNS INTEGER
BEGIN
  RETURN 0;
END;$$

CREATE FUNCTION `func_schema`.`func_sum` (a INTEGER, b INTEGER) RETURNS INTEGER
BEGIN
  RETURN a + b;
END;$$

CREATE FUNCTION `func_schema`.`move_char` (a VARCHAR(20)) RETURNS VARCHAR(20)
BEGIN
  RETURN CONCAT(a," appended");
END;$$

CREATE FUNCTION `func_schema`.`move_date` (a DATE) RETURNS DATE
BEGIN
  RETURN  a;
END;$$

CREATE FUNCTION `func_schema`.`move_year` (a YEAR) RETURNS YEAR
BEGIN
  RETURN  a;
END;$$

CREATE FUNCTION `func_schema`.`move_time` (a TIME) RETURNS TIME
BEGIN
  RETURN  a;
END;$$

CREATE FUNCTION `func_schema`.`move_bit` (a BIT(1)) RETURNS BIT(1)
BEGIN
  RETURN  a;
END;$$

CREATE FUNCTION `func_schema`.`move_tinyint1` (a TINYINT(1)) RETURNS TINYINT(1)
BEGIN
  RETURN  a+1;
END;$$

CREATE FUNCTION `func_schema`.`move_tinyint8` (a TINYINT(8)) RETURNS TINYINT(8)
BEGIN
  RETURN  a+1;
END;$$

CREATE FUNCTION `func_schema`.`move_decimal` (a DECIMAL) RETURNS DECIMAL
BEGIN
  RETURN  a+1;
END;$$

CREATE FUNCTION `func_schema`.`move_float` (a FLOAT) RETURNS FLOAT
BEGIN
  RETURN  a+1;
END;$$

CREATE FUNCTION `func_schema`.`move_double` (a DOUBLE) RETURNS DOUBLE
BEGIN
  RETURN  a+1;
END;$$

CREATE FUNCTION `func_schema`.`move_json` (a JSON) RETURNS JSON
BEGIN
  RETURN a;
END;$$


CREATE FUNCTION `func_schema`.`report_back_mysql_error1` (mysql_error INTEGER) RETURNS INTEGER
BEGIN
  SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'This stored procedure signaled an error.', MYSQL_ERRNO = mysql_error;
  RETURN 2;
END;$$

CREATE FUNCTION `func_schema`.`report_back_mysql_error2` (mysql_error INTEGER, mysql_message TEXT) RETURNS INTEGER
BEGIN
  SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = mysql_message, MYSQL_ERRNO = mysql_error;
  RETURN 2;
END;$$

CREATE FUNCTION `func_schema`.`report_back_mysql_error_if`(error_out BOOLEAN) RETURNS INTEGER
BEGIN
    IF (error_out = 1) THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'This stored procedured signaled an error.', MYSQL_ERRNO = 5511;
    END IF;
    RETURN 2;
END$$

--enable_query_log
--enable_result_log

--echo # DB `func_schema` - created

DELIMITER ;$$
