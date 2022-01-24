################################################################################
# System tables are created in the InnoDB engine. Altering engine of System table
# to MyISAM is not supported. To support logical upgrade, creation of System
# table in MyISAM engine is supported with some restrictions. Test cases in this
# file verifies aforementioned behavior.
#
# =============== TEST CASES ==================
#
# 1 Verify altering system table' engine to MyISAM.
# 2 Verify system table creation in MyISAM engine.
#
################################################################################

--source include/have_myisam.inc

CREATE TABLE system_tables (ID INT PRIMARY KEY AUTO_INCREMENT,
                            table_name VARCHAR(100));

if (!$uppercase_system_table_names) {
  INSERT INTO system_tables(table_name)
         SELECT concat(table_schema, ".", table_name)
         FROM INFORMATION_SCHEMA.tables WHERE table_schema =
              'mysql' AND table_name NOT IN('general_log', 'slow_log',
                                            'ndb_binlog_index')
         ORDER BY table_name;
}

# Use system table names in uppercase. Tests with l_c_t_n = 1 or 2 sets this
# variable.
if ($uppercase_system_table_names) {
  INSERT INTO system_tables(table_name)
         SELECT UPPER(concat(table_schema, ".", table_name))
         FROM INFORMATION_SCHEMA.tables WHERE table_schema =
              'mysql' AND table_name NOT IN('general_log', 'slow_log',
                                            'ndb_binlog_index')
         ORDER BY table_name;
}

--echo #-----------------------------------------------------------------------
--echo # Test case to verify altering system table's engine to MyISAM. Changing
--echo # system table's engine to MyISAM is not allowed.
--echo #-----------------------------------------------------------------------
DELIMITER $;
CREATE PROCEDURE test_system_table_alter_engine()
BEGIN
  DECLARE n INT DEFAULT 0;
  DECLARE i INT DEFAULT 1;

-- ER_UNSUPPORTED_ENGINE(1726) is reported for all the system engines except for
-- innodb_index_stats and innodb_table_stats. ER_TOO_LONG_KEY(1071) is reported for
-- innodb_index_stats and ER_NOT_ALLOWED_COMMAND(1148) is reported for innodb_table_stats
-- for now.
  DECLARE CONTINUE HANDLER FOR 1726, 1071, 1148
  BEGIN
    GET DIAGNOSTICS CONDITION 1 @message = MESSAGE_TEXT;
    SELECT @message AS ERROR;
  END;

  SELECT count(*) FROM system_tables INTO n;
  WHILE i <= n DO
    SELECT table_name FROM system_tables WHERE id = i INTO @table_name;
    SELECT @table_name;

    SET @sql_text = concat("ALTER TABLE ", @table_name, " ENGINE = MyISAM");
    PREPARE stmt FROM @sql_text;
    EXECUTE stmt;
    DEALLOCATE PREPARE stmt;

    SET i = i + 1;
  END WHILE;
END$
DELIMITER ;$

CALL test_system_table_alter_engine();

#cleanup
DROP PROCEDURE test_system_table_alter_engine;


--echo #-----------------------------------------------------------------------
--echo # Test case to verify system table creation in MyISAM engine. Creating
--echo # system table in MyISAM is allowed to support logical upgrade.
--echo #-----------------------------------------------------------------------
DELIMITER $;
CREATE PROCEDURE execute_stmt(stmt VARCHAR(255))
BEGIN
  SET @error_no = 0;
  SET @sql_stmt = stmt;

  PREPARE stmt FROM @sql_stmt;
  EXECUTE stmt;

  GET DIAGNOSTICS CONDITION 1 @error_no = MYSQL_ERRNO, @error_message = MESSAGE_TEXT;
  IF @error_no > 0 THEN
    SELECT "Warning" AS SEVERITY, @error_no as ERRNO, @error_message as MESSAGE;
  END IF;

  DEALLOCATE PREPARE stmt;
END$

CREATE PROCEDURE test_create_system_table()
BEGIN
  DECLARE n INT DEFAULT 0;
  DECLARE i INT DEFAULT 1;

-- Error ER_NO_SYSTEM_TABLE_ACCESS(3554) is reported for innodb_table_stats and
-- innodb_index_stats. For other tables ER_UNSUPPORTED_ENGINE "warning" is reported.
-- Warning is handled in the execute_stmt().
  DECLARE CONTINUE HANDLER FOR 3554
  BEGIN
    GET DIAGNOSTICS CONDITION 1 @error = MYSQL_ERRNO, @error_message = MESSAGE_TEXT;
  END;

  SELECT count(*) FROM system_tables INTO n;

  WHILE i <= n DO
    SET @error = 0;
    SELECT table_name FROM system_tables WHERE id = i INTO @table_name;

    SET @sql_text = concat('RENAME TABLE ', @table_name, ' TO mysql.backup_table');
    CALL execute_stmt(@sql_text);

    SET @sql_text = concat('CREATE TABLE ', @table_name, ' ENGINE=InnoDB AS SELECT * FROM mysql.backup_table');
    CALL execute_stmt(@sql_text);

    SET @sql_text = concat('DROP TABLE ', @table_name);
    CALL execute_stmt(@sql_text);

    SET @sql_text = concat('CREATE TABLE ', @table_name, ' ENGINE=MyISAM AS SELECT * FROM mysql.backup_table');
    CALL execute_stmt(@sql_text);

    IF @error > 0 THEN
      SELECT "ERROR" AS SEVERITY, @error AS ERRNO, @error_message AS MESSAGE;
    ELSE
      SET @sql_text = concat('DROP TABLE ', @table_name);
      CALL execute_stmt(@sql_text);
    END IF;

    SET @sql_text = concat('RENAME TABLE mysql.backup_table TO ', @table_name);
    CALL execute_stmt(@sql_text);

    SET i = i + 1;
  END WHILE;
END$
DELIMITER ;$

CALL test_create_system_table();

#cleanup
DROP PROCEDURE test_create_system_table;
DROP PROCEDURE execute_stmt;
DROP TABLE system_tables;
