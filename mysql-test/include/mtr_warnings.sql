delimiter ||;

use mtr||

--
-- Create table where testcases can insert patterns to
-- be suppressed
--
CREATE TABLE test_suppressions (
  pattern VARCHAR(255)
) ENGINE=MyISAM||


--
-- Declare a trigger that makes sure
-- no invalid patterns can be inserted
-- into test_suppressions
--
/*!50002
CREATE DEFINER=root@localhost TRIGGER ts_insert
BEFORE INSERT ON test_suppressions
FOR EACH ROW BEGIN
  DECLARE dummy INT;
  SELECT "" REGEXP NEW.pattern INTO dummy;
END
*/||


--
-- Load table with patterns that will be suppressed globally(always)
--
CREATE TABLE global_suppressions (
  pattern VARCHAR(255)
) ENGINE=MyISAM||


-- Declare a trigger that makes sure
-- no invalid patterns can be inserted
-- into global_suppressions
--
/*!50002
CREATE DEFINER=root@localhost TRIGGER gs_insert
BEFORE INSERT ON global_suppressions
FOR EACH ROW BEGIN
  DECLARE dummy INT;
  SELECT "" REGEXP NEW.pattern INTO dummy;
END
*/||



--
-- Insert patterns that should always be suppressed
--
INSERT INTO global_suppressions VALUES
 ("'SELECT UNIX_TIMESTAMP\\(\\)' failed on master"),
 ("Aborted connection"),
 ("Client requested master to start replication from impossible position"),
 ("Could not find first log file name in binary log"),
 ("Enabling keys got errno"),
 ("Error reading master configuration"),
 ("Error reading packet"),
 ("Event Scheduler"),
 ("Failed to open log"),
 ("Failed to open the existing master info file"),
 ("Forcing shutdown of [0-9]* plugins"),
 ("Forcing close of thread"),

 /*
   Due to timing issues, it might be that this warning
   is printed when the server shuts down and the
   computer is loaded.
 */

 ("Got error [0-9]* when reading table"),
 ("Incorrect definition of table"),
 ("Incorrect information in file"),
 ("InnoDB: Warning: we did not need to do crash recovery"),
 ("Invalid \\(old\\?\\) table or database name"),
 ("Lock wait timeout exceeded"),
 ("Log entry on master is longer than max_allowed_packet"),
 ("unknown option '--loose-"),
 ("unknown variable 'loose-"),
 ("You have forced lower_case_table_names to 0 through a command-line option"),
 ("Setting lower_case_table_names=2"),
 ("NDB Binlog:"),
 ("NDB: failed to setup table"),
 ("NDB: only row based binary logging"),
 ("Neither --relay-log nor --relay-log-index were used"),
 ("Query partially completed"),
 ("Slave I.O thread aborted while waiting for relay log"),
 ("Slave SQL thread is stopped because UNTIL condition"),
 ("Slave SQL thread retried transaction"),
 ("Slave \\(additional info\\)"),
 ("Slave: .*Duplicate column name"),
 ("Slave: .*master may suffer from"),
 ("Slave: According to the master's version"),
 ("Slave: Column [0-9]* type mismatch"),
 ("Slave: Error .* doesn't exist"),
 ("Slave: Error .*Unknown table"),
 ("Slave: Error in Write_rows event: "),
 ("Slave: Field .* of table .* has no default value"),
 ("Slave: Field .* doesn't have a default value"),
 ("Slave: Query caused different errors on master and slave"),
 ("Slave: Table .* doesn't exist"),
 ("Slave: Table width mismatch"),
 ("Slave: The incident LOST_EVENTS occured on the master"),
 ("Slave: Unknown error.* 1105"),
 ("Slave: Can't drop database.* database doesn't exist"),
 ("Slave SQL:.*(Error_code: \[\[:digit:\]\]+|Query:.*)"),
 ("Sort aborted"),
 ("Time-out in NDB"),
 ("Warning:\s+One can only use the --user.*root"),
 ("Warning:\s+Setting lower_case_table_names=2"),
 ("Warning:\s+Table:.* on (delete|rename)"),
 ("You have an error in your SQL syntax"),
 ("deprecated"),
 ("description of time zone"),
 ("equal MySQL server ids"),
 ("error .*connecting to master"),
 ("error reading log entry"),
 ("lower_case_table_names is set"),
 ("skip-name-resolve mode"),
 ("slave SQL thread aborted"),
 ("Slave: .*Duplicate entry"),

 /*
   Special case, made as specific as possible, for:
   Bug #28436: Incorrect position in SHOW BINLOG EVENTS causes
   server coredump
 */

 ("Error in Log_event::read_log_event\\\(\\\): 'Sanity check failed', data_len: 258, event_type: 49"),

 ("Statement may not be safe to log in statement format"),

 /* test case for Bug#bug29807 copies a stray frm into database */
 ("InnoDB: Error: table `test`.`bug29807` does not exist in the InnoDB internal"),
 ("Cannot find or open table test\/bug29807 from"),

 /* innodb foreign key tests that fail in ALTER or RENAME produce this */
 ("InnoDB: Error: in ALTER TABLE `test`.`t[123]`"),
 ("InnoDB: Error: in RENAME TABLE table `test`.`t1`"),
 ("InnoDB: Error: table `test`.`t[123]` does not exist in the InnoDB internal"),

 /* Test case for Bug#14233 produces the following warnings: */
 ("Stored routine 'test'.'bug14233_1': invalid value in column mysql.proc"),
 ("Stored routine 'test'.'bug14233_2': invalid value in column mysql.proc"),
 ("Stored routine 'test'.'bug14233_3': invalid value in column mysql.proc"),

 /*
   BUG#32080 - Excessive warnings on Solaris: setrlimit could not
   change the size of core files
  */
 ("setrlimit could not change the size of core files to 'infinity'"),

 /*
   rpl_extrColmaster_*.test, the slave thread produces warnings
   when it get updates to a table that has more columns on the
   master
  */
 ("Slave: Unknown column 'c7' in 't15' Error_code: 1054"),
 ("Slave: Can't DROP 'c7'.* 1091"),
 ("Slave: Key column 'c6'.* 1072"),
 ("The slave I.O thread stops because a fatal error is encountered when it try to get the value of SERVER_ID variable from master."),
 (".SELECT UNIX_TIMESTAMP... failed on master, do not trust column Seconds_Behind_Master of SHOW SLAVE STATUS"),

 /* Test case for Bug#31590 in order_by.test produces the following error */
 ("Out of sort memory; increase server sort buffer size"),

 /* Special case for Bug #26402 in show_check.test
    - Question marks are not valid file name parts on Windows. Ignore
      this error message.
  */
 ("Can't find file: '.\\\\test\\\\\\?{8}.frm'"),
 ("Slave: Unknown table 't1' Error_code: 1051"),

 ("THE_LAST_SUPPRESSION")||


