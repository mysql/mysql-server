#bug28940878
CREATE TABLE t1(d DATE);
INSERT INTO t1 VALUES('2011-02-20');
# In the default sql mode invalid dates should return errors
SELECT * FROM t1 WHERE d <= '2013-02-32';
ERROR HY000: Incorrect DATE value: '2013-02-32'
SELECT * FROM t1 WHERE d <= '2013-02-30';
ERROR HY000: Incorrect DATE value: '2013-02-30'
SELECT * FROM t1 WHERE d >= '0000-00-00';
ERROR HY000: Incorrect DATE value: '0000-00-00'
SELECT * FROM t1 WHERE d >= 'wrong-date';
ERROR HY000: Incorrect DATE value: 'wrong-date'
SET @old_sql_mode := @@sql_mode;
SET @@sql_mode = 'ALLOW_INVALID_DATES';
# In the 'ALLOW_INVALID_DATES' sql mode only the first two query should return error
SELECT * FROM t1 WHERE d <= '2013-02-32';
ERROR HY000: Incorrect DATE value: '2013-02-32'
SELECT * FROM t1 WHERE d >= 'wrong-date';
ERROR HY000: Incorrect DATE value: 'wrong-date'
SELECT * FROM t1 WHERE d <= '2013-02-30';
d
2011-02-20
SELECT * FROM t1 WHERE d >= '0000-00-00';
d
2011-02-20
SET @@sql_mode = @old_sql_mode;
DROP TABLE t1;
