--echo #
--echo # Bug #35380295: sql_mode='PAD_CHAR_TO_FULL_LENGTH' and
--echo #   'GRANT EXECUTE ON PROCEDURE' bring strange behavior.
--echo #

--source include/count_sessions.inc
CREATE DATABASE db35380295;
CREATE USER u1b35380295, u2b35380295;

GRANT ALL PRIVILEGES ON db35380295.* TO u1b35380295;

connect(conu1,localhost,u1b35380295);
USE db35380295;
CREATE TABLE t1(i1 INT NOT NULL PRIMARY KEY, v2 VARCHAR(20));

DELIMITER $;
CREATE PROCEDURE p35380295(v_max INT)
BEGIN
  DECLARE v_id INT DEFAULT 0;
  REPEAT
    SET v_id = v_id + 1;
    INSERT IGNORE INTO t1 VALUES(FLOOR(RAND()*100000000), v_id);
    IF (MOD(v_id,10000) = 0) THEN COMMIT;
    END IF;
  UNTIL v_id >= v_max
  END REPEAT;
END$
DELIMITER ;$

connection default;
disconnect conu1;
GRANT EXECUTE ON PROCEDURE db35380295.p35380295 TO u2b35380295;
--echo # test: should show the EXECUTE PRIV
--sorted_result
SHOW GRANTS FOR u2b35380295;
FLUSH PRIVILEGES;
--echo # test: should be the same as the previous SHOW
--sorted_result
SHOW GRANTS FOR u2b35380295;

connect(conu2,localhost,u2b35380295);
--echo # Test: should succeed
CALL db35380295.p35380295(10);
connection default;
disconnect conu2;

--echo # cleanup
--source include/wait_until_count_sessions.inc
REVOKE EXECUTE ON PROCEDURE db35380295.p35380295 FROM u2b35380295;
DROP PROCEDURE db35380295.p35380295;
DROP TABLE db35380295.t1;
REVOKE ALL PRIVILEGES ON db35380295.* FROM u1b35380295;
DROP USER u1b35380295, u2b35380295;
DROP DATABASE db35380295;


--echo # End of 8.0 tests