--
-- Procedure that uses the above created tables to check
-- the servers error log for warnings
--
CREATE DEFINER=root@localhost PROCEDURE check_warnings(OUT result INT)
BEGIN
  DECLARE `pos` bigint unsigned;

  -- Don't write these queries to binlog
  SET SQL_LOG_BIN=0;

  --
  -- Remove mark from lines that are suppressed by global suppressions
  --
  UPDATE error_log el, global_suppressions gs
    SET suspicious=0
      WHERE el.suspicious=1 AND el.line REGEXP gs.pattern;

  --
  -- Remove mark from lines that are suppressed by test specific suppressions
  --
  UPDATE error_log el, test_suppressions ts
    SET suspicious=0
      WHERE el.suspicious=1 AND el.line REGEXP ts.pattern;

  --
  -- Get the number of marked lines and return result
  --
  SELECT COUNT(*) INTO @num_warnings FROM error_log
    WHERE suspicious=1;

  IF @num_warnings > 0 THEN
    SELECT line
        FROM error_log WHERE suspicious=1;
    --SELECT * FROM test_suppressions;
    -- Return 2 -> check failed
    SELECT 2 INTO result;
  ELSE
    -- Return 0 -> OK
    SELECT 0 INTO RESULT;
  END IF;

  -- Cleanup for next test
  TRUNCATE test_suppressions;
  DROP TABLE error_log;

END||

--
-- Declare a procedure testcases can use to insert test
-- specific suppressions
--
/*!50001
CREATE DEFINER=root@localhost
PROCEDURE add_suppression(pattern VARCHAR(255))
BEGIN
  INSERT INTO test_suppressions (pattern) VALUES (pattern);
END
*/||


