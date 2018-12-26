 #Get deafult engine value
--let $DEFAULT_ENGINE = `select @@global.default_storage_engine`

#
# Test of ansi mode
#

--disable_warnings
drop table if exists t1;
--enable_warnings

set @@sql_mode="ANSI";
select @@sql_mode;

# Test some functions that works different in ansi mode

SELECT 'A' || 'B';

# Test GROUP BY behaviour

CREATE TABLE t1 (id INT, id2 int);
SELECT id,NULL,1,1.1,'a' FROM t1 GROUP BY id;
# ONLY_FULL_GROUP_BY is included in ANSI:
--error ER_WRONG_FIELD_WITH_GROUP
SELECT id FROM t1 GROUP BY id2;
drop table t1;

SET @@SQL_MODE="";
