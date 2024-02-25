--echo #bug28940878
CREATE TABLE t1(d DATE);
INSERT INTO t1 VALUES('2011-02-20');

--echo # In the default sql mode invalid dates should return errors
--error ER_WRONG_VALUE
SELECT * FROM t1 WHERE d <= '2013-02-32';
--error ER_WRONG_VALUE
SELECT * FROM t1 WHERE d <= '2013-02-30';
--error ER_WRONG_VALUE
SELECT * FROM t1 WHERE d >= '0000-00-00';
--error ER_WRONG_VALUE
SELECT * FROM t1 WHERE d >= 'wrong-date';

SET @old_sql_mode := @@sql_mode;
SET @@sql_mode = 'ALLOW_INVALID_DATES';

--echo # In the 'ALLOW_INVALID_DATES' sql mode only the first two query should return error
--error ER_WRONG_VALUE
SELECT * FROM t1 WHERE d <= '2013-02-32';
--error ER_WRONG_VALUE
SELECT * FROM t1 WHERE d >= 'wrong-date';

SELECT * FROM t1 WHERE d <= '2013-02-30';
SELECT * FROM t1 WHERE d >= '0000-00-00';

SET @@sql_mode = @old_sql_mode;

DROP TABLE t1;

