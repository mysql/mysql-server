#
# This file contains tests covering the parser
#

#=============================================================================
# LEXICAL PARSER (lex)
#=============================================================================

#
# Maintainer: these tests are for the lexical parser, so every character,
# even whitespace or comments, is significant here.
#

SET @save_sql_mode=@@sql_mode;

#
# Documenting the current behavior, to detect incompatible changes.
# In each cases:
# - no error is the correct result
# - an error is the expected result with the current implementation,
#   and is a limitation.

set SQL_MODE='';

create table ADDDATE(a int);
drop table ADDDATE;
create table ADDDATE (a int);
drop table ADDDATE;

--error ER_PARSE_ERROR
create table BIT_AND(a int);
create table BIT_AND (a int);
drop table BIT_AND;

--error ER_PARSE_ERROR
create table BIT_OR(a int);
create table BIT_OR (a int);
drop table BIT_OR;

--error ER_PARSE_ERROR
create table BIT_XOR(a int);
create table BIT_XOR (a int);
drop table BIT_XOR;

--error ER_PARSE_ERROR
create table CAST(a int);
create table CAST (a int);
drop table CAST;

--error ER_PARSE_ERROR
create table COUNT(a int);
create table COUNT (a int);
drop table COUNT;

--error ER_PARSE_ERROR
create table CURDATE(a int);
create table CURDATE (a int);
drop table CURDATE;

--error ER_PARSE_ERROR
create table CURTIME(a int);
create table CURTIME (a int);
drop table CURTIME;

--error ER_PARSE_ERROR
create table DATE_ADD(a int);
create table DATE_ADD (a int);
drop table DATE_ADD;

--error ER_PARSE_ERROR
create table DATE_SUB(a int);
create table DATE_SUB (a int);
drop table DATE_SUB;

--error ER_PARSE_ERROR
create table EXTRACT(a int);
create table EXTRACT (a int);
drop table EXTRACT;

--error ER_PARSE_ERROR
create table GROUP_CONCAT(a int);
create table GROUP_CONCAT (a int);
drop table GROUP_CONCAT;

# Limitation removed in 5.1
create table GROUP_UNIQUE_USERS(a int);
drop table GROUP_UNIQUE_USERS;
create table GROUP_UNIQUE_USERS (a int);
drop table GROUP_UNIQUE_USERS;

--error ER_PARSE_ERROR
create table MAX(a int);
create table MAX (a int);
drop table MAX;

--error ER_PARSE_ERROR
create table MID(a int);
create table MID (a int);
drop table MID;

--error ER_PARSE_ERROR
create table MIN(a int);
create table MIN (a int);
drop table MIN;

--error ER_PARSE_ERROR
create table NOW(a int);
create table NOW (a int);
drop table NOW;

--error ER_PARSE_ERROR
create table POSITION(a int);
create table POSITION (a int);
drop table POSITION;

create table SESSION_USER(a int);
drop table SESSION_USER;
create table SESSION_USER (a int);
drop table SESSION_USER;

--error ER_PARSE_ERROR
create table STD(a int);
create table STD (a int);
drop table STD;

--error ER_PARSE_ERROR
create table STDDEV(a int);
create table STDDEV (a int);
drop table STDDEV;

--error ER_PARSE_ERROR
create table STDDEV_POP(a int);
create table STDDEV_POP (a int);
drop table STDDEV_POP;

--error ER_PARSE_ERROR
create table STDDEV_SAMP(a int);
create table STDDEV_SAMP (a int);
drop table STDDEV_SAMP;

create table SUBDATE(a int);
drop table SUBDATE;
create table SUBDATE (a int);
drop table SUBDATE;

--error ER_PARSE_ERROR
create table SUBSTR(a int);
create table SUBSTR (a int);
drop table SUBSTR;

--error ER_PARSE_ERROR
create table SUBSTRING(a int);
create table SUBSTRING (a int);
drop table SUBSTRING;

--error ER_PARSE_ERROR
create table SUM(a int);
create table SUM (a int);
drop table SUM;

--error ER_PARSE_ERROR
create table SYSDATE(a int);
create table SYSDATE (a int);
drop table SYSDATE;

create table SYSTEM_USER(a int);
drop table SYSTEM_USER;
create table SYSTEM_USER (a int);
drop table SYSTEM_USER;

--error ER_PARSE_ERROR
create table TRIM(a int);
create table TRIM (a int);
drop table TRIM;

# Limitation removed in 5.1
create table UNIQUE_USERS(a int);
drop table UNIQUE_USERS;
create table UNIQUE_USERS (a int);
drop table UNIQUE_USERS;

--error ER_PARSE_ERROR
create table VARIANCE(a int);
create table VARIANCE (a int);
drop table VARIANCE;

--error ER_PARSE_ERROR
create table VAR_POP(a int);
create table VAR_POP (a int);
drop table VAR_POP;

--error ER_PARSE_ERROR
create table VAR_SAMP(a int);
create table VAR_SAMP (a int);
drop table VAR_SAMP;

set SQL_MODE='IGNORE_SPACE';

create table ADDDATE(a int);
drop table ADDDATE;
create table ADDDATE (a int);
drop table ADDDATE;

--error ER_PARSE_ERROR
create table BIT_AND(a int);
--error ER_PARSE_ERROR
create table BIT_AND (a int);

--error ER_PARSE_ERROR
create table BIT_OR(a int);
--error ER_PARSE_ERROR
create table BIT_OR (a int);

--error ER_PARSE_ERROR
create table BIT_XOR(a int);
--error ER_PARSE_ERROR
create table BIT_XOR (a int);

--error ER_PARSE_ERROR
create table CAST(a int);
--error ER_PARSE_ERROR
create table CAST (a int);

--error ER_PARSE_ERROR
create table COUNT(a int);
--error ER_PARSE_ERROR
create table COUNT (a int);

--error ER_PARSE_ERROR
create table CURDATE(a int);
--error ER_PARSE_ERROR
create table CURDATE (a int);

--error ER_PARSE_ERROR
create table CURTIME(a int);
--error ER_PARSE_ERROR
create table CURTIME (a int);

--error ER_PARSE_ERROR
create table DATE_ADD(a int);
--error ER_PARSE_ERROR
create table DATE_ADD (a int);

--error ER_PARSE_ERROR
create table DATE_SUB(a int);
--error ER_PARSE_ERROR
create table DATE_SUB (a int);

--error ER_PARSE_ERROR
create table EXTRACT(a int);
--error ER_PARSE_ERROR
create table EXTRACT (a int);

--error ER_PARSE_ERROR
create table GROUP_CONCAT(a int);
--error ER_PARSE_ERROR
create table GROUP_CONCAT (a int);

# Limitation removed in 5.1
create table GROUP_UNIQUE_USERS(a int);
drop table GROUP_UNIQUE_USERS;
create table GROUP_UNIQUE_USERS (a int);
drop table GROUP_UNIQUE_USERS;

--error ER_PARSE_ERROR
create table MAX(a int);
--error ER_PARSE_ERROR
create table MAX (a int);

--error ER_PARSE_ERROR
create table MID(a int);
--error ER_PARSE_ERROR
create table MID (a int);

--error ER_PARSE_ERROR
create table MIN(a int);
--error ER_PARSE_ERROR
create table MIN (a int);

--error ER_PARSE_ERROR
create table NOW(a int);
--error ER_PARSE_ERROR
create table NOW (a int);

--error ER_PARSE_ERROR
create table POSITION(a int);
--error ER_PARSE_ERROR
create table POSITION (a int);

create table SESSION_USER(a int);
drop table SESSION_USER;
create table SESSION_USER (a int);
drop table SESSION_USER;

--error ER_PARSE_ERROR
create table STD(a int);
--error ER_PARSE_ERROR
create table STD (a int);

--error ER_PARSE_ERROR
create table STDDEV(a int);
--error ER_PARSE_ERROR
create table STDDEV (a int);

--error ER_PARSE_ERROR
create table STDDEV_POP(a int);
--error ER_PARSE_ERROR
create table STDDEV_POP (a int);

--error ER_PARSE_ERROR
create table STDDEV_SAMP(a int);
--error ER_PARSE_ERROR
create table STDDEV_SAMP (a int);

create table SUBDATE(a int);
drop table SUBDATE;
create table SUBDATE (a int);
drop table SUBDATE;

--error ER_PARSE_ERROR
create table SUBSTR(a int);
--error ER_PARSE_ERROR
create table SUBSTR (a int);

--error ER_PARSE_ERROR
create table SUBSTRING(a int);
--error ER_PARSE_ERROR
create table SUBSTRING (a int);

--error ER_PARSE_ERROR
create table SUM(a int);
--error ER_PARSE_ERROR
create table SUM (a int);

--error ER_PARSE_ERROR
create table SYSDATE(a int);
--error ER_PARSE_ERROR
create table SYSDATE (a int);

create table SYSTEM_USER(a int);
drop table SYSTEM_USER;
create table SYSTEM_USER (a int);
drop table SYSTEM_USER;

--error ER_PARSE_ERROR
create table TRIM(a int);
--error ER_PARSE_ERROR
create table TRIM (a int);

# Limitation removed in 5.1
create table UNIQUE_USERS(a int);
drop table UNIQUE_USERS;
create table UNIQUE_USERS (a int);
drop table UNIQUE_USERS;

--error ER_PARSE_ERROR
create table VARIANCE(a int);
--error ER_PARSE_ERROR
create table VARIANCE (a int);

--error ER_PARSE_ERROR
create table VAR_POP(a int);
--error ER_PARSE_ERROR
create table VAR_POP (a int);

--error ER_PARSE_ERROR
create table VAR_SAMP(a int);
--error ER_PARSE_ERROR
create table VAR_SAMP (a int);

--echo #
--echo # Test "UNIQUE KEY" and "UNIQUE" "KEY" grammar ambiguity
--echo #

CREATE TABLE t1 (i INT KEY);
SHOW CREATE TABLE t1;

CREATE TABLE t2 (i INT UNIQUE);
SHOW CREATE TABLE t2;

CREATE TABLE t3 (i INT UNIQUE KEY);
--echo # Should output "UNIQUE KEY `i` (`i`)" only:
SHOW CREATE TABLE t3;

DROP TABLE t1, t2, t3;

--echo #

#
# Bug#25930 (CREATE TABLE x SELECT ... parses columns wrong when ran with
#            ANSI_QUOTES mode)
#

--disable_warnings
DROP TABLE IF EXISTS table_25930_a;
DROP TABLE IF EXISTS table_25930_b;
--enable_warnings

SET SQL_MODE = 'ANSI_QUOTES';
CREATE TABLE table_25930_a ( "blah" INT );
CREATE TABLE table_25930_b SELECT "blah" - 1 FROM table_25930_a;

# The lexer used to chop the first <">,
# not marking the start of the token "blah" correctly.
desc table_25930_b;

DROP TABLE table_25930_a;
DROP TABLE table_25930_b;


SET @@sql_mode=@save_sql_mode;

#
# Bug#26030 (Parsing fails for stored routine w/multi-statement execution
# enabled)
#

--disable_warnings
DROP PROCEDURE IF EXISTS p26030;
--enable_warnings

delimiter $$;

select "non terminated"$$
select "terminated";$$
select "non terminated, space"      $$
select "terminated, space";      $$
select "non terminated, comment" /* comment */$$
select "terminated, comment"; /* comment */$$

select "stmt 1";select "stmt 2 non terminated"$$
select "stmt 1";select "stmt 2 terminated";$$
select "stmt 1";select "stmt 2 non terminated, space"      $$
select "stmt 1";select "stmt 2 terminated, space";      $$
select "stmt 1";select "stmt 2 non terminated, comment" /* comment */$$
select "stmt 1";select "stmt 2 terminated, comment"; /* comment */$$

select "stmt 1";             select "space, stmt 2"$$
select "stmt 1";/* comment */select "comment, stmt 2"$$

DROP PROCEDURE IF EXISTS p26030; CREATE PROCEDURE p26030() BEGIN SELECT 1; END; CALL p26030()
$$

DROP PROCEDURE IF EXISTS p26030; CREATE PROCEDURE p26030() SELECT 1; CALL p26030()
$$

delimiter ;$$
DROP PROCEDURE p26030;

#=============================================================================
# SYNTACTIC PARSER (bison)
#=============================================================================

#
#
# Bug#21114 (Foreign key creation fails to table with name format)
# 

# Test coverage with edge conditions

-- error ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT
select pi(3.14);

-- error ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT
select tan();
-- error ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT
select tan(1, 2);

-- error ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT
select makedate(1);
-- error ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT
select makedate(1, 2, 3);

-- error ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT
select maketime();
-- error ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT
select maketime(1);
-- error ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT
select maketime(1, 2);
-- error ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT
select maketime(1, 2, 3, 4);

-- error ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT
select atan();
-- error ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT
select atan2(1, 2, 3);

-- error ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT
select concat();
select concat("foo");

-- error ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT
select concat_ws();
-- error ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT
select concat_ws("foo");

-- error ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT
select elt();
-- error ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT
select elt(1);

-- error ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT
select export_set();
-- error ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT
select export_set("p1");
-- error ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT
select export_set("p1", "p2");
-- error ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT
select export_set("p1", "p2", "p3", "p4", "p5", "p6");

-- error ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT
select field();
-- error ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT
select field("p1");

-- error ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT
select from_unixtime();
-- error ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT
select from_unixtime(1, 2, 3);

-- error ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT
select unix_timestamp(1, 2);

-- error ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT
select greatest();
-- error ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT
select greatest(12);

-- error ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT
select last_insert_id(1, 2);

-- error ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT
select least();
-- error ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT
select least(12);

-- error ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT
select locate();
-- error ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT
select locate(1);
-- error ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT
select locate(1, 2, 3, 4);

-- error ER_PARSE_ERROR
select log();
-- error ER_PARSE_ERROR
select log(1, 2, 3);

-- error ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT
select make_set();
-- error ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT
select make_set(1);

-- error ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT
select source_pos_wait();
-- error ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT
select source_pos_wait(1);

--disable_result_log
--error 0,ER_WRONG_ARGUMENTS
select source_pos_wait('binlog.999999', 4, -1);
--enable_result_log

-- error ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT
select rand(1, 2, 3);

-- error ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT
select round(1, 2, 3);

-- error ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT
select yearweek();
-- error ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT
select yearweek(1, 2, 3);

#
# Bug#24736: UDF functions parsed as Stored Functions
#

# Verify that the syntax for calling UDF : foo(expr AS param, ...)
# can not be used when calling native functions

# Native function with 1 argument

select abs(3);
-- error ER_WRONG_PARAMETERS_TO_NATIVE_FCT
select abs(3 AS three);
-- error ER_WRONG_PARAMETERS_TO_NATIVE_FCT
select abs(3 three);
-- error ER_WRONG_PARAMETERS_TO_NATIVE_FCT
select abs(3 AS "three");
-- error ER_WRONG_PARAMETERS_TO_NATIVE_FCT
select abs(3 "three");

# Native function with 2 arguments

set @bar="bar";
set @foobar="foobar";

select instr("foobar", "bar");
-- error ER_WRONG_PARAMETERS_TO_NATIVE_FCT
select instr("foobar" AS p1, "bar");
-- error ER_WRONG_PARAMETERS_TO_NATIVE_FCT
select instr("foobar" p1, "bar");
-- error ER_WRONG_PARAMETERS_TO_NATIVE_FCT
select instr("foobar" AS "p1", "bar");
## String concatenation, valid syntax
select instr("foobar" "p1", "bar");
-- error ER_WRONG_PARAMETERS_TO_NATIVE_FCT
select instr(@foobar "p1", "bar");
-- error ER_WRONG_PARAMETERS_TO_NATIVE_FCT
select instr("foobar", "bar" AS p2);
-- error ER_WRONG_PARAMETERS_TO_NATIVE_FCT
select instr("foobar", "bar" p2);
-- error ER_WRONG_PARAMETERS_TO_NATIVE_FCT
select instr("foobar", "bar" AS "p2");
## String concatenation, valid syntax
select instr("foobar", "bar" "p2");
-- error ER_WRONG_PARAMETERS_TO_NATIVE_FCT
select instr("foobar", @bar "p2");
-- error ER_WRONG_PARAMETERS_TO_NATIVE_FCT
select instr("foobar" AS p1, "bar" AS p2);

# Native function with 3 arguments

select conv(255, 10, 16);
-- error ER_WRONG_PARAMETERS_TO_NATIVE_FCT
select conv(255 AS p1, 10, 16);
-- error ER_WRONG_PARAMETERS_TO_NATIVE_FCT
select conv(255 p1, 10, 16);
-- error ER_WRONG_PARAMETERS_TO_NATIVE_FCT
select conv(255 AS "p1", 10, 16);
-- error ER_WRONG_PARAMETERS_TO_NATIVE_FCT
select conv(255 "p1", 10, 16);
-- error ER_WRONG_PARAMETERS_TO_NATIVE_FCT
select conv(255, 10 AS p2, 16);
-- error ER_WRONG_PARAMETERS_TO_NATIVE_FCT
select conv(255, 10 p2, 16);
-- error ER_WRONG_PARAMETERS_TO_NATIVE_FCT
select conv(255, 10 AS "p2", 16);
-- error ER_WRONG_PARAMETERS_TO_NATIVE_FCT
select conv(255, 10 "p2", 16);
-- error ER_WRONG_PARAMETERS_TO_NATIVE_FCT
select conv(255, 10, 16 AS p3);
-- error ER_WRONG_PARAMETERS_TO_NATIVE_FCT
select conv(255, 10, 16 p3);
-- error ER_WRONG_PARAMETERS_TO_NATIVE_FCT
select conv(255, 10, 16 AS "p3");
-- error ER_WRONG_PARAMETERS_TO_NATIVE_FCT
select conv(255, 10, 16 "p3");
-- error ER_WRONG_PARAMETERS_TO_NATIVE_FCT
select conv(255 AS p1, 10 AS p2, 16 AS p3);

# Native function with a variable number of arguments

# Bug in libm.so on Solaris:
#   atan(10) from 32-bit version returns 1.4711276743037347
#   atan(10) from 64-bit version returns 1.4711276743037345
--replace_result 1.4711276743037345 1.4711276743037347
select atan(10);
-- error ER_WRONG_PARAMETERS_TO_NATIVE_FCT
select atan(10 AS p1);
-- error ER_WRONG_PARAMETERS_TO_NATIVE_FCT
select atan(10 p1);
-- error ER_WRONG_PARAMETERS_TO_NATIVE_FCT
select atan(10 AS "p1");
-- error ER_WRONG_PARAMETERS_TO_NATIVE_FCT
select atan(10 "p1");

select atan(10, 20);
-- error ER_WRONG_PARAMETERS_TO_NATIVE_FCT
select atan(10 AS p1, 20);
-- error ER_WRONG_PARAMETERS_TO_NATIVE_FCT
select atan(10 p1, 20);
-- error ER_WRONG_PARAMETERS_TO_NATIVE_FCT
select atan(10 AS "p1", 20);
-- error ER_WRONG_PARAMETERS_TO_NATIVE_FCT
select atan(10 "p1", 20);
-- error ER_WRONG_PARAMETERS_TO_NATIVE_FCT
select atan(10, 20 AS p2);
-- error ER_WRONG_PARAMETERS_TO_NATIVE_FCT
select atan(10, 20 p2);
-- error ER_WRONG_PARAMETERS_TO_NATIVE_FCT
select atan(10, 20 AS "p2");
-- error ER_WRONG_PARAMETERS_TO_NATIVE_FCT
select atan(10, 20 "p2");
-- error ER_WRONG_PARAMETERS_TO_NATIVE_FCT
select atan(10 AS p1, 20 AS p2);

#
# Bug#22312 Syntax error in expression with INTERVAL()
#

--disable_warnings
DROP TABLE IF EXISTS t1;
--enable_warnings
SET sql_mode = 'NO_ENGINE_SUBSTITUTION';
SELECT STR_TO_DATE('10:00 PM', '%h:%i %p') + INTERVAL 10 MINUTE;
SELECT STR_TO_DATE('10:00 PM', '%h:%i %p') + INTERVAL (INTERVAL(1,2,3) + 1) MINUTE;
SELECT "1997-12-31 23:59:59" + INTERVAL 1 SECOND;
SELECT 1 + INTERVAL(1,0,1,2) + 1;
SELECT INTERVAL(1^1,0,1,2) + 1;
SELECT INTERVAL(1,0+1,2,3) * 5.5;
SELECT INTERVAL(3,3,1+3,4+4) / 0.5;
SELECT (INTERVAL(1,0,1,2) + 5) * 7 + INTERVAL(1,0,1,2) / 2;
SELECT INTERVAL(1,0,1,2) + 1, 5 * INTERVAL(1,0,1,2);
SELECT INTERVAL(0,(1*5)/2) + INTERVAL(5,4,3);

--disable_warnings
SELECT 1^1 + INTERVAL 1+1 SECOND & 1 + INTERVAL 1+1 SECOND;
SELECT 1%2 - INTERVAL 1^1 SECOND | 1%2 - INTERVAL 1^1 SECOND;
--enable_warnings

CREATE TABLE t1 (a INT, b DATETIME);
INSERT INTO t1 VALUES (INTERVAL(3,2,1) + 1, "1997-12-31 23:59:59" + INTERVAL 1 SECOND);
SELECT * FROM t1 WHERE a = INTERVAL(3,2,1) + 1;
DROP TABLE t1;
SET sql_mode = default;
#
# Bug#28317 Left Outer Join with {oj outer-join}
#

--disable_warnings
DROP TABLE IF EXISTS t1,t2,t3;
--enable_warnings
CREATE TABLE t1 (a1 INT, a2 INT, a3 INT, a4 DATETIME);
CREATE TABLE t2 LIKE t1;
CREATE TABLE t3 LIKE t1;
SELECT t1.* FROM t1 AS t0, { OJ t2 INNER JOIN t1 ON (t1.a1=t2.a1) } WHERE t0.a3=2;
SELECT t1.*,t2.* FROM { OJ ((t1 INNER JOIN t2 ON (t1.a1=t2.a2)) LEFT OUTER JOIN t3 ON t3.a3=t2.a1)};
SELECT t1.*,t2.* FROM { OJ ((t1 LEFT OUTER JOIN t2 ON t1.a3=t2.a2) INNER JOIN t3 ON (t3.a1=t2.a2))};
SELECT t1.*,t2.* FROM { OJ (t1 LEFT OUTER JOIN t2 ON t1.a1=t2.a2) CROSS JOIN t3 ON (t3.a2=t2.a3)};
SELECT * FROM {oj t1 LEFT OUTER JOIN t2 ON t1.a1=t2.a3} WHERE t1.a2 > 10;
SELECT {fn CONCAT(a1,a2)} FROM t1;
UPDATE t3 SET a4={d '1789-07-14'} WHERE a1=0;
SELECT a1, a4 FROM t2 WHERE a4 LIKE {fn UCASE('1789-07-14')};
DROP TABLE t1, t2, t3;

--echo #
--echo # End of 5.1 tests
--echo #

#
# Bug#12546960 - 60993: NAME QUOTED WITH QUOTE INSTEAD OF BACKTICK 
#                       GIVES NO SYNTAX ERROR
#
CREATE TABLE t (id INT PRIMARY KEY);
--error ER_PARSE_ERROR
--query ALTER TABLE t RENAME TO `t1';
DROP TABLE t;

--echo #
--echo # Bug#13819100 BROKEN SYNTAX ACCEPTED FOR START SLAVE, STOP SLAVE
--echo #

--error ER_PARSE_ERROR
STOP REPLICA ,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,, ;
--error ER_PARSE_ERROR
STOP REPLICA ,,,,,,,,,,,,, sql_thread, ,,,,,,,,,,,,,,,,,,, ;
--error ER_PARSE_ERROR
STOP REPLICA ,,,,,,,,,,,,, io_thread, ,,,,,,,,,,,,,,,,,,, ;

--echo #
--echo # Bug#13819132 BROKEN SYNTAX ACCEPTED FOR START TRANSACTION
--echo #

--error ER_PARSE_ERROR
START TRANSACTION ,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,, ;

--echo #
--echo # Test of collective fix for three parser bugs:
--echo #
--echo # Bug #17727401, Bug #17426017, Bug #17473479:
--echo #   The server accepts wrong syntax and then fails in different ways
--echo #

CREATE TABLE t1 (i INT);

--echo # bug #17426017
--error ER_PARSE_ERROR
SELECT (SELECT EXISTS(SELECT * LIMIT 1 ORDER BY VALUES (c00)));

--echo # bug#17473479
CREATE TABLE a(a int);
CREATE TABLE b(a int);
--error ER_PARSE_ERROR
DELETE FROM b ORDER BY(SELECT 1 FROM a ORDER BY a ORDER BY a);
DROP TABLE a, b;

--echo # bug #17727401
--error ER_PARSE_ERROR
SELECT '' IN (SELECT '1' c FROM t1 ORDER BY '' ORDER BY '') FROM t1;

--echo # regression & coverage tests

--echo # uniform syntax for FROM DUAL clause:

SELECT 1 FROM DUAL WHERE 1 GROUP BY 1 HAVING 1 ORDER BY 1
  FOR UPDATE;

SELECT 1 FROM
  (SELECT 1 FROM DUAL WHERE 1 GROUP BY 1 HAVING 1 ORDER BY 1
   FOR UPDATE) a;

SELECT 1 FROM t1
  WHERE EXISTS(SELECT 1 FROM DUAL WHERE 1 GROUP BY 1 HAVING 1 ORDER BY 1
               FOR UPDATE);

SELECT 1 FROM t1
UNION
SELECT 1 FROM DUAL WHERE 1 GROUP BY 1 HAVING 1 ORDER BY 1
  FOR UPDATE;

(SELECT 1 FROM t1)
UNION 
(SELECT 1 FROM DUAL WHERE 1 GROUP BY 1 HAVING 1 ORDER BY 1
 FOR UPDATE);

--echo # "FOR UPDATE" tests

SELECT 1 FROM t1 UNION SELECT 1 FROM t1 ORDER BY 1 LIMIT 1;
--error ER_PARSE_ERROR
SELECT 1 FROM t1 FOR UPDATE UNION SELECT 1 FROM t1 ORDER BY 1 LIMIT 1;
SELECT 1 FROM t1 UNION SELECT 1 FROM t1 ORDER BY 1 LIMIT 1 FOR UPDATE;


--echo # "INTO" clause tests

SELECT 1 FROM t1 INTO @var17727401;
SELECT 1 FROM DUAL INTO @var17727401;
SELECT 1 INTO @var17727401;

SELECT 1 INTO @var17727401 FROM t1;
SELECT 1 INTO @var17727401 FROM DUAL;

# Double "INTO" clause: parse error is "near 'INTO @var17727401_2' at line 1"
--error ER_MULTIPLE_INTO_CLAUSES
SELECT 1 INTO @var17727401_1 FROM t1 INTO @var17727401_2;

# Double "INTO" clause: parse error is "near 'INTO @var17727401_2' at line 2"
--error ER_MULTIPLE_INTO_CLAUSES
SELECT 1 INTO @var17727401_1 FROM DUAL
  INTO @var17727401_2;

SELECT 1 INTO @var17727401 FROM t1 WHERE 1 GROUP BY 1 HAVING 1 ORDER BY 1 LIMIT 1;
SELECT 1 FROM t1 WHERE 1 GROUP BY 1 HAVING 1 ORDER BY 1 LIMIT 1 INTO @var17727401;

--error ER_PARSE_ERROR
SELECT 1 FROM t1 WHERE 1 INTO @var17727401 GROUP BY 1 HAVING 1 ORDER BY 1 LIMIT 1;

--error ER_MULTIPLE_INTO_CLAUSES
SELECT 1 INTO @var17727401_1
  FROM t1 WHERE 1 GROUP BY 1 HAVING 1 ORDER BY 1 LIMIT 1
  INTO @var17727401_2;

--error ER_PARSE_ERROR
SELECT (SELECT 1 FROM t1 INTO @var17727401);
--error ER_PARSE_ERROR
SELECT 1 FROM (SELECT 1 FROM t1 INTO @var17727401) a;
--error ER_PARSE_ERROR
SELECT EXISTS(SELECT 1 FROM t1 INTO @var17727401);

--error ER_PARSE_ERROR
SELECT 1 FROM t1 INTO @var17727401 UNION SELECT 1 FROM t1 INTO t1;
--error ER_PARSE_ERROR
(SELECT 1 FROM t1 INTO @var17727401) UNION (SELECT 1 FROM t1 INTO t1);

SELECT 1 FROM t1 UNION SELECT 1 FROM t1 INTO @var17727401;

--echo # ORDER and LIMIT clause combinations

(SELECT 1 FROM t1 ORDER BY 1) ORDER BY 1;
(SELECT 1 FROM t1 LIMIT 1) LIMIT 1;

((SELECT 1 FROM t1 ORDER BY 1) ORDER BY 1) ORDER BY 1;
((SELECT 1 FROM t1 LIMIT 1) LIMIT 1) LIMIT 1;

(SELECT 1 FROM t1 ORDER BY 1) LIMIT 1;
(SELECT 1 FROM t1 LIMIT 1) ORDER BY 1;

--error ER_PARSE_ERROR
((SELECT 1 FROM t1 ORDER BY 1) LIMIT 1) ORDER BY 1);
--error ER_PARSE_ERROR
((SELECT 1 FROM t1 LIMIT 1) ORDER BY 1) LIMIT 1);

# ORDER/LIMIT and UNION:

let $q=SELECT 1 FROM t1 UNION SELECT 1 FROM t1 ORDER BY 1;
eval $q;
eval SELECT ($q);
eval SELECT 1 FROM ($q) a;

let $q=SELECT 1 FROM t1 UNION SELECT 1 FROM t1 LIMIT 1;
eval $q;
eval SELECT ($q);
eval SELECT 1 FROM ($q) a;

let $q=SELECT 1 FROM t1 UNION SELECT 1 FROM t1 ORDER BY 1 LIMIT 1;
eval $q;
eval SELECT ($q);
eval SELECT 1 FROM ($q) a;

let $q=SELECT 1 FROM t1 UNION SELECT 1 FROM t1 LIMIT 1 ORDER BY 1;
--error ER_PARSE_ERROR
eval $q;
--error ER_PARSE_ERROR
eval SELECT ($q);
--error ER_PARSE_ERROR
eval SELECT 1 FROM ($q) a;

let $q=SELECT 1 FROM t1 ORDER BY 1 UNION SELECT 1 FROM t1;
--error ER_PARSE_ERROR
eval $q;
--error ER_PARSE_ERROR
eval SELECT ($q);
--error ER_PARSE_ERROR
eval SELECT 1 FROM ($q) a;

let $q=SELECT 1 FROM t1 LIMIT 1 UNION SELECT 1 FROM t1;
--error ER_PARSE_ERROR
eval $q;
--error ER_PARSE_ERROR
eval SELECT ($q);
--error ER_PARSE_ERROR
eval SELECT 1 FROM ($q) a;

let $q=SELECT 1 FROM t1 ORDER BY 1 LIMIT 1 UNION SELECT 1 FROM t1;
--error ER_PARSE_ERROR
eval $q;
--error ER_PARSE_ERROR
eval SELECT ($q);
--error ER_PARSE_ERROR
eval SELECT 1 FROM ($q) a;

let $q=SELECT 1 FROM t1 LIMIT 1 ORDER BY 1 UNION SELECT 1 FROM t1;
--error ER_PARSE_ERROR
eval $q;
--error ER_PARSE_ERROR
eval SELECT ($q);
--error ER_PARSE_ERROR
eval SELECT 1 FROM ($q) a;

let $q=SELECT 1 FROM t1 ORDER BY 1 UNION SELECT 1 FROM t1 ORDER BY 1;
--error ER_PARSE_ERROR
eval $q;
--error ER_PARSE_ERROR
eval SELECT ($q);
--error ER_PARSE_ERROR
eval SELECT 1 FROM ($q) a;

let $q=SELECT 1 FROM t1 LIMIT 1 UNION SELECT 1 FROM t1 LIMIT 1;
--error ER_PARSE_ERROR
eval $q;
--error ER_PARSE_ERROR
eval SELECT ($q);
--error ER_PARSE_ERROR
eval SELECT 1 FROM ($q) a;

let $q=SELECT 1 FROM t1 LIMIT 1 UNION SELECT 1 FROM t1 ORDER BY 1;
--error ER_PARSE_ERROR
eval $q;
--error ER_PARSE_ERROR
eval SELECT ($q);
--error ER_PARSE_ERROR
eval SELECT 1 FROM ($q) a;

let $q=SELECT 1 FROM t1 ORDER BY 1 UNION SELECT 1 FROM t1 LIMIT 1;
--error ER_PARSE_ERROR
eval $q;
--error ER_PARSE_ERROR
eval SELECT ($q);
--error ER_PARSE_ERROR
eval SELECT 1 FROM ($q) a;

DROP TABLE t1;

--echo #
--echo # Bug #18106014: RECENT REGRESSION: MORE CASES OF ASSERTION FAILED:
--echo #                !JOIN->PLAN_IS_CONST()
--echo #

SELECT COUNT(1) FROM DUAL GROUP BY '1' ORDER BY 1 ;
SELECT COUNT(1)           GROUP BY '1' ORDER BY 1 ;

DO(SELECT 1 c           GROUP BY 1 HAVING 1 ORDER BY COUNT(1));
DO(SELECT 1 c FROM DUAL GROUP BY 1 HAVING 1 ORDER BY COUNT(1));

SELECT (SELECT 1 c                   GROUP BY 1 HAVING 1 ORDER BY COUNT(1)) AS
  'null is not expected';
SELECT (SELECT 1 c FROM DUAL         GROUP BY 1 HAVING 1 ORDER BY COUNT(1)) AS
  'null is not expected';
SELECT (SELECT 1 c                   GROUP BY 1 HAVING 0 ORDER BY COUNT(1)) AS
  'null is expected';
SELECT (SELECT 1 c FROM DUAL         GROUP BY 1 HAVING 0 ORDER BY COUNT(1)) AS
  'null is expected';

SELECT (SELECT 1 c           WHERE 1 GROUP BY 1 HAVING 1 ORDER BY COUNT(1)) AS
  'null is not expected';
SELECT (SELECT 1 c FROM DUAL WHERE 1 GROUP BY 1 HAVING 1 ORDER BY COUNT(1)) AS
  'null is not expected';
SELECT (SELECT 1 c           WHERE 1 GROUP BY 1 HAVING 0 ORDER BY COUNT(1)) AS
  'null is expected';
SELECT (SELECT 1 c FROM DUAL WHERE 1 GROUP BY 1 HAVING 0 ORDER BY COUNT(1)) AS
  'null is expected';

SELECT (SELECT 1 c           WHERE 0 GROUP BY 1 HAVING 1 ORDER BY COUNT(1)) AS
  'null is expected';
SELECT (SELECT 1 c FROM DUAL WHERE 0 GROUP BY 1 HAVING 1 ORDER BY COUNT(1)) AS
  'null is expected';
SELECT (SELECT 1 c           WHERE 0 GROUP BY 1 HAVING 0 ORDER BY COUNT(1)) AS
  'null is expected';
SELECT (SELECT 1 c FROM DUAL WHERE 0 GROUP BY 1 HAVING 0 ORDER BY COUNT(1)) AS
  'null is expected';

SELECT 1 c FROM DUAL GROUP BY 1 HAVING 1 ORDER BY COUNT(1);
SELECT 1 c FROM DUAL GROUP BY 1 HAVING 0 ORDER BY COUNT(1);
SELECT 1 c           GROUP BY 1 HAVING 1 ORDER BY COUNT(1);

--echo #
--echo # Bug #18106058: RECENT REGRESSION: CRASH IN JOIN::MAKE_TMP_TABLES_INFO
--echo #

CREATE TABLE t1 (i INT);
INSERT INTO t1 VALUES (1);

SELECT ((SELECT 1 AS f           HAVING EXISTS(SELECT 1 FROM t1) IS TRUE
  ORDER BY f));
SELECT ((SELECT 1 AS f FROM DUAL HAVING EXISTS(SELECT 1 FROM t1) IS TRUE
  ORDER BY f));

SELECT 1 AS f          FROM DUAL HAVING EXISTS(SELECT 1 FROM t1) IS TRUE
  ORDER BY f;
SELECT 1 AS f                   HAVING EXISTS(SELECT 1 FROM t1) IS TRUE
  ORDER BY f;

DROP TABLE t1;

--echo #
--echo # Bug#17075846 : unquoted file names for variable values are
--echo #                accepted but parsed incorrectly
--echo #
--error ER_WRONG_TYPE_FOR_VAR 
SET default_storage_engine=a.myisam;
--error ER_PARSE_ERROR
SET default_storage_engine = .a.MyISAM;
--error ER_WRONG_TYPE_FOR_VAR 
SET default_storage_engine = a.b.MyISAM;
--error ER_WRONG_TYPE_FOR_VAR 
SET default_storage_engine = `a`.MyISAM;
--error ER_WRONG_TYPE_FOR_VAR 
SET default_storage_engine = `a`.`MyISAM`; 
--error ER_UNKNOWN_STORAGE_ENGINE 
set default_storage_engine = "a.MYISAM";
--error ER_UNKNOWN_STORAGE_ENGINE 
set default_storage_engine = 'a.MYISAM';
--error ER_UNKNOWN_STORAGE_ENGINE 
set default_storage_engine = `a.MYISAM`;
CREATE TABLE t1 (s VARCHAR(100));
--ERROR ER_BAD_FIELD_ERROR
CREATE TRIGGER trigger1 BEFORE INSERT ON t1 FOR EACH ROW
SET default_storage_engine = NEW.INNODB;
DROP TABLE t1;

--echo #
--echo # Some additional coverage tests for WL#7199 and friends
--echo #

CREATE TABLE t1 (i INT);
INSERT INTO t1 VALUES (1), (2);
CREATE TABLE t2 (i INT);
INSERT INTO t2 VALUES (10), (20);

SELECT i FROM t1 WHERE i = 1
UNION
SELECT i FROM t2 WHERE i = 10
ORDER BY i;

SELECT i FROM t1 WHERE i = 1
UNION
SELECT i FROM t2 WHERE i = 10
LIMIT 100;

SELECT i FROM t1 WHERE i = 1
UNION
SELECT i FROM t2 GROUP BY i HAVING i = 10
ORDER BY i;

SELECT i FROM t1 WHERE i = 1
UNION
SELECT i FROM t2 GROUP BY i HAVING i = 10
LIMIT 100;

(SELECT i FROM t1 WHERE i = 1) ORDER BY i;
(SELECT i FROM t1 WHERE i = 1) LIMIT 100;

(SELECT i FROM t1 GROUP BY i HAVING i = 1) ORDER BY i;
(SELECT i FROM t1 GROUP BY i HAVING i = 1) LIMIT 100;

DROP TABLE t1, t2;

--echo #
--echo # Bug#18486460 ASSERTION FAILED: N < M_SIZE AFTER FIX_INNER_REFS
--echo #

CREATE TABLE t1(b INT);
CREATE TABLE t2(a INT, b INT, c INT, d INT);

--source include/turn_off_only_full_group_by.inc
EXPLAIN SELECT
(
  SELECT
  ROW(t1.b, a) = ROW( ROW(1, t2.c) = ROW(1, d), c) = a
  FROM t1
)
FROM t2 GROUP BY a;
--source include/restore_sql_mode_after_turn_off_only_full_group_by.inc

DROP TABLE t1, t2;

--echo #
--echo # Bug#18498344: SELECT WITH ALIAS NOT WORKING IN 5.7
--echo #

CREATE TABLE t1 (
  a INT
);

INSERT INTO t1 VALUES ( 2 );

--echo # Should succeed
SELECT *
FROM ( SELECT a FROM t1 UNION SELECT 1 ORDER BY a ) AS a1
WHERE a1.a = 1 OR a1.a = 2;

DROP TABLE t1;

--echo #
--echo # Bug #18484088: PROBLEMS IN CREATE_FUNC_CAST ON QUERY ERRORS...
--echo #

# original bugreport:
--error ER_TOO_BIG_PRECISION
DO(CONVERT(CONVERT('',DECIMAL(66,0)), DECIMAL(66,0))), CAST(CONVERT(1,DECIMAL(65,31)) AS DATE);

# test an error message for the complex "column" name: "CONVERT('',DECIMAL(65,0))"
--error ER_TOO_BIG_PRECISION
SELECT CONVERT(CONVERT('',DECIMAL(65,0)), DECIMAL(66,0));

--echo #
--echo # Bug #18759387: PROBLEM IN ITEM_FUNC_XOR::NEG_TRANSFORMER
--echo #

--error ER_PARSE_ERROR
SELECT 1<
!(1 XOR TO_BASE64()));

--error ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT
SELECT 1<
!(1 XOR TO_BASE64());

--error ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT
SELECT  !('' XOR LENGTH());

--error ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT
SELECT  !((UNHEX() IS NULL));

--echo #
--echo # Bug #20086997: PARSER CONFUSES WITH 7BIT-CHARACTER STRING DETECTION
--echo #

--character_set latin2
CREATE DATABASE mysqltest1 CHARACTER SET LATIN2;
USE mysqltest1;
CREATE TABLE t1 (a VARCHAR(255) CHARACTER SET LATIN2);
SET CHARACTER SET cp1250_latin2;
INSERT INTO t1 VALUES ('£¥ª¯');
INSERT INTO t1 VALUES ('£¥ª¯' '');
SELECT HEX(a) FROM t1;
DROP DATABASE mysqltest1;
USE test;
--character_set utf8mb4

--echo #
--echo # WL #7201, WL #7202 and WL#8062 coverage tests
--echo #

CREATE TABLE t1 (i INT);

INSERT INTO t1 () SELECT * FROM t1;
INSERT INTO t1 SELECT HIGH_PRIORITY * FROM t1;
--error ER_WRONG_USAGE
INSERT INTO t1 SELECT DISTINCT ALL * FROM t1;
--error ER_WRONG_TABLE_NAME
REPLACE INTO `` SELECT * FROM ``;
DELETE QUICK FROM t1 WHERE i = 0;

DROP TABLE t1;

--echo #
--echo # Bug #21035515: PARSE_GCOL_EXPR SHOULD BE A KIND OF RESERVED WORD,
--echo #                NOT A KEYWORD

SET @parse_gcol_expr = 1;

SELECT 1 AS parse_gcol_expr;

CREATE TABLE parse_gcol_expr (i INT);
DROP TABLE parse_gcol_expr;

DELIMITER |;
--echo # parse_gcol_expr can't be a label:
CREATE PROCEDURE p1()
BEGIN
parse_gcol_expr: LOOP
  SELECT 1;
END LOOP parse_gcol_expr;
END|
DELIMITER ;|

DROP PROCEDURE p1;

--echo # PARSE_GCOL_EXPR is not a valid statement:
--error ER_PARSE_ERROR
PARSE_GCOL_EXPR (1);

--echo #
--echo # Bug #17400320	ALGORITHM= IS NOT SUPPORTED FOR ALTER TABLE WITH <PARTITION_OPTIONS>
--echo #

# Test some variant of dummy ALTER TABLE statements.

CREATE TABLE t1 (x INT PRIMARY KEY);
ALTER TABLE t1;
ALTER TABLE t1 ALGORITHM=DEFAULT;
ALTER TABLE t1 ALGORITHM=COPY;
ALTER TABLE t1 ALGORITHM=INPLACE;
ALTER TABLE t1 LOCK=DEFAULT;
--error ER_ALTER_OPERATION_NOT_SUPPORTED
ALTER TABLE t1 LOCK=NONE;
--error ER_ALTER_OPERATION_NOT_SUPPORTED
ALTER TABLE t1 LOCK=SHARED;
ALTER TABLE t1 LOCK=EXCLUSIVE;
ALTER TABLE t1 LOCK=SHARED, ALGORITHM=COPY,
               LOCK=NONE, ALGORITHM=DEFAULT,
               LOCK=EXCLUSIVE, ALGORITHM=INPLACE;
--error ER_WRONG_USAGE
ALTER TABLE t1 WITH VALIDATION;
--error ER_WRONG_USAGE
ALTER TABLE t1 WITHOUT VALIDATION;
--error ER_WRONG_USAGE
ALTER TABLE t1 LOCK=SHARED, WITH VALIDATION, ALGORITHM=COPY,
               LOCK=EXCLUSIVE, WITHOUT VALIDATION, ALGORITHM=INPLACE;
DROP TABLE t1;

--echo #
--echo # WL#8083: Introduce <query expression> parser rule
--echo # Bug#14743786: PARSE ERROR WITH UNION PARENTHESES ON TOP LEVEL
--echo #

--echo #
--echo # Part1: Regression Testing.
--echo #
CREATE TABLE t1 ( a INT );
INSERT INTO t1 VALUES ( 1 );

CREATE TABLE t2 ( a INT );
INSERT INTO t2 VALUES ( 2 ), ( 2 );

CREATE TABLE t3 ( a INT );
INSERT INTO t3 VALUES ( 3 ), ( 3 ), ( 3 );

SELECT 1 UNION SELECT 2;
(SELECT 1) UNION SELECT 2;
SELECT 1 UNION (SELECT 2);
(SELECT 1) UNION (SELECT 2);

  SELECT 2 FROM t1 UNION ((SELECT 3 FROM t1));
 (SELECT 2 FROM t1)  UNION SELECT 3 FROM t1;
((SELECT 2 FROM t1)) UNION SELECT 3 FROM t1;

(SELECT 1 FROM t1 ORDER BY 1) ORDER BY 1;

(SELECT 1 FROM t1 LIMIT 1) LIMIT 1;

--error ER_PARSE_ERROR
SELECT a FROM t1 LIMIT 1 UNION ALL SELECT a FROM t1;

(SELECT 1) UNION (SELECT 2 UNION SELECT 3);

(SELECT a FROM t1 LIMIT 1) UNION ALL (SELECT a FROM t1 ORDER BY a) LIMIT 2;

(SELECT a FROM t1 LIMIT 1) UNION ALL ((SELECT a FROM t1 ORDER BY a)) LIMIT 2;

--error ER_BAD_FIELD_ERROR
SELECT 1 UNION SELECT 2 FROM t1 ORDER BY a LIMIT 1;
SELECT 1 UNION (SELECT 2 FROM t1 ORDER BY a LIMIT 1);
--error ER_BAD_FIELD_ERROR
(SELECT 1 FROM t1 LIMIT 2) UNION  SELECT 2 FROM t1 ORDER BY a LIMIT 1;
(SELECT 1 FROM t1 LIMIT 2) UNION (SELECT 2 FROM t1 ORDER BY a LIMIT 1);
(SELECT a FROM t1 LIMIT 2) LIMIT 1;
((SELECT a FROM t1 LIMIT 2)) LIMIT 1;
(SELECT a FROM t1 LIMIT 2) ORDER BY 1;
(SELECT 1 FROM t1 LIMIT 2) UNION (SELECT 2 FROM t1 ORDER BY a LIMIT 1) LIMIT 1;

--error ER_BAD_FIELD_ERROR
(SELECT 1 FROM t1 LIMIT 2) UNION (SELECT 2 FROM t1 ORDER BY a LIMIT 1)
  ORDER BY a;

--error ER_BAD_FIELD_ERROR
(SELECT 1 FROM t1 LIMIT 1) UNION ((SELECT 2 FROM t1 ORDER BY a LIMIT 2))
  ORDER BY a;

(SELECT a FROM t2 LIMIT 1) UNION (SELECT a FROM t3 LIMIT 2) LIMIT 1;

(SELECT 1 FROM t1 ORDER BY 1) ORDER BY 1;

--error ER_TABLENAME_NOT_ALLOWED_HERE
(SELECT a FROM t1 LIMIT 1) ORDER BY t1.a;

--error ER_TABLENAME_NOT_ALLOWED_HERE
((SELECT a FROM t1 LIMIT 1)) ORDER BY t1.a;

--error ER_TABLENAME_NOT_ALLOWED_HERE
(SELECT a FROM t1 LIMIT 1) UNION ALL (SELECT 2) ORDER BY t1.b;

--error ER_TABLENAME_NOT_ALLOWED_HERE
(SELECT a FROM t1 LIMIT 1) UNION ALL ((SELECT 2)) ORDER BY t1.b;

--error ER_TABLENAME_NOT_ALLOWED_HERE
(SELECT a FROM t1 LIMIT 1) UNION ALL (SELECT a FROM t1 ORDER BY a LIMIT 2)
  ORDER BY t1.b;

# [ before WL11350: Generally speaking, we don't support nested unions]
# We do now.

SELECT 1 UNION ( SELECT 1 UNION SELECT 1 );
# OTOH union expressions are left-associative by their nature,
# so "s1 UNION s2 UNION ..." and "(s1 UNION s2) UNION ..." are same,
# thus the parser accepts nested query expressions at the left hand side:
( SELECT 1 UNION SELECT 1 ) UNION SELECT 1;
( (SELECT 1 UNION SELECT 1 ) UNION SELECT 1 ) UNION SELECT 1;
# [ But: ] now ok
( SELECT 1 UNION ( SELECT 1 UNION SELECT 1 ) UNION SELECT 1 );

# Missing ")":
--error ER_PARSE_ERROR
( SELECT 1 UNION ( SELECT 1 UNION SELECT 1 ) UNION SELECT 1;

--error ER_PARSE_ERROR
SELECT a FROM t1 ORDER BY a UNION SELECT a FROM t1;

(SELECT * FROM t1 LIMIT 1) UNION SELECT * FROM t1;
(SELECT * FROM t1 ORDER BY a) UNION SELECT * FROM t1;

--error ER_PARSE_ERROR
SELECT a FROM t1
UNION
SELECT a FROM t1 ORDER BY a
UNION
SELECT a FROM t1;

(SELECT SQL_CALC_FOUND_ROWS a FROM t1 LIMIT 2)
UNION
(SELECT a FROM t2 ORDER BY a) LIMIT 2;

DROP TABLE t1, t2, t3;

delimiter |;

CREATE PROCEDURE p1() BEGIN IF whatever THEN SELECT 1; END IF; END|

delimiter ;|

--error 1054
CALL p1();

DROP PROCEDURE p1;

--echo #
--echo # Part 2: Test of changed (fixed) behavior.
--echo #

CREATE TABLE t1 ( a INT );
INSERT INTO t1 VALUES ( 1 );

CREATE TABLE t2 ( a INT );
INSERT INTO t2 VALUES ( 2 ), ( 2 );

CREATE TABLE t3 ( a INT );
INSERT INTO t3 VALUES ( 3 ), ( 3 ), ( 3 );

   (SELECT 1 FROM t1   UNION   SELECT 2 FROM t1);
  ((SELECT 1 FROM t1   UNION   SELECT 2 FROM t1));
   (SELECT 1 FROM t1   UNION  (SELECT 2 FROM t1));
  ((SELECT 1 FROM t1   UNION  (SELECT 2 FROM t1)));
  ((SELECT 1 FROM t1   UNION ((SELECT 2 FROM t1))));
  ((SELECT 1 FROM t1)  UNION   SELECT 2 FROM t1);
 (((SELECT 1 FROM t1)) UNION   SELECT 2 FROM t1);
((((SELECT 1 FROM t1)) UNION   SELECT 2 FROM t1));
  ((SELECT 1 FROM t1)  UNION  (SELECT 2 FROM t1));
 (((SELECT 1 FROM t1)  UNION  (SELECT 2 FROM t1)));
((((SELECT 1 FROM t1)) UNION  (SELECT 2 FROM t1)));
 (((SELECT 1 FROM t1)  UNION ((SELECT 2 FROM t1))));
((((SELECT 1 FROM t1)) UNION ((SELECT 2 FROM t1))));

(SELECT 1 UNION SELECT 2) ORDER BY 1;
((SELECT 1 UNION SELECT 2)) ORDER BY 1;
((SELECT 1) ORDER BY 1);
((SELECT 1) LIMIT 1);
(SELECT 1 UNION SELECT 2) LIMIT 1;
((SELECT 1 UNION SELECT 2)) LIMIT 1;

--error ER_PARSE_ERROR
(SELECT a FROM t1) LIMIT 1 UNION ALL ((SELECT a FROM t1 ORDER BY a)) LIMIT 2;

DROP TABLE t1, t2, t3;

--echo #
--echo # Part 3: The <table factor> syntax.
--echo #
CREATE TABLE t1 ( a INT );
INSERT INTO t1 VALUES ( 1 );

CREATE TABLE t2 ( a INT );
INSERT INTO t2 VALUES ( 2 );

CREATE TABLE t3 ( a INT );
INSERT INTO t3 VALUES ( 3 );

CREATE TABLE t4 ( a INT );
INSERT INTO t4 VALUES ( 3 );

SELECT * FROM    (SELECT 1 FROM t1   UNION   SELECT 2 FROM t1) dt;
SELECT * FROM   ((SELECT 1 FROM t1   UNION   SELECT 2 FROM t1)) dt;
SELECT * FROM    (SELECT 1 FROM t1   UNION  (SELECT 2 FROM t1)) dt;
SELECT * FROM   ((SELECT 1 FROM t1   UNION  (SELECT 2 FROM t1))) dt;
SELECT * FROM   ((SELECT 1 FROM t1   UNION ((SELECT 2 FROM t1)))) dt;
SELECT * FROM   ((SELECT 1 FROM t1)  UNION   SELECT 2 FROM t1) dt;
SELECT * FROM  (((SELECT 1 FROM t1)) UNION   SELECT 2 FROM t1) dt;
SELECT * FROM ((((SELECT 1 FROM t1)) UNION   SELECT 2 FROM t1)) dt;
SELECT * FROM   ((SELECT 1 FROM t1)  UNION  (SELECT 2 FROM t1)) dt;
SELECT * FROM  (((SELECT 1 FROM t1)  UNION  (SELECT 2 FROM t1))) dt;
SELECT * FROM ((((SELECT 1 FROM t1)) UNION  (SELECT 2 FROM t1))) dt;
SELECT * FROM  (((SELECT 1 FROM t1)  UNION ((SELECT 2 FROM t1)))) dt;
SELECT * FROM ((((SELECT 1 FROM t1)) UNION ((SELECT 2 FROM t1)))) dt;

--error ER_DERIVED_MUST_HAVE_ALIAS
SELECT * FROM    (SELECT 1 FROM t1   UNION   SELECT 2 FROM t1);
--error ER_DERIVED_MUST_HAVE_ALIAS
SELECT * FROM   ((SELECT 1 FROM t1   UNION   SELECT 2 FROM t1));
--error ER_DERIVED_MUST_HAVE_ALIAS
SELECT * FROM    (SELECT 1 FROM t1   UNION  (SELECT 2 FROM t1));
--error ER_DERIVED_MUST_HAVE_ALIAS
SELECT * FROM   ((SELECT 1 FROM t1   UNION  (SELECT 2 FROM t1)));
--error ER_DERIVED_MUST_HAVE_ALIAS
SELECT * FROM   ((SELECT 1 FROM t1   UNION ((SELECT 2 FROM t1))));
--error ER_DERIVED_MUST_HAVE_ALIAS
SELECT * FROM   ((SELECT 1 FROM t1)  UNION   SELECT 2 FROM t1);
--error ER_DERIVED_MUST_HAVE_ALIAS
SELECT * FROM  (((SELECT 1 FROM t1)) UNION   SELECT 2 FROM t1);
--error ER_DERIVED_MUST_HAVE_ALIAS
SELECT * FROM ((((SELECT 1 FROM t1)) UNION   SELECT 2 FROM t1));
--error ER_DERIVED_MUST_HAVE_ALIAS
SELECT * FROM   ((SELECT 1 FROM t1)  UNION  (SELECT 2 FROM t1));
--error ER_DERIVED_MUST_HAVE_ALIAS
SELECT * FROM  (((SELECT 1 FROM t1)  UNION  (SELECT 2 FROM t1)));
--error ER_DERIVED_MUST_HAVE_ALIAS
SELECT * FROM ((((SELECT 1 FROM t1)) UNION  (SELECT 2 FROM t1)));
--error ER_DERIVED_MUST_HAVE_ALIAS
SELECT * FROM  (((SELECT 1 FROM t1)  UNION ((SELECT 2 FROM t1))));
--error ER_DERIVED_MUST_HAVE_ALIAS
SELECT * FROM ((((SELECT 1 FROM t1)) UNION ((SELECT 2 FROM t1))));

SELECT * FROM  ( t1 JOIN t2 ON TRUE );
SELECT * FROM (( t1 JOIN t2 ON TRUE ));

SELECT * FROM ( t1 JOIN t2 ON TRUE  JOIN t3 ON TRUE );
SELECT * FROM ((t1 JOIN t2 ON TRUE) JOIN t3 ON TRUE );

SELECT * FROM (t1 INNER JOIN t2 ON (t1.a = t2.a));

--error ER_PARSE_ERROR
SELECT 1 FROM (SELECT 1 FROM t1 INTO @v) a;

SELECT 1 FROM (t1);
SELECT 1 FROM ((t1));

SELECT 1 UNION SELECT 2 FROM (t2);

SELECT 1 FROM  (SELECT 2  ORDER BY 1) AS res;

--echo #
--echo # This syntax is no longer allowed
--echo #
#--error ER_DERIVED_MUST_HAVE_ALIAS
SELECT 1 FROM ((SELECT 2) ORDER BY 1) AS res;
--error ER_PARSE_ERROR
SELECT 1 FROM ((SELECT 2) a ORDER BY 1) AS res;
#--error ER_DERIVED_MUST_HAVE_ALIAS
SELECT 1 FROM ((SELECT 2) LIMIT 1) AS res;
--error ER_PARSE_ERROR
SELECT 1 FROM ((SELECT 2) a LIMIT 1) AS res;

SELECT * FROM ( t1 AS alias1 );
SELECT * FROM   t1 AS alias1, t2 AS alias2;
SELECT * FROM ( t1 AS alias1, t2 AS alias2 );

--error ER_NONUNIQ_TABLE
SELECT * FROM ( t1 JOIN t2 ON TRUE, t1 JOIN t3 ON TRUE );
--error ER_PARSE_ERROR
SELECT * FROM ( t1 JOIN t2 ON TRUE, t1 t11 JOIN t3 ON TRUE ) t1a;
--error ER_PARSE_ERROR
SELECT * FROM ( t1 JOIN t2 ON TRUE, SELECT 1 FROM DUAL );
SELECT * FROM ( t1 JOIN t2 ON TRUE, (SELECT 1 FROM DUAL) t1a );
SELECT * FROM t1 JOIN t2 ON TRUE, (SELECT 1 FROM DUAL) t1a;
--error ER_DERIVED_MUST_HAVE_ALIAS
SELECT * FROM ( SELECT 1 FROM DUAL );

SELECT * FROM ( SELECT 1 FROM DUAL ) t1a;

SELECT * FROM  ( t1, t2 );
SELECT * FROM (( t1, t2 ));

SELECT * FROM  ( (t1),   t2  );
SELECT * FROM  (((t1)),  t2  );
SELECT * FROM  ( (t1),  (t2) );
SELECT * FROM  (  t1,   (t2) );

((SELECT 1 UNION SELECT 1) UNION SELECT 1);
SELECT * FROM ((SELECT 1 UNION SELECT 1) UNION SELECT 1) a;

SELECT * FROM (t1, t2) JOIN (t3, t4) ON TRUE;
SELECT * FROM ((t1, t2) JOIN t3 ON TRUE);
--error ER_NON_UNIQ_ERROR
SELECT * FROM t1 JOIN ( t2, t3 ) USING ( a );

DROP TABLE t1, t2, t3, t4;

--echo # Check the format in the note.
CREATE TABLE t1 (a INT);
EXPLAIN SELECT 1 FROM (SELECT 1 FROM t1) t;
DROP TABLE t1;

CREATE TABLE t1 ( a INT );
CREATE TABLE t2 ( b INT );
CREATE TABLE t3 ( c INT );
CREATE TABLE t4 ( d INT );
INSERT INTO t1 VALUES (1);
INSERT INTO t2 VALUES (2);
INSERT INTO t3 VALUES (2);
INSERT INTO t4 VALUES (2);

SELECT * FROM t1 LEFT JOIN ( t2, t3, t4 ) ON a = c;

--error ER_NONUNIQ_TABLE
SELECT * FROM t1 NATURAL JOIN ((t1 NATURAL JOIN t1), (t1 NATURAL JOIN t1));

DROP TABLE t1, t2, t3, t4;

CREATE TABLE t1 ( a INT );
CREATE TABLE t2 ( b INT );
CREATE TABLE t3 ( c INT );
CREATE TABLE t4 ( d INT );
CREATE TABLE t5 ( d INT );

SELECT * FROM t5 NATURAL JOIN ((t1 NATURAL JOIN t2), (t3 NATURAL JOIN t4));

SELECT * FROM ((t1 NATURAL JOIN t2), (t3 NATURAL JOIN t4)) NATURAL JOIN t5;

SELECT * FROM t1 JOIN ( t2, t3 ) ON TRUE;

SELECT * FROM ( t1, t2 , t3 );
SELECT * FROM ( ( t1, t2 ), t3 );
SELECT * FROM ( ((t1, t2)), t3 );
SELECT * FROM ( t1, ( t2, t3 ) );
SELECT * FROM ( t1, ((t2, t3)) );

SELECT * FROM ((( t1, t2 ), t3));
SELECT * FROM ((((t1, t2)), t3));
SELECT * FROM ((t1, ( t2, t3 )));
SELECT * FROM ((t1, ((t2, t3))));

--error ER_VIEW_SELECT_CLAUSE
CREATE VIEW v1 AS SELECT 1 INTO @v;

CREATE VIEW v1 AS SELECT 1 FROM ( SELECT 1 FROM t1 ) my_table;

DROP TABLE t1, t2, t3, t4, t5;
DROP VIEW v1;

CREATE TABLE t1( a INT );
INSERT INTO t1 VALUES (1);

SELECT 1 INTO @v;
(SELECT 1 INTO @v);
((SELECT 1 INTO @v));

SELECT 1 FROM t1 INTO @v;
(SELECT 1 FROM t1 INTO @v);
((SELECT 1 FROM t1 INTO @v));

--error ER_PARSE_ERROR
SELECT 1 FROM t1 INTO @v UNION SELECT 1;
--error ER_PARSE_ERROR
(SELECT 1 FROM t1 INTO @v) UNION SELECT 1;
--error ER_PARSE_ERROR
SELECT 1 FROM t1 INTO @v UNION (SELECT 1);
--error ER_PARSE_ERROR
((SELECT 1 FROM t1 INTO @v) UNION (SELECT 1));

--error ER_MISPLACED_INTO
SELECT 1 INTO @v UNION SELECT 1;
--error ER_MISPLACED_INTO
(SELECT 1 INTO @v) UNION SELECT 1;
--error ER_MISPLACED_INTO
SELECT 1 INTO @v UNION (SELECT 1);
--error ER_MISPLACED_INTO
((SELECT 1 INTO @v) UNION (SELECT 1));

--error ER_TOO_MANY_ROWS
SELECT 1 UNION SELECT 2 INTO @v;
--error ER_TOO_MANY_ROWS
(SELECT 1) UNION SELECT 2 INTO @v;
--error ER_TOO_MANY_ROWS
(SELECT 1) UNION (SELECT 2 INTO @v);
--error ER_TOO_MANY_ROWS
((SELECT 1) UNION (SELECT 2 INTO @v));

--error ER_TOO_MANY_ROWS
SELECT 1 UNION SELECT 2 INTO @v FROM t1;
--error ER_TOO_MANY_ROWS
(SELECT 1) UNION SELECT 2 INTO @v FROM t1;
--error ER_TOO_MANY_ROWS
(SELECT 1) UNION (SELECT 2 INTO @v FROM t1);
--error ER_TOO_MANY_ROWS
((SELECT 1) UNION (SELECT 2 INTO @v FROM t1));

SELECT 1 UNION SELECT 1 INTO @v FROM t1;
(SELECT 1) UNION SELECT 1 INTO @v FROM t1;
(SELECT 1) UNION (SELECT 1 INTO @v FROM t1);
((SELECT 1) UNION (SELECT 1 INTO @v FROM t1));

SELECT 1 UNION SELECT 2 INTO OUTFILE 'parser.test.file1';
SELECT 1 UNION (SELECT 2 INTO OUTFILE 'parser.test.file2');
(SELECT 1) UNION SELECT 2 INTO OUTFILE 'parser.test.file3';
(SELECT 1) UNION (SELECT 2 INTO OUTFILE 'parser.test.file4');
((SELECT 1) UNION (SELECT 2 INTO OUTFILE 'parser.test.file5'));
--let $datadir=`select @@datadir`
--remove_file $datadir/test/parser.test.file1
--remove_file $datadir/test/parser.test.file2
--remove_file $datadir/test/parser.test.file3
--remove_file $datadir/test/parser.test.file4
--remove_file $datadir/test/parser.test.file5

--error ER_MISPLACED_INTO
SELECT * FROM (SELECT a INTO @v FROM t1) t1a;
--error ER_MISPLACED_INTO
SELECT * FROM (SELECT a INTO @v) t1a;

DROP TABLE t1;

--echo #
--echo # Part 4: The <joined table> syntax.
--echo #
CREATE TABLE t1( a INT );
CREATE TABLE t2( b INT );
CREATE TABLE t3( c INT );
CREATE TABLE t4( d INT );
CREATE TABLE t5( e INT );

--echo # We use EXPLAIN so we can get the parser's interpretation of the
--echo # nesting. We don't really care about the execution plan.

SET optimizer_switch = 'block_nested_loop=off';

SELECT * FROM t1 JOIN t2;
SELECT * FROM t1 JOIN t2 ON a = b;
SELECT * FROM t1 t11 JOIN t1 t12 USING ( a );
SELECT * FROM t1 INNER JOIN t2;
SELECT * FROM t1 INNER JOIN t2 ON a = b;
SELECT * FROM t1 t11 INNER JOIN t1 t12 USING ( a );

SELECT * FROM t1 CROSS JOIN t2;
SELECT * FROM t1 CROSS JOIN t2 ON a = b;
SELECT * FROM t1 t11 CROSS JOIN t1 t12 USING ( a );

SELECT * FROM t1 STRAIGHT_JOIN t2;
SELECT * FROM t1 STRAIGHT_JOIN t2 ON a = b;
SELECT * FROM t1 t11 STRAIGHT_JOIN t1 t12 USING ( a );

SELECT * FROM t1 t11 NATURAL JOIN t1 t12;
--error ER_PARSE_ERROR
SELECT * FROM t1 t11 NATURAL JOIN t1 t12 ON t11.a = t12.a;
--error ER_PARSE_ERROR
SELECT * FROM t1 t11 NATURAL JOIN t1 t12 USING ( a );
SELECT * FROM t1 t11 NATURAL INNER JOIN t1 t12;

--error ER_PARSE_ERROR
SELECT * FROM t1 LEFT JOIN t2;
SELECT * FROM t1 LEFT JOIN t2 ON a = b;
SELECT * FROM t1 t11 LEFT JOIN t1 t12 USING ( a );
SELECT * FROM t1 NATURAL LEFT JOIN t2;
--error ER_PARSE_ERROR
SELECT * FROM t1 LEFT OUTER JOIN t2;
SELECT * FROM t1 LEFT OUTER JOIN t2 ON a = b;
SELECT * FROM t1 t11 LEFT OUTER JOIN t1 t12 USING ( a );
SELECT * FROM t1 NATURAL LEFT OUTER JOIN t2;

--error ER_PARSE_ERROR
SELECT * FROM t1 RIGHT JOIN t2;
SELECT * FROM t1 RIGHT JOIN t2 ON a = b;
SELECT * FROM t1 t11 RIGHT JOIN t1 t12 USING ( a );
SELECT * FROM t1 NATURAL RIGHT JOIN t2;
--error ER_PARSE_ERROR
SELECT * FROM t1 RIGHT OUTER JOIN t2;
SELECT * FROM t1 RIGHT OUTER JOIN t2 ON a = b;
SELECT * FROM t1 t11 RIGHT OUTER JOIN t1 t12 USING ( a );
SELECT * FROM t1 NATURAL RIGHT OUTER JOIN t2;


--echo # Right-deep join nesting.
EXPLAIN SELECT * FROM t1 JOIN t2 JOIN t3 ON t2.b = t3.c ON t1.a = t2.b;
--echo # Right-deep join nesting from t2 and on.
EXPLAIN SELECT * FROM t1 JOIN t2 JOIN t3 JOIN t4 ON t3.c = t4.d ON t2.b = t3.c;
--echo # Right-deep join nesting from t2 and on.
EXPLAIN SELECT * FROM t1 JOIN t2 JOIN t3 JOIN t4 ON t3.c = t4.d ON t1.a = t2.b;
--echo # Left-deep join nesting.
EXPLAIN SELECT * FROM t1 JOIN t2 ON t1.a = t2.b JOIN t3 ON t2.b = t3.c;
--echo # A cross join joined with an inner join (i.e. with a join condition).
EXPLAIN SELECT * FROM t1 JOIN t2 JOIN t3 ON t1.a = t2.b;
EXPLAIN SELECT * FROM t1 t11 JOIN t2 JOIN t1 t12 USING ( a );
--echo # A left-deep cross join tree joined with an inner join.
EXPLAIN SELECT * FROM t1 JOIN t2 JOIN t3 JOIN t4 ON t1.a = t2.b;
EXPLAIN SELECT * FROM t1 JOIN t2 JOIN t3 JOIN t4 JOIN t5 ON t1.a = t2.b;

--echo # The different kinds of <table_ref> when used with context-dependent
--echo # join. Tests the top-down build of the parse tree.
EXPLAIN SELECT * FROM t1 JOIN (t2) JOIN t3;
EXPLAIN SELECT * FROM t1 JOIN (SELECT 1 AS b) a JOIN t3;
EXPLAIN SELECT * FROM t1 JOIN (t2 JOIN t3) JOIN t4;
EXPLAIN SELECT * FROM t1 JOIN (t2, t3) JOIN t4;

EXPLAIN SELECT * FROM t1 JOIN t2 JOIN t3 ON t2.b = t3.c JOIN t4 ON t1.a = t4.d;
EXPLAIN SELECT * FROM t1 JOIN t2 JOIN t3 ON t2.b = t3.c JOIN t4 ON t1.a = t2.b;
EXPLAIN SELECT * FROM t1 t11 JOIN t2 JOIN t1 t12 USING(a) JOIN t1 t13 USING(a);
EXPLAIN SELECT * FROM t2 JOIN t1 t11 JOIN t1 t12 USING(a) JOIN t1 t13 USING(a);

EXPLAIN SELECT * FROM t1 JOIN t2 JOIN t3 JOIN t4 ON t3.c = t4.d;

SET optimizer_switch = DEFAULT;

DROP TABLE t1, t2, t3, t4, t5;

--echo # Testing correct nesting of natural joins.
CREATE TABLE t1( a INT, b int );
CREATE TABLE t2( a INT, c int );
CREATE TABLE t3( a INT, d int );

INSERT INTO t1 VALUES (1, 1), (2, 2), (3, 3);
INSERT INTO t2 VALUES         (2, 2), (3, 3), (4, 4);
INSERT INTO t3 VALUES                 (3, 3), (4, 4), (5, 5);

--echo # The two queries below should produce identical result sets.
--sorted_result
SELECT * FROM t1 NATURAL LEFT JOIN t2 NATURAL RIGHT JOIN t3;
--sorted_result
SELECT * FROM (t1 NATURAL LEFT JOIN t2) NATURAL RIGHT JOIN t3;
--echo # This result should differ from the result sets above.
--sorted_result
SELECT * FROM t1 NATURAL LEFT JOIN (t2 NATURAL RIGHT JOIN t3);

DROP TABLE t1, t2, t3;


--echo #
--echo # Bug#22995438: BUILD INDEX DEFINITION SYNTAX BOTTOM-UP
--echo #

--echo # This is regression testing for the refactoring.

CREATE TABLE t1 (
  a INT,
  b INT,
  c INT,
  d INT,
  e INT,
  f INT,
  g INT,
  h INT,
  i INT,
  j INT,
  k INT,
  l INT,
  m INT,
  n INT,
  o INT
 );

CREATE        INDEX a_index             ON t1( a );
CREATE UNIQUE INDEX b_index             ON t1( b );
CREATE        INDEX c_index USING btree ON t1( c );
--error ER_PARSE_ERROR
CREATE        INDEX c_index USING btree USING btree ON t1( c );
--error ER_SPATIAL_MUST_HAVE_GEOM_COL
CREATE        INDEX d_index USING rtree ON t1( d );
CREATE        INDEX e_index TYPE  btree ON t1( e );
CREATE        INDEX type TYPE  btree ON t1( f );
--error ER_PARSE_ERROR
CREATE        INDEX TYPE  btree ON t1( g );
--error ER_SPATIAL_MUST_HAVE_GEOM_COL
CREATE        INDEX h_index TYPE  rtree ON t1( h );
CREATE        INDEX i_index             ON t1( i ) KEY_BLOCK_SIZE = 1;
CREATE        INDEX j_index             ON t1( j ) KEY_BLOCK_SIZE = 1 KEY_BLOCK_SIZE = 1;
CREATE        INDEX k_index             ON t1( k ) COMMENT 'A comment';
CREATE        INDEX k_index2            ON t1( k ) COMMENT 'A comment' COMMENT 'Another comment';
CREATE        INDEX l_index             ON t1( l ) USING btree;
CREATE        INDEX m_index             ON t1( m ) TYPE btree;
CREATE        INDEX n_index USING btree ON t1( n ) USING btree;
--error ER_SPATIAL_MUST_HAVE_GEOM_COL
CREATE        INDEX x_index USING btree ON t1( o ) USING rtree;
CREATE        INDEX o_index USING rtree ON t1( o ) USING btree;

ANALYZE TABLE t1;
SHOW INDEXES FROM t1;

DROP TABLE t1;

--echo #
--echo # WL#8907: Parser refactoring: merge all SELECT rules into one.
--echo #

--echo # Warning on hints in CREATE or ALTER VIEW:

CREATE VIEW v1 AS SELECT /*+ QB_NAME(a) */ 1;
ALTER VIEW v1 AS SELECT /*+ QB_NAME(a) */ 1;
SELECT * FROM v1;
DROP VIEW v1;

--echo # The "Hints aren't supported in CREATE or ALTER VIEW" warning is masked:

CREATE VIEW v1 AS SELECT /*+ BAD_HINT */ 1;
ALTER VIEW v1 AS SELECT /*+ BAD_HINT */ 1;
SELECT * FROM v1;
DROP VIEW v1;

--echo #
--echo #
--echo # Bug#23321895: CRASH IN IS_VIEW_OR_DERIVED OR
--echo # TABLE_LIST::IS_LEAF_FOR_NAME_RESOLUTION
--echo #
CREATE TABLE t1( a INT );
CREATE TABLE t2( a INT );
CREATE TABLE t3( a INT );
CREATE TABLE t4( a INT );

--error ER_UNKNOWN_SYSTEM_VARIABLE
SELECT 1
FROM ( SELECT 1 FROM t1 JOIN t2 ON @@q ) AS d
JOIN t3 LEFT JOIN t4 ON 1;

DROP TABLE t1, t2, t3, t4;



###########################################################################
--echo #
--echo # Bug#39559: dump of stored procedures / functions with C-style
--echo #     comment can't be read back
--echo #

--write_file $MYSQLTEST_VARDIR/tmp/bug39559.sql
select 2 as expected, /*!01000/**/*/ 2 as result;
select 1 as expected, /*!99998/**/*/ 1 as result;
select 3 as expected, /*!01000 1 + */ 2 as result;
select 2 as expected, /*!99990 1 + */ 2 as result;
select 7 as expected, /*!01000 1 + /* 8 + */ 2 + */ 4 as result;
select 8 as expected, /*!99998 1 + /* 2 + */ 4 + */ 8 as result;
select 7 as expected, /*!01000 1 + /*!01000 8 + */ 2 + */ 4 as result;
select 7 as expected, /*!01000 1 + /*!99998 8 + */ 2 + */ 4 as result;
select 4 as expected, /*!99998 1 + /*!99998 8 + */ 2 + */ 4 as result;
select 4 as expected, /*!99998 1 + /*!01000 8 + */ 2 + */ 4 as result;
select 7 as expected, /*!01000 1 + /*!01000 8 + /*!01000 error */ 16 + */ 2 + */ 4 as result;
select 4 as expected, /* 1 + /*!01000 8 + */ 2 + */ 4;
EOF

--exec $MYSQL --comments --force --table test <$MYSQLTEST_VARDIR/tmp/bug39559.sql
--remove_file $MYSQLTEST_VARDIR/tmp/bug39559.sql

--echo # Bug#46527 "COMMIT AND CHAIN RELEASE does not make sense"
--echo #
--error ER_PARSE_ERROR
COMMIT AND CHAIN RELEASE;

COMMIT AND NO CHAIN RELEASE;
disconnect default;
connect(default, localhost, root,,);

COMMIT RELEASE;
disconnect default;
connect(default, localhost, root,,);

--error ER_PARSE_ERROR
COMMIT CHAIN RELEASE;

--error ER_PARSE_ERROR
COMMIT NO CHAIN RELEASE;

--error ER_PARSE_ERROR
COMMIT AND NO RELEASE;
--error ER_PARSE_ERROR
COMMIT AND RELEASE;

COMMIT NO RELEASE;
--error ER_PARSE_ERROR
COMMIT CHAIN NO RELEASE;
--error ER_PARSE_ERROR
COMMIT NO CHAIN NO RELEASE;

--error ER_PARSE_ERROR
COMMIT AND RELEASE CHAIN;

COMMIT AND NO CHAIN NO RELEASE;

--error ER_PARSE_ERROR
ROLLBACK AND CHAIN RELEASE;

ROLLBACK AND NO CHAIN RELEASE;
disconnect default;
connect(default, localhost, root,,);

ROLLBACK RELEASE;
disconnect default;
connect(default, localhost, root,,);

--error ER_PARSE_ERROR
ROLLBACK CHAIN RELEASE;

--error ER_PARSE_ERROR
ROLLBACK NO CHAIN RELEASE;
disconnect default;
connect(default, localhost, root,,);

--error ER_PARSE_ERROR
ROLLBACK AND NO RELEASE;

--error ER_PARSE_ERROR
ROLLBACK AND RELEASE;

ROLLBACK NO RELEASE;

--error ER_PARSE_ERROR
ROLLBACK CHAIN NO RELEASE;

--error ER_PARSE_ERROR
ROLLBACK NO CHAIN NO RELEASE;
--error ER_PARSE_ERROR
ROLLBACK AND RELEASE CHAIN;

ROLLBACK AND NO CHAIN NO RELEASE;

--echo #
--echo # Bug#26132947: SERVER CAN EXIT ON ALTER TABLE ADD PARTITION SYNTAX
--echo #
CREATE TABLE t1 (a INT PRIMARY KEY) PARTITION BY HASH (a) PARTITIONS 1;
--error ER_ADD_PARTITION_NO_NEW_PARTITION
ALTER TABLE t1 ADD PARTITION;
DROP TABLE t1;

--echo #

--echo #
--echo # Bug#25717617: Wrong syntax error line numbers when sql_mode has the
--echo #               IGNORE_SPACE flag
--echo #

SET @save_sql_mode=@@sql_mode;
SET sql_mode='IGNORE_SPACE';

--echo # Expected error line number is 2:
--error ER_PARSE_ERROR
CREATE
TABLE;

--echo # Expected error line number is 5:
--error ER_PARSE_ERROR
CREATE
#
#
#
TABLE;

--echo #
--echo # Regression test added in WL#8657
--echo #

CREATE TEMPORARY TABLE t1(a INT);
SHOW COLUMNS FROM t1 WHERE FIELD='a';
DROP TABLE t1;

--error ER_WRONG_DB_NAME
ALTER TABLE t1 RENAME TO ``.t1;

--error ER_PARSE_ERROR
CREATE TABLE t1 (a INT) PARTITION BY KEY ALGORITHM = 10 () PARTITIONS 3;

--error ER_PARSE_ERROR
ALTER EVENT ev1;

--error ER_PARSE_ERROR
ALTER INSTANCE ROTATE MyISAM MASTER KEY;

--error ER_PARSE_ERROR
REVOKE SELECT(c1) ON FUNCTION *.* FROM r1;

--error ER_PARSE_ERROR
GRANT SELECT(c1) ON FUNCTION *.* TO r1;

--error ER_WRONG_TABLE_NAME
CREATE INDEX idx1 ON `` (c1);

--error ER_WRONG_TABLE_NAME
ALTER TABLE t1 EXCHANGE PARTITION p0 WITH TABLE `` WITH VALIDATION;

--error ER_WRONG_TABLE_NAME
CHECK TABLE ``;

--error ER_WRONG_TABLE_NAME
DROP INDEX idx1 ON ``;

--error ER_WRONG_TABLE_NAME
CACHE INDEX `` IN c;

--error ER_WRONG_TABLE_NAME
CACHE INDEX `` PARTITION (ALL) IN c;

--error ER_WRONG_TABLE_NAME
LOAD INDEX INTO CACHE `` PARTITION (ALL);

--error ER_WRONG_TABLE_NAME
LOAD INDEX INTO CACHE ``;


SET @@sql_mode=@save_sql_mode;

--echo #
--echo # Bug#24756971: REMOVE OBSOLETE LEXER HACK AROUND SELECT LIST
--echo #

--error ER_PARSE_ERROR
SELECT 1,,2;

--error ER_PARSE_ERROR
SELECT ,,1;

--error ER_PARSE_ERROR
SELECT ,,,;


--echo #
--echo # Removal of undocumented syntax, see bug#27389878.
--echo #

--error ER_PARSE_ERROR
CREATE DATABASE db CHARSET DEFAULT;

--error ER_PARSE_ERROR
CREATE DATABASE db COLLATE DEFAULT;

--error ER_PARSE_ERROR
CREATE TABLE t (i INT) CHARSET DEFAULT;

--error ER_PARSE_ERROR
CREATE TABLE t (i INT) COLLATE DEFAULT;

--error ER_PARSE_ERROR
ALTER DATABASE db CHARSET DEFAULT;

--error ER_PARSE_ERROR
ALTER DATABASE db COLLATE DEFAULT;

--error ER_PARSE_ERROR
ALTER TABLE t COLLATE DEFAULT;

--error ER_PARSE_ERROR
SET NAMES utf8mb3 COLLATE DEFAULT;

--error ER_PARSE_ERROR
SET NAMES DEFAULT COLLATE DEFAULT;

DELIMITER //;

--error ER_PARSE_ERROR
CREATE PROCEDURE p1()
BEGIN
  DECLARE c CHAR(1) CHARSET DEFAULT;
END//

--error ER_PARSE_ERROR
CREATE PROCEDURE p1()
BEGIN
  DECLARE c CHAR(1) COLLATE DEFAULT;
END//

DELIMITER ;//

--error ER_PARSE_ERROR
CREATE PROCEDURE p1(c CHAR(1) CHARSET DEFAULT) BEGIN END;

--error ER_PARSE_ERROR
CREATE PROCEDURE p1(c CHAR(1) COLLATE DEFAULT) BEGIN END;

--error ER_PARSE_ERROR
CREATE FUNCTION f1(c CHAR(1) CHARSET DEFAULT) RETURNS INT RETURN 1;

--error ER_PARSE_ERROR
CREATE FUNCTION f1(c CHAR(1) COLLATE DEFAULT) RETURNS INT RETURN 1;

--error ER_PARSE_ERROR
CREATE FUNCTION f1() RETURNS CHAR(1) CHARSET DEFAULT RETURN '';

--error ER_PARSE_ERROR
CREATE FUNCTION f1() RETURNS CHAR(1) COLLATE DEFAULT RETURN '';


--echo #
--echo # Bug#27814204: THE "ADMIN" WORD SHOULD BE A NON-RESERVED WORD IN THE
--echo #               SQL GRAMMAR
--echo #

CREATE TEMPORARY TABLE admin (admin INT);
DROP TABLE admin;


--echo #
--echo # Bug#27760787: ERROR IN SQL SYNTAX WHEN USING "DEFAULT" KEYWORD IN
--echo #               ALTER TABLE COMMAND

SELECT @@default_collation_for_utf8mb4;

--echo #########################################################################
--echo #
--echo # 1. @@default_collation_for_utf8mb4 does not matter:
--echo #
--echo #########################################################################

CREATE DATABASE db1 CHARSET cp1251 COLLATE cp1251_general_ci;
USE db1;
CREATE TABLE t1 (i INT) CHARSET utf8mb4;

--echo #
--echo # Implicit COLLATE:
--echo #

ALTER TABLE t1 CONVERT TO CHARACTER SET DEFAULT;
SHOW CREATE TABLE t1;

--echo #
--echo # Explicit COLLATE:
--echo #

ALTER TABLE t1 CONVERT TO CHARACTER SET DEFAULT COLLATE cp1251_bin;
SHOW CREATE TABLE t1;

DROP DATABASE db1;

--echo #########################################################################
--echo #
--echo # 2. @@default_collation_for_utf8mb4 == utf8mb4_general_ci
--echo #    @@collation_database            == utf8mb4_0900_ai_ci
--echo #
--echo #########################################################################

SET @@default_collation_for_utf8mb4 = utf8mb4_general_ci;

CREATE DATABASE db2 COLLATE utf8mb4_0900_ai_ci;
USE db2;

CREATE TABLE t2 (i INT) CHARSET latin1;

--echo #
--echo # Implicit COLLATE result in utf8mb4_0900_ai_ci (@@collation_database):
--echo #

ALTER TABLE t2 CONVERT TO CHARACTER SET DEFAULT;
SHOW CREATE TABLE t2;

--echo # Cleanup:
ALTER TABLE t2 CONVERT TO CHARACTER SET latin1;

--echo #
--echo # Explicit COLLATE should result in utf8mb4_bin:
--echo #

ALTER TABLE t2 CONVERT TO CHARACTER SET DEFAULT COLLATE utf8mb4_bin;
SHOW CREATE TABLE t2;

DROP DATABASE db2;

--echo #########################################################################
--echo #
--echo # 3. @@default_collation_for_utf8mb4 == utf8mb4_0900_ai_ci
--echo #    @@collation_database            == utf8mb4_general_ci
--echo #
--echo #########################################################################

SET @@default_collation_for_utf8mb4 = DEFAULT;
SELECT @@default_collation_for_utf8mb4;

CREATE DATABASE db3 COLLATE utf8mb4_general_ci;
USE db3;

CREATE TABLE t3 (i INT) CHARSET latin1;

--echo #
--echo # Implicit COLLATE should result in utf8mb4_general_ci (@@collation_database):
--echo #

ALTER TABLE t3 CONVERT TO CHARACTER SET DEFAULT;
SHOW CREATE TABLE t3;

--echo #
--echo # Explicit COLLATE should result in utf8mb4_bin:
--echo #

ALTER TABLE t3 CONVERT TO CHARACTER SET DEFAULT COLLATE utf8mb4_bin;
SHOW CREATE TABLE t3;

--echo #########################################################################
--echo #
--echo # 4.  Incompatible character set in @@character_set_database and COLLATE should fail:
--echo #
--echo #########################################################################

--error ER_COLLATION_CHARSET_MISMATCH
ALTER TABLE t3 CONVERT TO CHARACTER SET DEFAULT COLLATE cp1251_general_cs;

DROP DATABASE db3;

--echo # Cleanup

USE test;

--echo
--echo Bug #27714748: @@PARSER_MAX_MEM_SIZE DOES NOT WORK FOR ROUTINES
--echo

SET parser_max_mem_size = 10000000; # minimum allowed value
--let $s = `SELECT REPEAT('x', @@parser_max_mem_size)`
--disable_query_log
--error ER_CAPACITY_EXCEEDED
--eval CREATE PROCEDURE p() SELECT 1 FROM (SELECT '$s') a
--enable_query_log
SET parser_max_mem_size = default;

--echo #
--echo # Bug #28968848: SIMPLIFY MYSQL_YYABORT
--echo #

--error ER_PARSE_ERROR
CREATE PROCEDURE p1 () wrong syntax;

--echo #
--echo # Bug #25220656: THE "PERSIST" EXTENSION IS A RESERVED KEYWORD
--echo #

# PERSIST and PERSIST_ONLY are not reserved words any more:

SELECT 1 AS PERSIST, 2 AS PERSIST_ONLY;

# Negative checks for non-reserved words not allowed as role names:

--error ER_PARSE_ERROR
CREATE ROLE EVENT;
--error ER_PARSE_ERROR
CREATE ROLE FILE;
--error ER_PARSE_ERROR
CREATE ROLE NONE;
--error ER_PARSE_ERROR
CREATE ROLE PROCESS;
--error ER_PARSE_ERROR
CREATE ROLE PROXY;
--error ER_PARSE_ERROR
CREATE ROLE RELOAD;
--error ER_PARSE_ERROR
CREATE ROLE REPLICATION;
--error ER_PARSE_ERROR
CREATE ROLE RESOURCE;
--error ER_PARSE_ERROR
CREATE ROLE SUPER;

# Negative checks for non-reserved words not allowed as system variable names:

--error ER_PARSE_ERROR
SET GLOBAL = DEFAULT;
--error ER_PARSE_ERROR
SET LOCAL = DEFAULT;
--error ER_PARSE_ERROR
SET PERSIST = DEFAULT;
--error ER_PARSE_ERROR
SET PERSIST_ONLY = DEFAULT;
--error ER_PARSE_ERROR
SET SESSION = DEFAULT;
--echo #
--echo # Bug#29033659: SOME NON-RESERVED WORDS CAN'T BE USED AS SP LABELS
--echo #

DELIMITER |;

CREATE FUNCTION f1() RETURNS INT
BEGIN
  ACCOUNT: LOOP RETURN 1; END LOOP;
  ALWAYS: LOOP RETURN 1; END LOOP;
  BACKUP: LOOP RETURN 1; END LOOP;
  CLOSE: LOOP RETURN 1; END LOOP;
  FORMAT: LOOP RETURN 1; END LOOP;
  GROUP_REPLICATION: LOOP RETURN 1; END LOOP;
  HOST: LOOP RETURN 1; END LOOP;
  INVISIBLE: LOOP RETURN 1; END LOOP;
  OPEN: LOOP RETURN 1; END LOOP;
  OPTIONS: LOOP RETURN 1; END LOOP;
  OWNER: LOOP RETURN 1; END LOOP;
  PARSER: LOOP RETURN 1; END LOOP;
  PORT: LOOP RETURN 1; END LOOP;
  REMOVE: LOOP RETURN 1; END LOOP;
  RESTORE: LOOP RETURN 1; END LOOP;
  ROLE: LOOP RETURN 1; END LOOP;
  SECONDARY: LOOP RETURN 1; END LOOP;
  SECONDARY_ENGINE: LOOP RETURN 1; END LOOP;
  SECONDARY_LOAD: LOOP RETURN 1; END LOOP;
  SECONDARY_UNLOAD: LOOP RETURN 1; END LOOP;
  SECURITY: LOOP RETURN 1; END LOOP;
  SERVER: LOOP RETURN 1; END LOOP;
  SOCKET: LOOP RETURN 1; END LOOP;
  SONAME: LOOP RETURN 1; END LOOP;
  UPGRADE: LOOP RETURN 1; END LOOP;
  VISIBLE: LOOP RETURN 1; END LOOP;
  WRAPPER: LOOP RETURN 1; END LOOP;
END|

DELIMITER ;|

DROP FUNCTION f1;

--echo #
--echo # Bug#29205289: PARSER ACCEPTS UNDOCUMENTED SYNTAX: `... = TABLE_ALIAS`
--echo #

CREATE TABLE t1 (i INT);
--error ER_PARSE_ERROR
LOCK TABLES t1=a READ;
--error ER_PARSE_ERROR
HANDLER t1 OPEN=a;
--error ER_PARSE_ERROR
SELECT * FROM t1=a;
--error ER_PARSE_ERROR
SELECT * FROM (SELECT 1)=a;
--error ER_PARSE_ERROR
SELECT * FROM t1 JOIN t1=a;
--error ER_PARSE_ERROR
UPDATE t1=a SET i=0;
DROP TABLE t1;

--echo #
--echo # Bug #22320942: ODBC OUTER JOIN ESCAPE SEQUENCE SYNTAX IS BROKEN
--echo #

CREATE TEMPORARY TABLE t1 (i INT);
CREATE TEMPORARY TABLE t2 (i INT);

SELECT * FROM { OJ t1 LEFT JOIN t2 ON TRUE };

--error ER_PARSE_ERROR
SELECT * FROM { `OJ` t1 LEFT JOIN t2 ON TRUE };

--error ER_PARSE_ERROR
SELECT * FROM { random_identifier t1 LEFT JOIN t2 ON TRUE };

DROP TABLE t1, t2;

--echo #
--echo # Bug #28997518: "COLLATE X GENERATED ALWAYS ... COLLATE Y": X HAS
--echo #                 A PRIORITY OVER Y
--echo #

# Even compatible multiple COLLATE clauses should fail:

--error ER_PARSE_ERROR
CREATE TABLE t1 (
  x VARCHAR(10)
  COLLATE ascii_bin
  COLLATE ascii_bin
);

# Incompatible COLLATE clauses should fail with the same error message:

--error ER_PARSE_ERROR
CREATE TABLE t2 (
  x VARCHAR(10)
  COLLATE ascii_bin
  COLLATE ascii_general_ci
);

# Multiple COLLATE clauses mixed with other column attributes should fail too:

--error ER_PARSE_ERROR
CREATE TABLE t3 (
  x VARCHAR(10)
  COLLATE ascii_bin
  NOT NULL
  COLLATE ascii_bin
);

# Multiple COLLATE clauses should fail in gcol definitions (syntax 1):

--error ER_PARSE_ERROR
CREATE TABLE t3 (
  x VARCHAR(10)
  COLLATE ascii_bin
  GENERATED ALWAYS AS(NULL)
  COLLATE ascii_bin
);

# Multiple COLLATE clauses should fail in gcol definitions (syntax 2):

--error ER_PARSE_ERROR
CREATE TABLE t4 (
  x VARCHAR(10)
  GENERATED ALWAYS AS(NULL)
  COLLATE ascii_bin
  COLLATE ascii_bin
);

--echo #
--echo # Bug#28961997: REMOVE PARENTHESES INFORMATION FROM AST
--echo #

CREATE VIEW v1 AS (SELECT 1 ORDER BY 1) UNION (SELECT 3 ORDER BY 1) ORDER BY 1;
SHOW CREATE VIEW v1;
DROP VIEW v1;

--echo #
--echo # Bug#29871803: REQUIRED PARENTHESES NO LONGER PRINTED FOR CERTAIN UNION
--echo # STATEMENTS
--echo #
CREATE TABLE t1(a INTEGER);
CREATE TABLE t2(a INTEGER);
CREATE TABLE t3(a INTEGER, b INTEGER, c INTEGER);
INSERT INTO t3 VALUES(1, 10, 100), (2, 20, 200), (3, 30, 300), (4, 40, 400);
ANALYZE TABLE t1, t2, t3;

EXPLAIN (SELECT a FROM t1 ORDER BY a LIMIT 1) UNION SELECT a FROM t2;
EXPLAIN SELECT a FROM t1 UNION (SELECT a FROM t2 LIMIT 1);

CREATE VIEW v1 as (SELECT a FROM t1 ORDER BY a LIMIT 1) UNION SELECT a FROM t2;
SHOW CREATE VIEW v1;

CREATE VIEW v2 as SELECT a FROM t1 UNION (SELECT a FROM t2 ORDER BY a LIMIT 1);
SHOW CREATE VIEW v2;

EXPLAIN (SELECT * FROM t3 ORDER BY a LIMIT 3) ORDER BY b DESC LIMIT 2;

EXPLAIN ((SELECT * FROM t3 ORDER BY a LIMIT 3)
            ORDER BY b DESC LIMIT 2)
                ORDER BY c LIMIT 1;

DROP VIEW v1;
DROP VIEW v2;
DROP TABLE t1, t2, t3;

--echo #
--echo # Bug#30131161: SYNTAX ERROR WITH || WHEN USING PIPES_AS_CONCAT AFTER
--echo #               UPGRADING TO 8.0.17
--echo #

SET sql_mode=(SELECT CONCAT(@@sql_mode, ',PIPES_AS_CONCAT'));

# `||` should have higher precedence than `LIKE` and `ESCAPE`:

SELECT 'ab' LIKE 'a%', 'ab' LIKE 'a' || '%';
SELECT 'ab' NOT LIKE 'a%', 'ab' NOT LIKE 'a' || '%';

SELECT 'ab' LIKE 'ac', 'ab' LIKE 'a' || 'c';
SELECT 'ab' NOT LIKE 'ac', 'ab' NOT LIKE 'a' || 'c';

SELECT 'a%' LIKE 'a!%' ESCAPE '!', 'a%' LIKE 'a!' || '%' ESCAPE '!';
SELECT 'a%' NOT LIKE 'a!%' ESCAPE '!', 'a%' NOT LIKE 'a!' || '%' ESCAPE '!';

SELECT 'a%' LIKE 'a!%' ESCAPE '$', 'a%' LIKE 'a!' || '%' ESCAPE '$';
SELECT 'a%' NOT LIKE 'a!%' ESCAPE '$', 'a%' NOT LIKE 'a!' || '%' ESCAPE '$';

SELECT 'a%' LIKE 'a!%' ESCAPE '!', 'a%' LIKE 'a!%' ESCAPE '' || '!';
SELECT 'a%' NOT LIKE 'a!%' ESCAPE '!', 'a%' NOT LIKE 'a!%' ESCAPE '' || '!';

SELECT 'a%' LIKE 'a!%' ESCAPE '' || '$', 'a%' LIKE 'a!%' ESCAPE '' || '$';
SELECT 'a%' NOT LIKE 'a!%' ESCAPE '' || '$', 'a%' NOT LIKE 'a!%' ESCAPE '' || '$';

# `||` should have a higher precedence than `^`:

SELECT 1 ^ 100, 1 ^ '10' || '0';

# `||` should have a lower precedence than the unary `-`:

SELECT -1 || '0';

SET sql_mode=DEFAULT;

--echo #
--echo # WL#13559: Deprecate syntax: confusing combinations of UNION and INTO
--echo #

--echo #
--echo # No warning expected:
--echo #

SELECT 1 UNION SELECT 1 INTO @var;
(SELECT 1 UNION SELECT 1 INTO @var);

SELECT 1 UNION SELECT 1 FROM DUAL INTO @var;
(SELECT 1 UNION SELECT 1 FROM DUAL INTO @var);

SELECT 1 UNION SELECT 1 FROM DUAL FOR UPDATE INTO @var;
(SELECT 1 UNION SELECT 1 FROM DUAL FOR UPDATE INTO @var);

--echo # Check that this also works with flatten_equal_set_ops
--echo # (minimum three operands to check this)
SELECT 1 UNION SELECT 1 UNION SELECT 1 INTO @var;
(SELECT 1 UNION SELECT 1 UNION SELECT 1 INTO @var);

SELECT 1 UNION SELECT 1 UNION SELECT 1 FROM DUAL INTO @var;
(SELECT 1 UNION SELECT 1 UNION SELECT 1 FROM DUAL INTO @var);

SELECT 1 UNION SELECT 1 UNION SELECT 1 FROM DUAL FOR UPDATE INTO @var;
(SELECT 1 UNION SELECT 1 UNION SELECT 1 FROM DUAL FOR UPDATE INTO @var);

--echo #
--echo # Deprecation warning expected:
--echo #

SELECT 1 UNION SELECT 1 INTO @var FROM DUAL;
SELECT 1 UNION (SELECT 1 INTO @var FROM DUAL);

SELECT 1 UNION SELECT 1 FROM DUAL INTO @var FOR UPDATE;
(SELECT 1 UNION SELECT 1 FROM DUAL INTO @var FOR UPDATE);

SELECT 1 UNION SELECT 1 INTO @var FOR UPDATE;
(SELECT 1 UNION SELECT 1 INTO @var FOR UPDATE);

--echo # Check that warning also works with flatten_equal_set_ops
--echo # (minimum three operands to check this)
SELECT 1 UNION SELECT 1 UNION SELECT 1 INTO @var FROM DUAL;
SELECT 1 UNION SELECT 1 UNION (SELECT 1 INTO @var FROM DUAL);

SELECT 1 UNION SELECT 1 UNION SELECT 1 FROM DUAL INTO @var FOR UPDATE;
(SELECT 1 UNION SELECT 1 UNION SELECT 1 FROM DUAL INTO @var FOR UPDATE);

SELECT 1 UNION SELECT 1 UNION SELECT 1 INTO @var FOR UPDATE;
(SELECT 1 UNION SELECT 1 UNION SELECT 1 INTO @var FOR UPDATE);

--echo # Check that PT_set_operation::has_into_clause works correctly
--echo # for more than two operands after flatten_equal_set_ops
--error ER_MISPLACED_INTO
SELECT 1 UNION (SELECT 1 INTO @var FROM DUAL) UNION SELECT 1;
--error ER_MISPLACED_INTO
SELECT 1 UNION SELECT 1 UNION SELECT * FROM (SELECT 2 UNION SELECT 1 INTO @var FROM DUAL) t;
--error ER_MISPLACED_INTO
SELECT 1 UNION SELECT 1 UNION SELECT * FROM (SELECT 1 UNION SELECT 1 UNION SELECT 1 INTO @var FROM DUAL) t;

--echo #
--echo # Syntax error expected:
--echo #

--error ER_PARSE_ERROR
SELECT 1 UNION (SELECT 1 FROM DUAL INTO @var);

--echo #
--echo # Bug#30871301: CTE CAN FAIL WITH A PARSE ERROR ON PSEUDO-COMMENTS
--echo #

WITH cte AS (SELECT 0 /*! ) */ SELECT * FROM cte a, cte b;
WITH cte AS /*! ( */ SELECT 0) SELECT * FROM cte a, cte b;

--echo #
--echo # Bug#30528450: SPECIAL SYMBOL NAMED COLUMN NOT HONORED IN A SELECT
--echo #

CREATE TABLE t1 (c1 INT, `*` INT, c3 INT);
INSERT INTO t1 VALUES (1, 2, 3);

SELECT `*` FROM t1;
SELECT t1.`*`, t1.* FROM t1;
SELECT test.t1.`*`, test.t1.* FROM t1;

DROP TABLE t1;

--echo #
--echo # Bug#30592703: UNDOCUMENTED SYNTAX
--echo #               '('<QUERY BLOCK>')' {LIMIT|ORDER [LIMIT]} INTO ...
--echo #

(SELECT 1) LIMIT 1 INTO @var;
SELECT @var;

(SELECT 2 AS c) ORDER BY c INTO @var;
SELECT @var;

(SELECT 3 AS c) ORDER BY c LIMIT 1 INTO @var;
SELECT @var;

(SELECT 4) INTO @var;
SELECT @var;

--echo #
--echo # <time zone> syntax.
--echo #

SET time_zone = '+01:00';

--error ER_NOT_SUPPORTED_YET
SELECT cast( DATE'2019-10-10' AT LOCAL AS DATETIME );
--error ER_INVALID_CAST
SELECT cast( DATE'2019-10-10' AT TIME ZONE 'UTC' AS DATETIME );

--error ER_NOT_SUPPORTED_YET
SELECT cast( TIME'10:10' AT LOCAL AS DATETIME );
--error ER_INVALID_CAST
SELECT cast( TIME'10:10' AT TIME ZONE '+01:00' AS DATETIME );
--error ER_INVALID_CAST
SELECT cast( TIME'10:10' AT TIME ZONE INTERVAL 'UTC' AS DATETIME );
--error ER_INVALID_CAST
SELECT cast( TIME'10:10' AT TIME ZONE '' AS DATETIME );
--error ER_INVALID_CAST
SELECT cast( TIME'10:10' AT TIME ZONE INTERVAL '' AS DATETIME );

--error ER_NOT_SUPPORTED_YET
SELECT cast( TIMESTAMP'2019-10-10 10:11:12' AT LOCAL AS DATETIME );

SELECT cast( NULL AT TIME ZONE 'UTC' AS DATETIME );

--echo # Casting to prohibited types
--error ER_PARSE_ERROR
SELECT cast( TIMESTAMP'2019-10-10 10:40:00' AT TIME ZONE 'UTC' AS BINARY );
--error ER_PARSE_ERROR
SELECT cast( TIMESTAMP'2019-10-10 10:40:00' AT TIME ZONE 'UTC' AS CHAR );
--error ER_PARSE_ERROR
SELECT cast( TIMESTAMP'2019-10-10 10:40:00' AT TIME ZONE 'UTC' AS DATE );
--error ER_PARSE_ERROR
SELECT cast( TIMESTAMP'2019-10-10 10:40:00' AT TIME ZONE 'UTC' AS DECIMAL );
--error ER_PARSE_ERROR
SELECT cast( TIMESTAMP'2019-10-10 10:40:00' AT TIME ZONE 'UTC' AS DOUBLE );
--error ER_PARSE_ERROR
SELECT cast( TIMESTAMP'2019-10-10 10:40:00' AT TIME ZONE 'UTC' AS FLOAT );
--error ER_PARSE_ERROR
SELECT cast( TIMESTAMP'2019-10-10 10:40:00' AT TIME ZONE 'UTC' AS SIGNED INTEGER );
--error ER_PARSE_ERROR
SELECT cast( TIMESTAMP'2019-10-10 10:40:00' AT TIME ZONE 'UTC' AS TIME );
--error ER_PARSE_ERROR
SELECT cast( TIMESTAMP'2019-10-10 10:40:00' AT TIME ZONE 'UTC' AS UNSIGNED INTEGER );
--error ER_PARSE_ERROR
SELECT cast( TIMESTAMP'2019-10-10 10:40:00' AT TIME ZONE 'UTC' AS JSON );
--error ER_PARSE_ERROR
SELECT cast( TIMESTAMP'2019-10-10 10:40:00' AT TIME ZONE 'UTC' AS REAL );

--echo #
--echo # Bug#32230229: MISMATCH EXPLAIN RESULT TO ACTUAL RUN WITH
--echo #               USER-DEFINED VARIABLE
--echo #

# The next two statements used to give no error when running with
# EXPLAIN. Verify that they give the expected error.
--error ER_MULTIPLE_INTO_CLAUSES
EXPLAIN SELECT 1 INTO @x FROM DUAL INTO @y;
--error ER_MULTIPLE_INTO_CLAUSES
EXPLAIN SELECT 1 INTO DUMPFILE 'file1' FROM DUAL INTO DUMPFILE 'file2';
# The next statement gave the expected error also before the bug was
# fixed. Verify that it still does.
--error ER_MULTIPLE_INTO_CLAUSES
EXPLAIN SELECT 1 INTO OUTFILE 'file1' FROM DUAL INTO OUTFILE 'file2';

--echo #
--echo # Deprecate usage of "full" as unquoted identfier
--echo #
CREATE TABLE full(i INT);
DROP TABLE full;
CREATE TABLE `full`(i INT);
SELECT * from `full`;
SELECT * from `full` AS full;
SELECT * from `full` AS `full`;
SELECT * from full;
SELECT * from full as full;
SELECT * from `full` full;
--error ER_PARSE_ERROR
SELECT * from full outer;

SET @save_sql_mode=@@sql_mode;
SET SQL_MODE = 'ANSI_QUOTES';
SELECT * from `full`;
SELECT * from "full";
SELECT * from full;
SET @@sql_mode=@save_sql_mode;

DROP TABLE `full`;

--echo #
--echo # WL#14189: Replace old terms in CHANGE MASTER TO and START SLAVE params
--echo # Check keywords can still be used in other contexts.
--echo #

CREATE TABLE get_source_public_key(i INT);

CREATE TABLE source_auto_position(i INT);

CREATE TABLE source_bind(i INT);

CREATE TABLE source_compression_algorithm(i INT);

CREATE TABLE source_connect_retry(i INT);

CREATE TABLE source_delay(i INT);

CREATE TABLE source_heartbeat_period(i INT);

CREATE TABLE source_host(i INT);

CREATE TABLE source_log_file(i INT);

CREATE TABLE source_log_pos(i INT);

CREATE TABLE source_password(i INT);

CREATE TABLE source_port(i INT);

CREATE TABLE source_public_key_path(i INT);

CREATE TABLE source_retry_count(i INT);

CREATE TABLE source_ssl(i INT);

CREATE TABLE source_ssl_ca(i INT);

CREATE TABLE source_ssl_capath(i INT);

CREATE TABLE source_ssl_cert(i INT);

CREATE TABLE source_ssl_cipher(i INT);

CREATE TABLE source_ssl_crl(i INT);

CREATE TABLE source_ssl_crlpath(i INT);

CREATE TABLE source_ssl_key(i INT);

CREATE TABLE source_ssl_verify_server_cert(i INT);

CREATE TABLE source_tls_ciphersuites(i INT);

CREATE TABLE source_tls_version(i INT);

CREATE TABLE source_user(i INT);

CREATE TABLE source_zstd_compression_level(i INT);

# Clean up

DROP TABLE get_source_public_key;
DROP TABLE source_auto_position;
DROP TABLE source_bind;
DROP TABLE source_compression_algorithm;
DROP TABLE source_connect_retry;
DROP TABLE source_delay;
DROP TABLE source_heartbeat_period;
DROP TABLE source_host;
DROP TABLE source_log_file;
DROP TABLE source_log_pos;
DROP TABLE source_password;
DROP TABLE source_port;
DROP TABLE source_public_key_path;
DROP TABLE source_retry_count;
DROP TABLE source_ssl;
DROP TABLE source_ssl_ca;
DROP TABLE source_ssl_capath;
DROP TABLE source_ssl_cert;
DROP TABLE source_ssl_cipher;
DROP TABLE source_ssl_crl;
DROP TABLE source_ssl_crlpath;
DROP TABLE source_ssl_key;
DROP TABLE source_ssl_verify_server_cert;
DROP TABLE source_tls_ciphersuites;
DROP TABLE source_tls_version;
DROP TABLE source_user;
DROP TABLE source_zstd_compression_level;

--echo #
--echo # Bug#22574003: Non-reserved "BINLOG" keyword costs 22 shift/reduce
--echo #               conflicts to MySQL grammar
--echo #

# "binlog: is allowed as table and column name
CREATE TABLE binlog(binlog INTEGER);
DROP TABLE binlog;

# "binlog" is allowed as a role name:
CREATE ROLE binlog;
DROP ROLE binlog;

# "binlog" is not allowed as procedure label

DELIMITER //;
--error ER_PARSE_ERROR
CREATE PROCEDURE p()
BEGIN
  DECLARE x INTEGER DEFAULT 3;
binlog:
  LOOP
    IF x = 0 THEN
      LEAVE binlog;
    END IF;
    SET x = x - 1;
  END LOOP binlog;
END;
//
DELIMITER ;//


--echo #
--echo #
--echo #

CREATE TABLE t(pk INT);

--echo #
--echo # 1) Simple comma-separated list exceeding MAX_TABLES (assume MAX_TABLES=61)
--echo # Expect ER_TOO_MANY_TABLES raised by parser.
--echo #

--error ER_TOO_MANY_TABLES
WITH x as (SELECT 1)
SELECT 1
FROM x x,x x0,x x1,x x2,x x3,x x4,x x5,x x6,x x7,x x8,x x9,x x10,
     x x11,x x12,x x13,x x14,x x15,x x16,x x17,x x18,x x19,x x20,
     x x21,x x22,x x23,x x24,x x25,x x26,x x27,x x28,x x29,x x30,
     x x31,x x32,x x33,x x34,x x35,x x36,x x37,x x38,x x39,x x40,
     x x41,x x42,x x43,x x44,x x45,x x46,x x47,x x48,x x49,x x50,
     x x51,x x52,x x53,x x54,x x55,x x56,x x57,x x58,x x59,x x60,
     x x61,x x62,x x63;

--echo #
--echo # 2) Long CROSS JOIN chain exceeding MAX_TABLES
--echo # Expect ER_TOO_MANY_TABLES from parser.
--echo #

--error ER_TOO_MANY_TABLES
SELECT 1 FROM
  t as t1
  CROSS JOIN t AS t2
  CROSS JOIN t AS t3
  CROSS JOIN t AS t4
  CROSS JOIN t AS t5
  CROSS JOIN t AS t6
  CROSS JOIN t AS t7
  CROSS JOIN t AS t8
  CROSS JOIN t AS t9
  CROSS JOIN t AS t10
  CROSS JOIN t AS t11
  CROSS JOIN t AS t12
  CROSS JOIN t AS t13
  CROSS JOIN t AS t14
  CROSS JOIN t AS t15
  CROSS JOIN t AS t16
  CROSS JOIN t AS t17
  CROSS JOIN t AS t18
  CROSS JOIN t AS t19
  CROSS JOIN t AS t20
  CROSS JOIN t AS t21
  CROSS JOIN t AS t22
  CROSS JOIN t AS t23
  CROSS JOIN t AS t24
  CROSS JOIN t AS t25
  CROSS JOIN t AS t26
  CROSS JOIN t AS t27
  CROSS JOIN t AS t28
  CROSS JOIN t AS t29
  CROSS JOIN t AS t30
  CROSS JOIN t AS t31
  CROSS JOIN t AS t32
  CROSS JOIN t AS t33
  CROSS JOIN t AS t34
  CROSS JOIN t AS t35
  CROSS JOIN t AS t36
  CROSS JOIN t AS t37
  CROSS JOIN t AS t38
  CROSS JOIN t AS t39
  CROSS JOIN t AS t40
  CROSS JOIN t AS t41
  CROSS JOIN t AS t42
  CROSS JOIN t AS t43
  CROSS JOIN t AS t44
  CROSS JOIN t AS t45
  CROSS JOIN t AS t46
  CROSS JOIN t AS t47
  CROSS JOIN t AS t48
  CROSS JOIN t AS t49
  CROSS JOIN t AS t50
  CROSS JOIN t AS t51
  CROSS JOIN t AS t52
  CROSS JOIN t AS t53
  CROSS JOIN t AS t54
  CROSS JOIN t AS t55
  CROSS JOIN t AS t56
  CROSS JOIN t AS t57
  CROSS JOIN t AS t58
  CROSS JOIN t AS t59
  CROSS JOIN t AS t60
  CROSS JOIN t AS t61
  CROSS JOIN t AS t62
  CROSS JOIN t AS t63;

--echo #
--echo # 3) Mixed comma and JOIN groups exceeding MAX_TABLES
--echo # Using CROSS JOIN inside parentheses to avoid ON-clause verbosity.
--echo # Expect ER_TOO_MANY_TABLES from parser.
--echo #

--error ER_TOO_MANY_TABLES
SELECT 1 FROM
  (t AS t1 CROSS JOIN t AS t2),   (t AS t3 CROSS JOIN t AS t4),
  (t AS t5 CROSS JOIN t AS t6),   (t AS t7 CROSS JOIN t AS t8),
  (t AS t9 CROSS JOIN t AS t10),  (t AS t11 CROSS JOIN t AS t12),
  (t AS t13 CROSS JOIN t AS t14), (t AS t15 CROSS JOIN t AS t16),
  (t AS t17 CROSS JOIN t AS t18), (t AS t19 CROSS JOIN t AS t20),
  (t AS t21 CROSS JOIN t AS t22), (t AS t23 CROSS JOIN t AS t24),
  (t AS t25 CROSS JOIN t AS t26), (t AS t27 CROSS JOIN t AS t28),
  (t AS t29 CROSS JOIN t AS t30), (t AS t31 CROSS JOIN t AS t32),
  (t AS t33 CROSS JOIN t AS t34), (t AS t35 CROSS JOIN t AS t36),
  (t AS t37 CROSS JOIN t AS t38), (t AS t39 CROSS JOIN t AS t40),
  (t AS t41 CROSS JOIN t AS t42), (t AS t43 CROSS JOIN t AS t44),
  (t AS t45 CROSS JOIN t AS t46), (t AS t47 CROSS JOIN t AS t48),
  (t AS t49 CROSS JOIN t AS t50), (t AS t51 CROSS JOIN t AS t52),
  (t AS t53 CROSS JOIN t AS t54), (t AS t55 CROSS JOIN t AS t56),
  (t AS t57 CROSS JOIN t AS t58), (t AS t59 CROSS JOIN t AS t60),
  (t AS t61 CROSS JOIN t AS t62);

--echo #
--echo # 4) Nested joins exceeding MAX_TABLES
--echo # Build a deeply nested expression; expect ER_TOO_MANY_TABLES from parser.
--echo #

--error ER_TOO_MANY_TABLES
SELECT 1 FROM
  ((((t AS t1 CROSS JOIN t AS t2) CROSS JOIN t AS t3) CROSS JOIN t AS t4)
    CROSS JOIN ((((t AS t5 CROSS JOIN t AS t6) CROSS JOIN t AS t7) CROSS JOIN t AS t8)
      CROSS JOIN ((((t AS t9 CROSS JOIN t AS t10) CROSS JOIN t AS t11) CROSS JOIN t AS t12)
        CROSS JOIN ((((t AS t13 CROSS JOIN t AS t14) CROSS JOIN t AS t15) CROSS JOIN t AS t16)
          CROSS JOIN ((((t AS t17 CROSS JOIN t AS t18) CROSS JOIN t AS t19) CROSS JOIN t AS t20)
            CROSS JOIN ((((t AS t21 CROSS JOIN t AS t22) CROSS JOIN t AS t23) CROSS JOIN t AS t24)
              CROSS JOIN ((((t AS t25 CROSS JOIN t AS t26) CROSS JOIN t AS t27) CROSS JOIN t AS t28)
                CROSS JOIN ((((t AS t29 CROSS JOIN t AS t30) CROSS JOIN t AS t31) CROSS JOIN t AS t32)
                  CROSS JOIN ((((t AS t33 CROSS JOIN t AS t34) CROSS JOIN t AS t35) CROSS JOIN t AS t36)
                    CROSS JOIN ((((t AS t37 CROSS JOIN t AS t38) CROSS JOIN t AS t39) CROSS JOIN t AS t40)
                      CROSS JOIN ((((t AS t41 CROSS JOIN t AS t42) CROSS JOIN t AS t43) CROSS JOIN t AS t44)
                        CROSS JOIN ((((t AS t45 CROSS JOIN t AS t46) CROSS JOIN t AS t47) CROSS JOIN t AS t48)
                          CROSS JOIN ((((t AS t49 CROSS JOIN t AS t50) CROSS JOIN t AS t51) CROSS JOIN t AS t52)
                            CROSS JOIN ((((t AS t53 CROSS JOIN t AS t54) CROSS JOIN t AS t55) CROSS JOIN t AS t56)
                              CROSS JOIN (t AS t57 CROSS JOIN t AS t58 CROSS JOIN t AS t59
                              CROSS JOIN t AS t60 CROSS JOIN t AS t61 CROSS JOIN t AS t62 CROSS JOIN t AS t63)))))))))))))));

--echo #

SELECT COUNT(*) = 0 FROM
  (SELECT t1.pk FROM
     t AS t1, t AS t2, t AS t3, t AS t4, t AS t5,
     t AS t6, t AS t7, t AS t8, t AS t9, t AS t10,
     t AS t11, t AS t12, t AS t13, t AS t14, t AS t15,
     t AS t16, t AS t17, t AS t18, t AS t19, t AS t20,
     t AS t21, t AS t22, t AS t23, t AS t24, t AS t25,
     t AS t26, t AS t27, t AS t28, t AS t29, t AS t30) AS d1
, (SELECT t31.pk FROM
     t AS t31, t AS t32, t AS t33, t AS t34, t AS t35,
     t AS t36, t AS t37, t AS t38, t AS t39, t AS t40,
     t AS t41, t AS t42, t AS t43, t AS t44, t AS t45,
     t AS t46, t AS t47, t AS t48, t AS t49, t AS t50,
     t AS t51, t AS t52, t AS t53, t AS t54, t AS t55,
     t AS t56, t AS t57, t AS t58, t AS t59, t AS t60) AS d2;

--echo #

--error ER_TOO_MANY_TABLES
SELECT 1 FROM t AS t1
WHERE EXISTS (
  SELECT 1 FROM
    t AS t1, t AS t2, t AS t3, t AS t4, t AS t5,
    t AS t6, t AS t7, t AS t8, t AS t9, t AS t10,
    t AS t11, t AS t12, t AS t13, t AS t14, t AS t15,
    t AS t16, t AS t17, t AS t18, t AS t19, t AS t20,
    t AS t21, t AS t22, t AS t23, t AS t24, t AS t25,
    t AS t26, t AS t27, t AS t28, t AS t29, t AS t30,
    t AS t31, t AS t32, t AS t33, t AS t34, t AS t35,
    t AS t36, t AS t37, t AS t38, t AS t39, t AS t40,
    t AS t41, t AS t42, t AS t43, t AS t44, t AS t45,
    t AS t46, t AS t47, t AS t48, t AS t49, t AS t50,
    t AS t51, t AS t52, t AS t53, t AS t54, t AS t55,
    t AS t56, t AS t57, t AS t58, t AS t59, t AS t60,
    t AS t61, t AS t62, t AS t63
);

--echo #
--echo # Extremely deep nesting
--echo #

--disable_result_log
--error ER_STACK_OVERRUN_NEED_MORE
SELECT *
FROM t t0 JOIN t t1 ON t JOIN t t2 ON t JOIN t t3 ON t JOIN t t4 ON t
 JOIN t t5 ON t JOIN t t6 ON t JOIN t t7 ON t JOIN t t8 ON t
 JOIN t t9 ON t JOIN t t10 ON t JOIN t t11 ON t JOIN t t12 ON t
 JOIN t t13 ON t JOIN t t14 ON t JOIN t t15 ON t JOIN t t16 ON t
 JOIN t t17 ON t JOIN t t18 ON t JOIN t t19 ON t JOIN t t20 ON t
 JOIN t t21 ON t JOIN t t22 ON t JOIN t t23 ON t JOIN t t24 ON t
 JOIN t t25 ON t JOIN t t26 ON t JOIN t t27 ON t JOIN t t28 ON t
 JOIN t t29 ON t JOIN t t30 ON t JOIN t t31 ON t JOIN t t32 ON t
 JOIN t t33 ON t JOIN t t34 ON t JOIN t t35 ON t JOIN t t36 ON t
 JOIN t t37 ON t JOIN t t38 ON t JOIN t t39 ON t JOIN t t40 ON t
 JOIN t t41 ON t JOIN t t42 ON t JOIN t t43 ON t JOIN t t44 ON t
 JOIN t t45 ON t JOIN t t46 ON t JOIN t t47 ON t JOIN t t48 ON t
 JOIN t t49 ON t JOIN t t50 ON t JOIN t t51 ON t JOIN t t52 ON t
 JOIN t t53 ON t JOIN t t54 ON t JOIN t t55 ON t JOIN t t56 ON t
 JOIN t t57 ON t JOIN t t58 ON t JOIN t t59 ON t JOIN t t60 ON t
 JOIN t t61 ON t JOIN t t62 ON t JOIN t t63 ON t JOIN t t64 ON t
 JOIN t t65 ON t JOIN t t66 ON t JOIN t t67 ON t JOIN t t68 ON t
 JOIN t t69 ON t JOIN t t70 ON t JOIN t t71 ON t JOIN t t72 ON t
 JOIN t t73 ON t JOIN t t74 ON t JOIN t t75 ON t JOIN t t76 ON t
 JOIN t t77 ON t JOIN t t78 ON t JOIN t t79 ON t JOIN t t80 ON t
 JOIN t t81 ON t JOIN t t82 ON t JOIN t t83 ON t JOIN t t84 ON t
 JOIN t t85 ON t JOIN t t86 ON t JOIN t t87 ON t JOIN t t88 ON t
 JOIN t t89 ON t JOIN t t90 ON t JOIN t t91 ON t JOIN t t92 ON t
 JOIN t t93 ON t JOIN t t94 ON t JOIN t t95 ON t JOIN t t96 ON t
 JOIN t t97 ON t JOIN t t98 ON t JOIN t t99 ON t JOIN t t100 ON t
 JOIN t t101 ON t JOIN t t102 ON t JOIN t t103 ON t JOIN t t104 ON t
 JOIN t t105 ON t JOIN t t106 ON t JOIN t t107 ON t JOIN t t108 ON t
 JOIN t t109 ON t JOIN t t110 ON t JOIN t t111 ON t JOIN t t112 ON t
 JOIN t t113 ON t JOIN t t114 ON t JOIN t t115 ON t JOIN t t116 ON t
 JOIN t t117 ON t JOIN t t118 ON t JOIN t t119 ON t JOIN t t120 ON t
 JOIN t t121 ON t JOIN t t122 ON t JOIN t t123 ON t JOIN t t124 ON t
 JOIN t t125 ON t JOIN t t126 ON t JOIN t t127 ON t JOIN t t128 ON t
 JOIN t t129 ON t JOIN t t130 ON t JOIN t t131 ON t JOIN t t132 ON t
 JOIN t t133 ON t JOIN t t134 ON t JOIN t t135 ON t JOIN t t136 ON t
 JOIN t t137 ON t JOIN t t138 ON t JOIN t t139 ON t JOIN t t140 ON t
 JOIN t t141 ON t JOIN t t142 ON t JOIN t t143 ON t JOIN t t144 ON t
 JOIN t t145 ON t JOIN t t146 ON t JOIN t t147 ON t JOIN t t148 ON t
 JOIN t t149 ON t JOIN t t150 ON t JOIN t t151 ON t JOIN t t152 ON t
 JOIN t t153 ON t JOIN t t154 ON t JOIN t t155 ON t JOIN t t156 ON t
 JOIN t t157 ON t JOIN t t158 ON t JOIN t t159 ON t JOIN t t160 ON t
 JOIN t t161 ON t JOIN t t162 ON t JOIN t t163 ON t JOIN t t164 ON t
 JOIN t t165 ON t JOIN t t166 ON t JOIN t t167 ON t JOIN t t168 ON t
 JOIN t t169 ON t JOIN t t170 ON t JOIN t t171 ON t JOIN t t172 ON t
 JOIN t t173 ON t JOIN t t174 ON t JOIN t t175 ON t JOIN t t176 ON t
 JOIN t t177 ON t JOIN t t178 ON t JOIN t t179 ON t JOIN t t180 ON t
 JOIN t t181 ON t JOIN t t182 ON t JOIN t t183 ON t JOIN t t184 ON t
 JOIN t t185 ON t JOIN t t186 ON t JOIN t t187 ON t JOIN t t188 ON t
 JOIN t t189 ON t JOIN t t190 ON t JOIN t t191 ON t JOIN t t192 ON t
 JOIN t t193 ON t JOIN t t194 ON t JOIN t t195 ON t JOIN t t196 ON t
 JOIN t t197 ON t JOIN t t198 ON t JOIN t t199 ON t JOIN t t200 ON t
 JOIN t t201 ON t JOIN t t202 ON t JOIN t t203 ON t JOIN t t204 ON t
 JOIN t t205 ON t JOIN t t206 ON t JOIN t t207 ON t JOIN t t208 ON t
 JOIN t t209 ON t JOIN t t210 ON t JOIN t t211 ON t JOIN t t212 ON t
 JOIN t t213 ON t JOIN t t214 ON t JOIN t t215 ON t JOIN t t216 ON t
 JOIN t t217 ON t JOIN t t218 ON t JOIN t t219 ON t JOIN t t220 ON t
 JOIN t t221 ON t JOIN t t222 ON t JOIN t t223 ON t JOIN t t224 ON t
 JOIN t t225 ON t JOIN t t226 ON t JOIN t t227 ON t JOIN t t228 ON t
 JOIN t t229 ON t JOIN t t230 ON t JOIN t t231 ON t JOIN t t232 ON t
 JOIN t t233 ON t JOIN t t234 ON t JOIN t t235 ON t JOIN t t236 ON t
 JOIN t t237 ON t JOIN t t238 ON t JOIN t t239 ON t JOIN t t240 ON t
 JOIN t t241 ON t JOIN t t242 ON t JOIN t t243 ON t JOIN t t244 ON t
 JOIN t t245 ON t JOIN t t246 ON t JOIN t t247 ON t JOIN t t248 ON t
 JOIN t t249 ON t JOIN t t250 ON t JOIN t t251 ON t JOIN t t252 ON t
 JOIN t t253 ON t JOIN t t254 ON t JOIN t t255 ON t JOIN t t256 ON t
 JOIN t t257 ON t JOIN t t258 ON t JOIN t t259 ON t JOIN t t260 ON t
 JOIN t t261 ON t JOIN t t262 ON t JOIN t t263 ON t JOIN t t264 ON t
 JOIN t t265 ON t JOIN t t266 ON t JOIN t t267 ON t JOIN t t268 ON t
 JOIN t t269 ON t JOIN t t270 ON t JOIN t t271 ON t JOIN t t272 ON t
 JOIN t t273 ON t JOIN t t274 ON t JOIN t t275 ON t JOIN t t276 ON t
 JOIN t t277 ON t JOIN t t278 ON t JOIN t t279 ON t JOIN t t280 ON t
 JOIN t t281 ON t JOIN t t282 ON t JOIN t t283 ON t JOIN t t284 ON t
 JOIN t t285 ON t JOIN t t286 ON t JOIN t t287 ON t JOIN t t288 ON t
 JOIN t t289 ON t JOIN t t290 ON t JOIN t t291 ON t JOIN t t292 ON t
 JOIN t t293 ON t JOIN t t294 ON t JOIN t t295 ON t JOIN t t296 ON t
 JOIN t t297 ON t JOIN t t298 ON t JOIN t t299 ON t JOIN t t300 ON t
 JOIN t t301 ON t JOIN t t302 ON t JOIN t t303 ON t JOIN t t304 ON t
 JOIN t t305 ON t JOIN t t306 ON t JOIN t t307 ON t JOIN t t308 ON t
 JOIN t t309 ON t JOIN t t310 ON t JOIN t t311 ON t JOIN t t312 ON t
 JOIN t t313 ON t JOIN t t314 ON t JOIN t t315 ON t JOIN t t316 ON t
 JOIN t t317 ON t JOIN t t318 ON t JOIN t t319 ON t JOIN t t320 ON t
 JOIN t t321 ON t JOIN t t322 ON t JOIN t t323 ON t JOIN t t324 ON t
 JOIN t t325 ON t JOIN t t326 ON t JOIN t t327 ON t JOIN t t328 ON t
 JOIN t t329 ON t JOIN t t330 ON t JOIN t t331 ON t JOIN t t332 ON t
 JOIN t t333 ON t JOIN t t334 ON t JOIN t t335 ON t JOIN t t336 ON t
 JOIN t t337 ON t JOIN t t338 ON t JOIN t t339 ON t JOIN t t340 ON t
 JOIN t t341 ON t JOIN t t342 ON t JOIN t t343 ON t JOIN t t344 ON t
 JOIN t t345 ON t JOIN t t346 ON t JOIN t t347 ON t JOIN t t348 ON t
 JOIN t t349 ON t JOIN t t350 ON t JOIN t t351 ON t JOIN t t352 ON t
 JOIN t t353 ON t JOIN t t354 ON t JOIN t t355 ON t JOIN t t356 ON t
 JOIN t t357 ON t JOIN t t358 ON t JOIN t t359 ON t JOIN t t360 ON t
 JOIN t t361 ON t JOIN t t362 ON t JOIN t t363 ON t JOIN t t364 ON t
 JOIN t t365 ON t JOIN t t366 ON t JOIN t t367 ON t JOIN t t368 ON t
 JOIN t t369 ON t JOIN t t370 ON t JOIN t t371 ON t JOIN t t372 ON t
 JOIN t t373 ON t JOIN t t374 ON t JOIN t t375 ON t JOIN t t376 ON t
 JOIN t t377 ON t JOIN t t378 ON t JOIN t t379 ON t JOIN t t380 ON t
 JOIN t t381 ON t JOIN t t382 ON t JOIN t t383 ON t JOIN t t384 ON t
 JOIN t t385 ON t JOIN t t386 ON t JOIN t t387 ON t JOIN t t388 ON t
 JOIN t t389 ON t JOIN t t390 ON t JOIN t t391 ON t JOIN t t392 ON t
 JOIN t t393 ON t JOIN t t394 ON t JOIN t t395 ON t JOIN t t396 ON t
 JOIN t t397 ON t JOIN t t398 ON t JOIN t t399 ON t JOIN t t400 ON t
 JOIN t t401 ON t JOIN t t402 ON t JOIN t t403 ON t JOIN t t404 ON t
 JOIN t t405 ON t JOIN t t406 ON t JOIN t t407 ON t JOIN t t408 ON t
 JOIN t t409 ON t JOIN t t410 ON t JOIN t t411 ON t JOIN t t412 ON t
 JOIN t t413 ON t JOIN t t414 ON t JOIN t t415 ON t JOIN t t416 ON t
 JOIN t t417 ON t JOIN t t418 ON t JOIN t t419 ON t JOIN t t420 ON t
 JOIN t t421 ON t JOIN t t422 ON t JOIN t t423 ON t JOIN t t424 ON t
 JOIN t t425 ON t JOIN t t426 ON t JOIN t t427 ON t JOIN t t428 ON t
 JOIN t t429 ON t JOIN t t430 ON t JOIN t t431 ON t JOIN t t432 ON t
 JOIN t t433 ON t JOIN t t434 ON t JOIN t t435 ON t JOIN t t436 ON t
 JOIN t t437 ON t JOIN t t438 ON t JOIN t t439 ON t JOIN t t440 ON t
 JOIN t t441 ON t JOIN t t442 ON t JOIN t t443 ON t JOIN t t444 ON t
 JOIN t t445 ON t JOIN t t446 ON t JOIN t t447 ON t JOIN t t448 ON t
 JOIN t t449 ON t JOIN t t450 ON t JOIN t t451 ON t JOIN t t452 ON t
 JOIN t t453 ON t JOIN t t454 ON t JOIN t t455 ON t JOIN t t456 ON t
 JOIN t t457 ON t JOIN t t458 ON t JOIN t t459 ON t JOIN t t460 ON t
 JOIN t t461 ON t JOIN t t462 ON t JOIN t t463 ON t JOIN t t464 ON t
 JOIN t t465 ON t JOIN t t466 ON t JOIN t t467 ON t JOIN t t468 ON t
 JOIN t t469 ON t JOIN t t470 ON t JOIN t t471 ON t JOIN t t472 ON t
 JOIN t t473 ON t JOIN t t474 ON t JOIN t t475 ON t JOIN t t476 ON t
 JOIN t t477 ON t JOIN t t478 ON t JOIN t t479 ON t JOIN t t480 ON t
 JOIN t t481 ON t JOIN t t482 ON t JOIN t t483 ON t JOIN t t484 ON t
 JOIN t t485 ON t JOIN t t486 ON t JOIN t t487 ON t JOIN t t488 ON t
 JOIN t t489 ON t JOIN t t490 ON t JOIN t t491 ON t JOIN t t492 ON t
 JOIN t t493 ON t JOIN t t494 ON t JOIN t t495 ON t JOIN t t496 ON t
 JOIN t t497 ON t JOIN t t498 ON t JOIN t t499 ON t JOIN t t500 ON t
 JOIN t t501 ON t JOIN t t502 ON t JOIN t t503 ON t JOIN t t504 ON t
 JOIN t t505 ON t JOIN t t506 ON t JOIN t t507 ON t JOIN t t508 ON t
 JOIN t t509 ON t JOIN t t510 ON t JOIN t t511 ON t JOIN t t512 ON t
 JOIN t t513 ON t JOIN t t514 ON t JOIN t t515 ON t JOIN t t516 ON t
 JOIN t t517 ON t JOIN t t518 ON t JOIN t t519 ON t JOIN t t520 ON t
 JOIN t t521 ON t JOIN t t522 ON t JOIN t t523 ON t JOIN t t524 ON t
 JOIN t t525 ON t JOIN t t526 ON t JOIN t t527 ON t JOIN t t528 ON t
 JOIN t t529 ON t JOIN t t530 ON t JOIN t t531 ON t JOIN t t532 ON t
 JOIN t t533 ON t JOIN t t534 ON t JOIN t t535 ON t JOIN t t536 ON t
 JOIN t t537 ON t JOIN t t538 ON t JOIN t t539 ON t JOIN t t540 ON t
 JOIN t t541 ON t JOIN t t542 ON t JOIN t t543 ON t JOIN t t544 ON t
 JOIN t t545 ON t JOIN t t546 ON t JOIN t t547 ON t JOIN t t548 ON t
 JOIN t t549 ON t JOIN t t550 ON t JOIN t t551 ON t JOIN t t552 ON t
 JOIN t t553 ON t JOIN t t554 ON t JOIN t t555 ON t JOIN t t556 ON t
 JOIN t t557 ON t JOIN t t558 ON t JOIN t t559 ON t JOIN t t560 ON t
 JOIN t t561 ON t JOIN t t562 ON t JOIN t t563 ON t JOIN t t564 ON t
 JOIN t t565 ON t JOIN t t566 ON t JOIN t t567 ON t JOIN t t568 ON t
 JOIN t t569 ON t JOIN t t570 ON t JOIN t t571 ON t JOIN t t572 ON t
 JOIN t t573 ON t JOIN t t574 ON t JOIN t t575 ON t JOIN t t576 ON t
 JOIN t t577 ON t JOIN t t578 ON t JOIN t t579 ON t JOIN t t580 ON t
 JOIN t t581 ON t JOIN t t582 ON t JOIN t t583 ON t JOIN t t584 ON t
 JOIN t t585 ON t JOIN t t586 ON t JOIN t t587 ON t JOIN t t588 ON t
 JOIN t t589 ON t JOIN t t590 ON t JOIN t t591 ON t JOIN t t592 ON t
 JOIN t t593 ON t JOIN t t594 ON t JOIN t t595 ON t JOIN t t596 ON t
 JOIN t t597 ON t JOIN t t598 ON t JOIN t t599 ON t JOIN t t600 ON t
 JOIN t t601 ON t JOIN t t602 ON t JOIN t t603 ON t JOIN t t604 ON t
 JOIN t t605 ON t JOIN t t606 ON t JOIN t t607 ON t JOIN t t608 ON t
 JOIN t t609 ON t JOIN t t610 ON t JOIN t t611 ON t JOIN t t612 ON t
 JOIN t t613 ON t JOIN t t614 ON t JOIN t t615 ON t JOIN t t616 ON t
 JOIN t t617 ON t JOIN t t618 ON t JOIN t t619 ON t JOIN t t620 ON t
 JOIN t t621 ON t JOIN t t622 ON t JOIN t t623 ON t JOIN t t624 ON t
 JOIN t t625 ON t JOIN t t626 ON t JOIN t t627 ON t JOIN t t628 ON t
 JOIN t t629 ON t JOIN t t630 ON t JOIN t t631 ON t JOIN t t632 ON t
 JOIN t t633 ON t JOIN t t634 ON t JOIN t t635 ON t JOIN t t636 ON t
 JOIN t t637 ON t JOIN t t638 ON t JOIN t t639 ON t JOIN t t640 ON t
 JOIN t t641 ON t JOIN t t642 ON t JOIN t t643 ON t JOIN t t644 ON t
 JOIN t t645 ON t JOIN t t646 ON t JOIN t t647 ON t JOIN t t648 ON t
 JOIN t t649 ON t JOIN t t650 ON t JOIN t t651 ON t JOIN t t652 ON t
 JOIN t t653 ON t JOIN t t654 ON t JOIN t t655 ON t JOIN t t656 ON t
 JOIN t t657 ON t JOIN t t658 ON t JOIN t t659 ON t JOIN t t660 ON t
 JOIN t t661 ON t JOIN t t662 ON t JOIN t t663 ON t JOIN t t664 ON t
 JOIN t t665 ON t JOIN t t666 ON t JOIN t t667 ON t JOIN t t668 ON t
 JOIN t t669 ON t JOIN t t670 ON t JOIN t t671 ON t JOIN t t672 ON t
 JOIN t t673 ON t JOIN t t674 ON t JOIN t t675 ON t JOIN t t676 ON t
 JOIN t t677 ON t JOIN t t678 ON t JOIN t t679 ON t JOIN t t680 ON t
 JOIN t t681 ON t JOIN t t682 ON t JOIN t t683 ON t JOIN t t684 ON t
 JOIN t t685 ON t JOIN t t686 ON t JOIN t t687 ON t JOIN t t688 ON t
 JOIN t t689 ON t JOIN t t690 ON t JOIN t t691 ON t JOIN t t692 ON t
 JOIN t t693 ON t JOIN t t694 ON t JOIN t t695 ON t JOIN t t696 ON t
 JOIN t t697 ON t JOIN t t698 ON t JOIN t t699 ON t JOIN t t700 ON t
 JOIN t t701 ON t JOIN t t702 ON t JOIN t t703 ON t JOIN t t704 ON t
 JOIN t t705 ON t JOIN t t706 ON t JOIN t t707 ON t JOIN t t708 ON t
 JOIN t t709 ON t JOIN t t710 ON t JOIN t t711 ON t JOIN t t712 ON t
 JOIN t t713 ON t JOIN t t714 ON t JOIN t t715 ON t JOIN t t716 ON t
 JOIN t t717 ON t JOIN t t718 ON t JOIN t t719 ON t JOIN t t720 ON t
 JOIN t t721 ON t JOIN t t722 ON t JOIN t t723 ON t JOIN t t724 ON t
 JOIN t t725 ON t JOIN t t726 ON t JOIN t t727 ON t JOIN t t728 ON t
 JOIN t t729 ON t JOIN t t730 ON t JOIN t t731 ON t JOIN t t732 ON t
 JOIN t t733 ON t JOIN t t734 ON t JOIN t t735 ON t JOIN t t736 ON t
 JOIN t t737 ON t JOIN t t738 ON t JOIN t t739 ON t JOIN t t740 ON t
 JOIN t t741 ON t JOIN t t742 ON t JOIN t t743 ON t JOIN t t744 ON t
 JOIN t t745 ON t JOIN t t746 ON t JOIN t t747 ON t JOIN t t748 ON t
 JOIN t t749 ON t JOIN t t750 ON t JOIN t t751 ON t JOIN t t752 ON t
 JOIN t t753 ON t JOIN t t754 ON t JOIN t t755 ON t JOIN t t756 ON t
 JOIN t t757 ON t JOIN t t758 ON t JOIN t t759 ON t JOIN t t760 ON t
 JOIN t t761 ON t JOIN t t762 ON t JOIN t t763 ON t JOIN t t764 ON t
 JOIN t t765 ON t JOIN t t766 ON t JOIN t t767 ON t JOIN t t768 ON t
 JOIN t t769 ON t JOIN t t770 ON t JOIN t t771 ON t JOIN t t772 ON t
 JOIN t t773 ON t JOIN t t774 ON t JOIN t t775 ON t JOIN t t776 ON t
 JOIN t t777 ON t JOIN t t778 ON t JOIN t t779 ON t JOIN t t780 ON t
 JOIN t t781 ON t JOIN t t782 ON t JOIN t t783 ON t JOIN t t784 ON t
 JOIN t t785 ON t JOIN t t786 ON t JOIN t t787 ON t JOIN t t788 ON t
 JOIN t t789 ON t JOIN t t790 ON t JOIN t t791 ON t JOIN t t792 ON t
 JOIN t t793 ON t JOIN t t794 ON t JOIN t t795 ON t JOIN t t796 ON t
 JOIN t t797 ON t JOIN t t798 ON t JOIN t t799 ON t JOIN t t800 ON t
 JOIN t t801 ON t JOIN t t802 ON t JOIN t t803 ON t JOIN t t804 ON t
 JOIN t t805 ON t JOIN t t806 ON t JOIN t t807 ON t JOIN t t808 ON t
 JOIN t t809 ON t JOIN t t810 ON t JOIN t t811 ON t JOIN t t812 ON t
 JOIN t t813 ON t JOIN t t814 ON t JOIN t t815 ON t JOIN t t816 ON t
 JOIN t t817 ON t JOIN t t818 ON t JOIN t t819 ON t JOIN t t820 ON t
 JOIN t t821 ON t JOIN t t822 ON t JOIN t t823 ON t JOIN t t824 ON t
 JOIN t t825 ON t JOIN t t826 ON t JOIN t t827 ON t JOIN t t828 ON t
 JOIN t t829 ON t JOIN t t830 ON t JOIN t t831 ON t JOIN t t832 ON t
 JOIN t t833 ON t JOIN t t834 ON t JOIN t t835 ON t JOIN t t836 ON t
 JOIN t t837 ON t JOIN t t838 ON t JOIN t t839 ON t JOIN t t840 ON t
 JOIN t t841 ON t JOIN t t842 ON t JOIN t t843 ON t JOIN t t844 ON t
 JOIN t t845 ON t JOIN t t846 ON t JOIN t t847 ON t JOIN t t848 ON t
 JOIN t t849 ON t JOIN t t850 ON t JOIN t t851 ON t JOIN t t852 ON t
 JOIN t t853 ON t JOIN t t854 ON t JOIN t t855 ON t JOIN t t856 ON t
 JOIN t t857 ON t JOIN t t858 ON t JOIN t t859 ON t JOIN t t860 ON t
 JOIN t t861 ON t JOIN t t862 ON t JOIN t t863 ON t JOIN t t864 ON t
 JOIN t t865 ON t JOIN t t866 ON t JOIN t t867 ON t JOIN t t868 ON t
 JOIN t t869 ON t JOIN t t870 ON t JOIN t t871 ON t JOIN t t872 ON t
 JOIN t t873 ON t JOIN t t874 ON t JOIN t t875 ON t JOIN t t876 ON t
 JOIN t t877 ON t JOIN t t878 ON t JOIN t t879 ON t JOIN t t880 ON t
 JOIN t t881 ON t JOIN t t882 ON t JOIN t t883 ON t JOIN t t884 ON t
 JOIN t t885 ON t JOIN t t886 ON t JOIN t t887 ON t JOIN t t888 ON t
 JOIN t t889 ON t JOIN t t890 ON t JOIN t t891 ON t JOIN t t892 ON t
 JOIN t t893 ON t JOIN t t894 ON t JOIN t t895 ON t JOIN t t896 ON t
 JOIN t t897 ON t JOIN t t898 ON t JOIN t t899 ON t JOIN t t900 ON t
 JOIN t t901 ON t JOIN t t902 ON t JOIN t t903 ON t JOIN t t904 ON t
 JOIN t t905 ON t JOIN t t906 ON t JOIN t t907 ON t JOIN t t908 ON t
 JOIN t t909 ON t JOIN t t910 ON t JOIN t t911 ON t JOIN t t912 ON t
 JOIN t t913 ON t JOIN t t914 ON t JOIN t t915 ON t JOIN t t916 ON t
 JOIN t t917 ON t JOIN t t918 ON t JOIN t t919 ON t JOIN t t920 ON t
 JOIN t t921 ON t JOIN t t922 ON t JOIN t t923 ON t JOIN t t924 ON t
 JOIN t t925 ON t JOIN t t926 ON t JOIN t t927 ON t JOIN t t928 ON t
 JOIN t t929 ON t JOIN t t930 ON t JOIN t t931 ON t JOIN t t932 ON t
 JOIN t t933 ON t JOIN t t934 ON t JOIN t t935 ON t JOIN t t936 ON t
 JOIN t t937 ON t JOIN t t938 ON t JOIN t t939 ON t JOIN t t940 ON t
 JOIN t t941 ON t JOIN t t942 ON t JOIN t t943 ON t JOIN t t944 ON t
 JOIN t t945 ON t JOIN t t946 ON t JOIN t t947 ON t JOIN t t948 ON t
 JOIN t t949 ON t JOIN t t950 ON t JOIN t t951 ON t JOIN t t952 ON t
 JOIN t t953 ON t JOIN t t954 ON t JOIN t t955 ON t JOIN t t956 ON t
 JOIN t t957 ON t JOIN t t958 ON t JOIN t t959 ON t JOIN t t960 ON t
 JOIN t t961 ON t JOIN t t962 ON t JOIN t t963 ON t JOIN t t964 ON t
 JOIN t t965 ON t JOIN t t966 ON t JOIN t t967 ON t JOIN t t968 ON t
 JOIN t t969 ON t JOIN t t970 ON t JOIN t t971 ON t JOIN t t972 ON t
 JOIN t t973 ON t JOIN t t974 ON t JOIN t t975 ON t JOIN t t976 ON t
 JOIN t t977 ON t JOIN t t978 ON t JOIN t t979 ON t JOIN t t980 ON t
 JOIN t t981 ON t JOIN t t982 ON t JOIN t t983 ON t JOIN t t984 ON t
 JOIN t t985 ON t JOIN t t986 ON t JOIN t t987 ON t JOIN t t988 ON t
 JOIN t t989 ON t JOIN t t990 ON t JOIN t t991 ON t JOIN t t992 ON t
 JOIN t t993 ON t JOIN t t994 ON t JOIN t t995 ON t JOIN t t996 ON t
 JOIN t t997 ON t JOIN t t998 ON t JOIN t t999 ON t
 JOIN t t1000 ON t JOIN t t1001 ON t JOIN t t1002 ON t JOIN t t1003 ON t
 JOIN t t1004 ON t JOIN t t1005 ON t JOIN t t1006 ON t JOIN t t1007 ON t
 JOIN t t1008 ON t JOIN t t1009 ON t JOIN t t1010 ON t JOIN t t1011 ON t
 JOIN t t1012 ON t JOIN t t1013 ON t JOIN t t1014 ON t JOIN t t1015 ON t
 JOIN t t1016 ON t JOIN t t1017 ON t JOIN t t1018 ON t JOIN t t1019 ON t
 JOIN t t1020 ON t JOIN t t1021 ON t JOIN t t1022 ON t JOIN t t1023 ON t
 JOIN t t1024 ON t JOIN t t1025 ON t JOIN t t1026 ON t JOIN t t1027 ON t
 JOIN t t1028 ON t JOIN t t1029 ON t JOIN t t1030 ON t JOIN t t1031 ON t
 JOIN t t1032 ON t JOIN t t1033 ON t JOIN t t1034 ON t JOIN t t1035 ON t
 JOIN t t1036 ON t JOIN t t1037 ON t JOIN t t1038 ON t JOIN t t1039 ON t
 JOIN t t1040 ON t JOIN t t1041 ON t JOIN t t1042 ON t JOIN t t1043 ON t
 JOIN t t1044 ON t JOIN t t1045 ON t JOIN t t1046 ON t JOIN t t1047 ON t
 JOIN t t1048 ON t JOIN t t1049 ON t JOIN t t1050 ON t JOIN t t1051 ON t
 JOIN t t1052 ON t JOIN t t1053 ON t JOIN t t1054 ON t JOIN t t1055 ON t
 JOIN t t1056 ON t JOIN t t1057 ON t JOIN t t1058 ON t JOIN t t1059 ON t
 JOIN t t1060 ON t JOIN t t1061 ON t JOIN t t1062 ON t JOIN t t1063 ON t
 JOIN t t1064 ON t JOIN t t1065 ON t JOIN t t1066 ON t JOIN t t1067 ON t
 JOIN t t1068 ON t JOIN t t1069 ON t JOIN t t1070 ON t JOIN t t1071 ON t
 JOIN t t1072 ON t JOIN t t1073 ON t JOIN t t1074 ON t JOIN t t1075 ON t
 JOIN t t1076 ON t JOIN t t1077 ON t JOIN t t1078 ON t JOIN t t1079 ON t
 JOIN t t1080 ON t JOIN t t1081 ON t JOIN t t1082 ON t JOIN t t1083 ON t
 JOIN t t1084 ON t JOIN t t1085 ON t JOIN t t1086 ON t JOIN t t1087 ON t
 JOIN t t1088 ON t JOIN t t1089 ON t JOIN t t1090 ON t JOIN t t1091 ON t
 JOIN t t1092 ON t JOIN t t1093 ON t JOIN t t1094 ON t JOIN t t1095 ON t
 JOIN t t1096 ON t JOIN t t1097 ON t JOIN t t1098 ON t JOIN t t1099 ON t
 JOIN t t1100 ON t JOIN t t1101 ON t JOIN t t1102 ON t JOIN t t1103 ON t
 JOIN t t1104 ON t JOIN t t1105 ON t JOIN t t1106 ON t JOIN t t1107 ON t
 JOIN t t1108 ON t JOIN t t1109 ON t JOIN t t1110 ON t JOIN t t1111 ON t
 JOIN t t1112 ON t JOIN t t1113 ON t JOIN t t1114 ON t JOIN t t1115 ON t
 JOIN t t1116 ON t JOIN t t1117 ON t JOIN t t1118 ON t JOIN t t1119 ON t
 JOIN t t1120 ON t JOIN t t1121 ON t JOIN t t1122 ON t JOIN t t1123 ON t
 JOIN t t1124 ON t JOIN t t1125 ON t JOIN t t1126 ON t JOIN t t1127 ON t
 JOIN t t1128 ON t JOIN t t1129 ON t JOIN t t1130 ON t JOIN t t1131 ON t
 JOIN t t1132 ON t JOIN t t1133 ON t JOIN t t1134 ON t JOIN t t1135 ON t
 JOIN t t1136 ON t JOIN t t1137 ON t JOIN t t1138 ON t JOIN t t1139 ON t
 JOIN t t1140 ON t JOIN t t1141 ON t JOIN t t1142 ON t JOIN t t1143 ON t
 JOIN t t1144 ON t JOIN t t1145 ON t JOIN t t1146 ON t JOIN t t1147 ON t
 JOIN t t1148 ON t JOIN t t1149 ON t JOIN t t1150 ON t JOIN t t1151 ON t
 JOIN t t1152 ON t JOIN t t1153 ON t JOIN t t1154 ON t JOIN t t1155 ON t
 JOIN t t1156 ON t JOIN t t1157 ON t JOIN t t1158 ON t JOIN t t1159 ON t
 JOIN t t1160 ON t JOIN t t1161 ON t JOIN t t1162 ON t JOIN t t1163 ON t
 JOIN t t1164 ON t JOIN t t1165 ON t JOIN t t1166 ON t JOIN t t1167 ON t
 JOIN t t1168 ON t JOIN t t1169 ON t JOIN t t1170 ON t JOIN t t1171 ON t
 JOIN t t1172 ON t JOIN t t1173 ON t JOIN t t1174 ON t JOIN t t1175 ON t
 JOIN t t1176 ON t JOIN t t1177 ON t JOIN t t1178 ON t JOIN t t1179 ON t
 JOIN t t1180 ON t JOIN t t1181 ON t JOIN t t1182 ON t JOIN t t1183 ON t
 JOIN t t1184 ON t JOIN t t1185 ON t JOIN t t1186 ON t JOIN t t1187 ON t
 JOIN t t1188 ON t JOIN t t1189 ON t JOIN t t1190 ON t JOIN t t1191 ON t
 JOIN t t1192 ON t JOIN t t1193 ON t JOIN t t1194 ON t JOIN t t1195 ON t
 JOIN t t1196 ON t JOIN t t1197 ON t JOIN t t1198 ON t JOIN t t1199 ON t
 JOIN t t1200 ON t JOIN t t1201 ON t JOIN t t1202 ON t JOIN t t1203 ON t
 JOIN t t1204 ON t JOIN t t1205 ON t JOIN t t1206 ON t JOIN t t1207 ON t
 JOIN t t1208 ON t JOIN t t1209 ON t JOIN t t1210 ON t JOIN t t1211 ON t
 JOIN t t1212 ON t JOIN t t1213 ON t JOIN t t1214 ON t JOIN t t1215 ON t
 JOIN t t1216 ON t JOIN t t1217 ON t JOIN t t1218 ON t JOIN t t1219 ON t
 JOIN t t1220 ON t JOIN t t1221 ON t JOIN t t1222 ON t JOIN t t1223 ON t
 JOIN t t1224 ON t JOIN t t1225 ON t JOIN t t1226 ON t JOIN t t1227 ON t
 JOIN t t1228 ON t JOIN t t1229 ON t JOIN t t1230 ON t JOIN t t1231 ON t
 JOIN t t1232 ON t JOIN t t1233 ON t JOIN t t1234 ON t JOIN t t1235 ON t
 JOIN t t1236 ON t JOIN t t1237 ON t JOIN t t1238 ON t JOIN t t1239 ON t
 JOIN t t1240 ON t JOIN t t1241 ON t JOIN t t1242 ON t JOIN t t1243 ON t
 JOIN t t1244 ON t JOIN t t1245 ON t JOIN t t1246 ON t JOIN t t1247 ON t
 JOIN t t1248 ON t JOIN t t1249 ON t JOIN t t1250 ON t JOIN t t1251 ON t
 JOIN t t1252 ON t JOIN t t1253 ON t JOIN t t1254 ON t JOIN t t1255 ON t
 JOIN t t1256 ON t JOIN t t1257 ON t JOIN t t1258 ON t JOIN t t1259 ON t
 JOIN t t1260 ON t JOIN t t1261 ON t JOIN t t1262 ON t JOIN t t1263 ON t
 JOIN t t1264 ON t JOIN t t1265 ON t JOIN t t1266 ON t JOIN t t1267 ON t
 JOIN t t1268 ON t JOIN t t1269 ON t JOIN t t1270 ON t JOIN t t1271 ON t
 JOIN t t1272 ON t JOIN t t1273 ON t JOIN t t1274 ON t JOIN t t1275 ON t
 JOIN t t1276 ON t JOIN t t1277 ON t JOIN t t1278 ON t JOIN t t1279 ON t
 JOIN t t1280 ON t JOIN t t1281 ON t JOIN t t1282 ON t JOIN t t1283 ON t
 JOIN t t1284 ON t JOIN t t1285 ON t JOIN t t1286 ON t JOIN t t1287 ON t
 JOIN t t1288 ON t JOIN t t1289 ON t JOIN t t1290 ON t JOIN t t1291 ON t
 JOIN t t1292 ON t JOIN t t1293 ON t JOIN t t1294 ON t JOIN t t1295 ON t
 JOIN t t1296 ON t JOIN t t1297 ON t JOIN t t1298 ON t JOIN t t1299 ON t
 JOIN t t1300 ON t JOIN t t1301 ON t JOIN t t1302 ON t JOIN t t1303 ON t
 JOIN t t1304 ON t JOIN t t1305 ON t JOIN t t1306 ON t JOIN t t1307 ON t
 JOIN t t1308 ON t JOIN t t1309 ON t JOIN t t1310 ON t JOIN t t1311 ON t
 JOIN t t1312 ON t JOIN t t1313 ON t JOIN t t1314 ON t JOIN t t1315 ON t
 JOIN t t1316 ON t JOIN t t1317 ON t JOIN t t1318 ON t JOIN t t1319 ON t
 JOIN t t1320 ON t JOIN t t1321 ON t JOIN t t1322 ON t JOIN t t1323 ON t
 JOIN t t1324 ON t JOIN t t1325 ON t JOIN t t1326 ON t JOIN t t1327 ON t
 JOIN t t1328 ON t JOIN t t1329 ON t JOIN t t1330 ON t JOIN t t1331 ON t
 JOIN t t1332 ON t JOIN t t1333 ON t JOIN t t1334 ON t JOIN t t1335 ON t
 JOIN t t1336 ON t JOIN t t1337 ON t JOIN t t1338 ON t JOIN t t1339 ON t
 JOIN t t1340 ON t JOIN t t1341 ON t JOIN t t1342 ON t JOIN t t1343 ON t
 JOIN t t1344 ON t JOIN t t1345 ON t JOIN t t1346 ON t JOIN t t1347 ON t
 JOIN t t1348 ON t JOIN t t1349 ON t JOIN t t1350 ON t JOIN t t1351 ON t
 JOIN t t1352 ON t JOIN t t1353 ON t JOIN t t1354 ON t JOIN t t1355 ON t
 JOIN t t1356 ON t JOIN t t1357 ON t JOIN t t1358 ON t JOIN t t1359 ON t
 JOIN t t1360 ON t JOIN t t1361 ON t JOIN t t1362 ON t JOIN t t1363 ON t
 JOIN t t1364 ON t JOIN t t1365 ON t JOIN t t1366 ON t JOIN t t1367 ON t
 JOIN t t1368 ON t JOIN t t1369 ON t JOIN t t1370 ON t JOIN t t1371 ON t
 JOIN t t1372 ON t JOIN t t1373 ON t JOIN t t1374 ON t JOIN t t1375 ON t
 JOIN t t1376 ON t JOIN t t1377 ON t JOIN t t1378 ON t JOIN t t1379 ON t
 JOIN t t1380 ON t JOIN t t1381 ON t JOIN t t1382 ON t JOIN t t1383 ON t
 JOIN t t1384 ON t JOIN t t1385 ON t JOIN t t1386 ON t JOIN t t1387 ON t
 JOIN t t1388 ON t JOIN t t1389 ON t JOIN t t1390 ON t JOIN t t1391 ON t
 JOIN t t1392 ON t JOIN t t1393 ON t JOIN t t1394 ON t JOIN t t1395 ON t
 JOIN t t1396 ON t JOIN t t1397 ON t JOIN t t1398 ON t JOIN t t1399 ON t
 JOIN t t1400 ON t JOIN t t1401 ON t JOIN t t1402 ON t JOIN t t1403 ON t
 JOIN t t1404 ON t JOIN t t1405 ON t JOIN t t1406 ON t JOIN t t1407 ON t
 JOIN t t1408 ON t JOIN t t1409 ON t JOIN t t1410 ON t JOIN t t1411 ON t
 JOIN t t1412 ON t JOIN t t1413 ON t JOIN t t1414 ON t JOIN t t1415 ON t
 JOIN t t1416 ON t JOIN t t1417 ON t JOIN t t1418 ON t JOIN t t1419 ON t
 JOIN t t1420 ON t JOIN t t1421 ON t JOIN t t1422 ON t JOIN t t1423 ON t
 JOIN t t1424 ON t JOIN t t1425 ON t JOIN t t1426 ON t JOIN t t1427 ON t
 JOIN t t1428 ON t JOIN t t1429 ON t JOIN t t1430 ON t JOIN t t1431 ON t
 JOIN t t1432 ON t JOIN t t1433 ON t JOIN t t1434 ON t JOIN t t1435 ON t
 JOIN t t1436 ON t JOIN t t1437 ON t JOIN t t1438 ON t JOIN t t1439 ON t
 JOIN t t1440 ON t JOIN t t1441 ON t JOIN t t1442 ON t JOIN t t1443 ON t
 JOIN t t1444 ON t JOIN t t1445 ON t JOIN t t1446 ON t JOIN t t1447 ON t
 JOIN t t1448 ON t JOIN t t1449 ON t JOIN t t1450 ON t JOIN t t1451 ON t
 JOIN t t1452 ON t JOIN t t1453 ON t JOIN t t1454 ON t JOIN t t1455 ON t
 JOIN t t1456 ON t JOIN t t1457 ON t JOIN t t1458 ON t JOIN t t1459 ON t
 JOIN t t1460 ON t JOIN t t1461 ON t JOIN t t1462 ON t JOIN t t1463 ON t
 JOIN t t1464 ON t JOIN t t1465 ON t JOIN t t1466 ON t JOIN t t1467 ON t
 JOIN t t1468 ON t JOIN t t1469 ON t JOIN t t1470 ON t JOIN t t1471 ON t
 JOIN t t1472 ON t JOIN t t1473 ON t JOIN t t1474 ON t JOIN t t1475 ON t
 JOIN t t1476 ON t JOIN t t1477 ON t JOIN t t1478 ON t JOIN t t1479 ON t
 JOIN t t1480 ON t JOIN t t1481 ON t JOIN t t1482 ON t JOIN t t1483 ON t
 JOIN t t1484 ON t JOIN t t1485 ON t JOIN t t1486 ON t JOIN t t1487 ON t
 JOIN t t1488 ON t JOIN t t1489 ON t JOIN t t1490 ON t JOIN t t1491 ON t
 JOIN t t1492 ON t JOIN t t1493 ON t JOIN t t1494 ON t JOIN t t1495 ON t
 JOIN t t1496 ON t JOIN t t1497 ON t JOIN t t1498 ON t JOIN t t1499 ON t
 JOIN t t1500 ON t JOIN t t1501 ON t JOIN t t1502 ON t JOIN t t1503 ON t
 JOIN t t1504 ON t JOIN t t1505 ON t JOIN t t1506 ON t JOIN t t1507 ON t
 JOIN t t1508 ON t JOIN t t1509 ON t JOIN t t1510 ON t JOIN t t1511 ON t
 JOIN t t1512 ON t JOIN t t1513 ON t JOIN t t1514 ON t JOIN t t1515 ON t
 JOIN t t1516 ON t JOIN t t1517 ON t JOIN t t1518 ON t JOIN t t1519 ON t
 JOIN t t1520 ON t JOIN t t1521 ON t JOIN t t1522 ON t JOIN t t1523 ON t
 JOIN t t1524 ON t JOIN t t1525 ON t JOIN t t1526 ON t JOIN t t1527 ON t
 JOIN t t1528 ON t JOIN t t1529 ON t JOIN t t1530 ON t JOIN t t1531 ON t
 JOIN t t1532 ON t JOIN t t1533 ON t JOIN t t1534 ON t JOIN t t1535 ON t
 JOIN t t1536 ON t JOIN t t1537 ON t JOIN t t1538 ON t JOIN t t1539 ON t
 JOIN t t1540 ON t JOIN t t1541 ON t JOIN t t1542 ON t JOIN t t1543 ON t
 JOIN t t1544 ON t JOIN t t1545 ON t JOIN t t1546 ON t JOIN t t1547 ON t
 JOIN t t1548 ON t JOIN t t1549 ON t JOIN t t1550 ON t JOIN t t1551 ON t
 JOIN t t1552 ON t JOIN t t1553 ON t JOIN t t1554 ON t JOIN t t1555 ON t
 JOIN t t1556 ON t JOIN t t1557 ON t JOIN t t1558 ON t JOIN t t1559 ON t
 JOIN t t1560 ON t JOIN t t1561 ON t JOIN t t1562 ON t JOIN t t1563 ON t
 JOIN t t1564 ON t JOIN t t1565 ON t JOIN t t1566 ON t JOIN t t1567 ON t
 JOIN t t1568 ON t JOIN t t1569 ON t JOIN t t1570 ON t JOIN t t1571 ON t
 JOIN t t1572 ON t JOIN t t1573 ON t JOIN t t1574 ON t JOIN t t1575 ON t
 JOIN t t1576 ON t JOIN t t1577 ON t JOIN t t1578 ON t JOIN t t1579 ON t
 JOIN t t1580 ON t JOIN t t1581 ON t JOIN t t1582 ON t JOIN t t1583 ON t
 JOIN t t1584 ON t JOIN t t1585 ON t JOIN t t1586 ON t JOIN t t1587 ON t
 JOIN t t1588 ON t JOIN t t1589 ON t JOIN t t1590 ON t JOIN t t1591 ON t
 JOIN t t1592 ON t JOIN t t1593 ON t JOIN t t1594 ON t JOIN t t1595 ON t
 JOIN t t1596 ON t JOIN t t1597 ON t JOIN t t1598 ON t JOIN t t1599 ON t
 JOIN t t1600 ON t JOIN t t1601 ON t JOIN t t1602 ON t JOIN t t1603 ON t
 JOIN t t1604 ON t JOIN t t1605 ON t JOIN t t1606 ON t JOIN t t1607 ON t
 JOIN t t1608 ON t JOIN t t1609 ON t JOIN t t1610 ON t JOIN t t1611 ON t
 JOIN t t1612 ON t JOIN t t1613 ON t JOIN t t1614 ON t JOIN t t1615 ON t
 JOIN t t1616 ON t JOIN t t1617 ON t JOIN t t1618 ON t JOIN t t1619 ON t
 JOIN t t1620 ON t JOIN t t1621 ON t JOIN t t1622 ON t JOIN t t1623 ON t
 JOIN t t1624 ON t JOIN t t1625 ON t JOIN t t1626 ON t JOIN t t1627 ON t
 JOIN t t1628 ON t JOIN t t1629 ON t JOIN t t1630 ON t JOIN t t1631 ON t
 JOIN t t1632 ON t JOIN t t1633 ON t JOIN t t1634 ON t JOIN t t1635 ON t
 JOIN t t1636 ON t JOIN t t1637 ON t JOIN t t1638 ON t JOIN t t1639 ON t
 JOIN t t1640 ON t JOIN t t1641 ON t JOIN t t1642 ON t JOIN t t1643 ON t
 JOIN t t1644 ON t JOIN t t1645 ON t JOIN t t1646 ON t JOIN t t1647 ON t
 JOIN t t1648 ON t JOIN t t1649 ON t JOIN t t1650 ON t JOIN t t1651 ON t
 JOIN t t1652 ON t JOIN t t1653 ON t JOIN t t1654 ON t JOIN t t1655 ON t
 JOIN t t1656 ON t JOIN t t1657 ON t JOIN t t1658 ON t JOIN t t1659 ON t
 JOIN t t1660 ON t JOIN t t1661 ON t JOIN t t1662 ON t JOIN t t1663 ON t
 JOIN t t1664 ON t JOIN t t1665 ON t JOIN t t1666 ON t JOIN t t1667 ON t
 JOIN t t1668 ON t JOIN t t1669 ON t JOIN t t1670 ON t JOIN t t1671 ON t
 JOIN t t1672 ON t JOIN t t1673 ON t JOIN t t1674 ON t JOIN t t1675 ON t
 JOIN t t1676 ON t JOIN t t1677 ON t JOIN t t1678 ON t JOIN t t1679 ON t
 JOIN t t1680 ON t JOIN t t1681 ON t JOIN t t1682 ON t JOIN t t1683 ON t
 JOIN t t1684 ON t JOIN t t1685 ON t JOIN t t1686 ON t JOIN t t1687 ON t
 JOIN t t1688 ON t JOIN t t1689 ON t JOIN t t1690 ON t JOIN t t1691 ON t
 JOIN t t1692 ON t JOIN t t1693 ON t JOIN t t1694 ON t JOIN t t1695 ON t
 JOIN t t1696 ON t JOIN t t1697 ON t JOIN t t1698 ON t JOIN t t1699 ON t
 JOIN t t1700 ON t JOIN t t1701 ON t JOIN t t1702 ON t JOIN t t1703 ON t
 JOIN t t1704 ON t JOIN t t1705 ON t JOIN t t1706 ON t JOIN t t1707 ON t
 JOIN t t1708 ON t JOIN t t1709 ON t JOIN t t1710 ON t JOIN t t1711 ON t
 JOIN t t1712 ON t JOIN t t1713 ON t JOIN t t1714 ON t JOIN t t1715 ON t
 JOIN t t1716 ON t JOIN t t1717 ON t JOIN t t1718 ON t JOIN t t1719 ON t
 JOIN t t1720 ON t JOIN t t1721 ON t JOIN t t1722 ON t JOIN t t1723 ON t
 JOIN t t1724 ON t JOIN t t1725 ON t JOIN t t1726 ON t JOIN t t1727 ON t
 JOIN t t1728 ON t JOIN t t1729 ON t JOIN t t1730 ON t JOIN t t1731 ON t
 JOIN t t1732 ON t JOIN t t1733 ON t JOIN t t1734 ON t JOIN t t1735 ON t
 JOIN t t1736 ON t JOIN t t1737 ON t JOIN t t1738 ON t JOIN t t1739 ON t
 JOIN t t1740 ON t JOIN t t1741 ON t JOIN t t1742 ON t JOIN t t1743 ON t
 JOIN t t1744 ON t JOIN t t1745 ON t JOIN t t1746 ON t JOIN t t1747 ON t
 JOIN t t1748 ON t JOIN t t1749 ON t JOIN t t1750 ON t JOIN t t1751 ON t
 JOIN t t1752 ON t JOIN t t1753 ON t JOIN t t1754 ON t JOIN t t1755 ON t
 JOIN t t1756 ON t JOIN t t1757 ON t JOIN t t1758 ON t JOIN t t1759 ON t
 JOIN t t1760 ON t JOIN t t1761 ON t JOIN t t1762 ON t JOIN t t1763 ON t
 JOIN t t1764 ON t JOIN t t1765 ON t JOIN t t1766 ON t JOIN t t1767 ON t
 JOIN t t1768 ON t JOIN t t1769 ON t JOIN t t1770 ON t JOIN t t1771 ON t
 JOIN t t1772 ON t JOIN t t1773 ON t JOIN t t1774 ON t JOIN t t1775 ON t
 JOIN t t1776 ON t JOIN t t1777 ON t JOIN t t1778 ON t JOIN t t1779 ON t
 JOIN t t1780 ON t JOIN t t1781 ON t JOIN t t1782 ON t JOIN t t1783 ON t
 JOIN t t1784 ON t JOIN t t1785 ON t JOIN t t1786 ON t JOIN t t1787 ON t
 JOIN t t1788 ON t JOIN t t1789 ON t JOIN t t1790 ON t JOIN t t1791 ON t
 JOIN t t1792 ON t JOIN t t1793 ON t JOIN t t1794 ON t JOIN t t1795 ON t
 JOIN t t1796 ON t JOIN t t1797 ON t JOIN t t1798 ON t JOIN t t1799 ON t
 JOIN t t1800 ON t JOIN t t1801 ON t JOIN t t1802 ON t JOIN t t1803 ON t
 JOIN t t1804 ON t JOIN t t1805 ON t JOIN t t1806 ON t JOIN t t1807 ON t
 JOIN t t1808 ON t JOIN t t1809 ON t JOIN t t1810 ON t JOIN t t1811 ON t
 JOIN t t1812 ON t JOIN t t1813 ON t JOIN t t1814 ON t JOIN t t1815 ON t
 JOIN t t1816 ON t JOIN t t1817 ON t JOIN t t1818 ON t JOIN t t1819 ON t
 JOIN t t1820 ON t JOIN t t1821 ON t JOIN t t1822 ON t JOIN t t1823 ON t
 JOIN t t1824 ON t JOIN t t1825 ON t JOIN t t1826 ON t JOIN t t1827 ON t
 JOIN t t1828 ON t JOIN t t1829 ON t JOIN t t1830 ON t JOIN t t1831 ON t
 JOIN t t1832 ON t JOIN t t1833 ON t JOIN t t1834 ON t JOIN t t1835 ON t
 JOIN t t1836 ON t JOIN t t1837 ON t JOIN t t1838 ON t JOIN t t1839 ON t
 JOIN t t1840 ON t JOIN t t1841 ON t JOIN t t1842 ON t JOIN t t1843 ON t
 JOIN t t1844 ON t JOIN t t1845 ON t JOIN t t1846 ON t JOIN t t1847 ON t
 JOIN t t1848 ON t JOIN t t1849 ON t JOIN t t1850 ON t JOIN t t1851 ON t
 JOIN t t1852 ON t JOIN t t1853 ON t JOIN t t1854 ON t JOIN t t1855 ON t
 JOIN t t1856 ON t JOIN t t1857 ON t JOIN t t1858 ON t JOIN t t1859 ON t
 JOIN t t1860 ON t JOIN t t1861 ON t JOIN t t1862 ON t JOIN t t1863 ON t
 JOIN t t1864 ON t JOIN t t1865 ON t JOIN t t1866 ON t JOIN t t1867 ON t
 JOIN t t1868 ON t JOIN t t1869 ON t JOIN t t1870 ON t JOIN t t1871 ON t
 JOIN t t1872 ON t JOIN t t1873 ON t JOIN t t1874 ON t JOIN t t1875 ON t
 JOIN t t1876 ON t JOIN t t1877 ON t JOIN t t1878 ON t JOIN t t1879 ON t
 JOIN t t1880 ON t JOIN t t1881 ON t JOIN t t1882 ON t JOIN t t1883 ON t
 JOIN t t1884 ON t JOIN t t1885 ON t JOIN t t1886 ON t JOIN t t1887 ON t
 JOIN t t1888 ON t JOIN t t1889 ON t JOIN t t1890 ON t JOIN t t1891 ON t
 JOIN t t1892 ON t JOIN t t1893 ON t JOIN t t1894 ON t JOIN t t1895 ON t
 JOIN t t1896 ON t JOIN t t1897 ON t JOIN t t1898 ON t JOIN t t1899 ON t
 JOIN t t1900 ON t JOIN t t1901 ON t JOIN t t1902 ON t JOIN t t1903 ON t
 JOIN t t1904 ON t JOIN t t1905 ON t JOIN t t1906 ON t JOIN t t1907 ON t
 JOIN t t1908 ON t JOIN t t1909 ON t JOIN t t1910 ON t JOIN t t1911 ON t
 JOIN t t1912 ON t JOIN t t1913 ON t JOIN t t1914 ON t JOIN t t1915 ON t
 JOIN t t1916 ON t JOIN t t1917 ON t JOIN t t1918 ON t JOIN t t1919 ON t
 JOIN t t1920 ON t JOIN t t1921 ON t JOIN t t1922 ON t JOIN t t1923 ON t
 JOIN t t1924 ON t JOIN t t1925 ON t JOIN t t1926 ON t JOIN t t1927 ON t
 JOIN t t1928 ON t JOIN t t1929 ON t JOIN t t1930 ON t JOIN t t1931 ON t
 JOIN t t1932 ON t JOIN t t1933 ON t JOIN t t1934 ON t JOIN t t1935 ON t
 JOIN t t1936 ON t JOIN t t1937 ON t JOIN t t1938 ON t JOIN t t1939 ON t
 JOIN t t1940 ON t JOIN t t1941 ON t JOIN t t1942 ON t JOIN t t1943 ON t
 JOIN t t1944 ON t JOIN t t1945 ON t JOIN t t1946 ON t JOIN t t1947 ON t
 JOIN t t1948 ON t JOIN t t1949 ON t JOIN t t1950 ON t JOIN t t1951 ON t
 JOIN t t1952 ON t JOIN t t1953 ON t JOIN t t1954 ON t JOIN t t1955 ON t
 JOIN t t1956 ON t JOIN t t1957 ON t JOIN t t1958 ON t JOIN t t1959 ON t
 JOIN t t1960 ON t JOIN t t1961 ON t JOIN t t1962 ON t JOIN t t1963 ON t
 JOIN t t1964 ON t JOIN t t1965 ON t JOIN t t1966 ON t JOIN t t1967 ON t
 JOIN t t1968 ON t JOIN t t1969 ON t JOIN t t1970 ON t JOIN t t1971 ON t
 JOIN t t1972 ON t JOIN t t1973 ON t JOIN t t1974 ON t JOIN t t1975 ON t
 JOIN t t1976 ON t JOIN t t1977 ON t JOIN t t1978 ON t JOIN t t1979 ON t
 JOIN t t1980 ON t JOIN t t1981 ON t JOIN t t1982 ON t JOIN t t1983 ON t
 JOIN t t1984 ON t JOIN t t1985 ON t JOIN t t1986 ON t JOIN t t1987 ON t
 JOIN t t1988 ON t JOIN t t1989 ON t JOIN t t1990 ON t JOIN t t1991 ON t
 JOIN t t1992 ON t JOIN t t1993 ON t JOIN t t1994 ON t JOIN t t1995 ON t
 JOIN t t1996 ON t JOIN t t1997 ON t JOIN t t1998 ON t JOIN t t1999 ON t
 JOIN t t2000 ON t JOIN t t2001 ON t JOIN t t2002 ON t JOIN t t2003 ON t
 JOIN t t2004 ON t JOIN t t2005 ON t JOIN t t2006 ON t JOIN t t2007 ON t
 JOIN t t2008 ON t JOIN t t2009 ON t JOIN t t2010 ON t JOIN t t2011 ON t
 JOIN t t2012 ON t JOIN t t2013 ON t JOIN t t2014 ON t JOIN t t2015 ON t
 JOIN t t2016 ON t JOIN t t2017 ON t JOIN t t2018 ON t JOIN t t2019 ON t
 JOIN t t2020 ON t JOIN t t2021 ON t JOIN t t2022 ON t JOIN t t2023 ON t
 JOIN t t2024 ON t JOIN t t2025 ON t JOIN t t2026 ON t JOIN t t2027 ON t
 JOIN t t2028 ON t JOIN t t2029 ON t JOIN t t2030 ON t JOIN t t2031 ON t
 JOIN t t2032 ON t JOIN t t2033 ON t JOIN t t2034 ON t JOIN t t2035 ON t
 JOIN t t2036 ON t JOIN t t2037 ON t JOIN t t2038 ON t JOIN t t2039 ON t
 JOIN t t2040 ON t JOIN t t2041 ON t JOIN t t2042 ON t JOIN t t2043 ON t
 JOIN t t2044 ON t JOIN t t2045 ON t JOIN t t2046 ON t JOIN t t2047 ON t
 JOIN t t2048 ON t JOIN t t2049 ON t JOIN t t2050 ON t JOIN t t2051 ON t
 JOIN t t2052 ON t JOIN t t2053 ON t JOIN t t2054 ON t JOIN t t2055 ON t
 JOIN t t2056 ON t JOIN t t2057 ON t JOIN t t2058 ON t JOIN t t2059 ON t
 JOIN t t2060 ON t JOIN t t2061 ON t JOIN t t2062 ON t JOIN t t2063 ON t
 JOIN t t2064 ON t JOIN t t2065 ON t JOIN t t2066 ON t JOIN t t2067 ON t
 JOIN t t2068 ON t JOIN t t2069 ON t JOIN t t2070 ON t JOIN t t2071 ON t
 JOIN t t2072 ON t JOIN t t2073 ON t JOIN t t2074 ON t JOIN t t2075 ON t
 JOIN t t2076 ON t JOIN t t2077 ON t JOIN t t2078 ON t JOIN t t2079 ON t
 JOIN t t2080 ON t JOIN t t2081 ON t JOIN t t2082 ON t JOIN t t2083 ON t
 JOIN t t2084 ON t JOIN t t2085 ON t JOIN t t2086 ON t JOIN t t2087 ON t
 JOIN t t2088 ON t JOIN t t2089 ON t JOIN t t2090 ON t JOIN t t2091 ON t
 JOIN t t2092 ON t JOIN t t2093 ON t JOIN t t2094 ON t JOIN t t2095 ON t
 JOIN t t2096 ON t JOIN t t2097 ON t JOIN t t2098 ON t JOIN t t2099 ON t
 JOIN t t2100 ON t JOIN t t2101 ON t JOIN t t2102 ON t JOIN t t2103 ON t
 JOIN t t2104 ON t JOIN t t2105 ON t JOIN t t2106 ON t JOIN t t2107 ON t
 JOIN t t2108 ON t JOIN t t2109 ON t JOIN t t2110 ON t JOIN t t2111 ON t
 JOIN t t2112 ON t JOIN t t2113 ON t JOIN t t2114 ON t JOIN t t2115 ON t
 JOIN t t2116 ON t JOIN t t2117 ON t JOIN t t2118 ON t JOIN t t2119 ON t
 JOIN t t2120 ON t JOIN t t2121 ON t JOIN t t2122 ON t JOIN t t2123 ON t
 JOIN t t2124 ON t JOIN t t2125 ON t JOIN t t2126 ON t JOIN t t2127 ON t
 JOIN t t2128 ON t JOIN t t2129 ON t JOIN t t2130 ON t JOIN t t2131 ON t
 JOIN t t2132 ON t JOIN t t2133 ON t JOIN t t2134 ON t JOIN t t2135 ON t
 JOIN t t2136 ON t JOIN t t2137 ON t JOIN t t2138 ON t JOIN t t2139 ON t
 JOIN t t2140 ON t JOIN t t2141 ON t JOIN t t2142 ON t JOIN t t2143 ON t
 JOIN t t2144 ON t JOIN t t2145 ON t JOIN t t2146 ON t JOIN t t2147 ON t
 JOIN t t2148 ON t JOIN t t2149 ON t JOIN t t2150 ON t JOIN t t2151 ON t
 JOIN t t2152 ON t JOIN t t2153 ON t JOIN t t2154 ON t JOIN t t2155 ON t
 JOIN t t2156 ON t JOIN t t2157 ON t JOIN t t2158 ON t JOIN t t2159 ON t
 JOIN t t2160 ON t JOIN t t2161 ON t JOIN t t2162 ON t JOIN t t2163 ON t
 JOIN t t2164 ON t JOIN t t2165 ON t JOIN t t2166 ON t JOIN t t2167 ON t
 JOIN t t2168 ON t JOIN t t2169 ON t JOIN t t2170 ON t JOIN t t2171 ON t
 JOIN t t2172 ON t JOIN t t2173 ON t JOIN t t2174 ON t JOIN t t2175 ON t
 JOIN t t2176 ON t JOIN t t2177 ON t JOIN t t2178 ON t JOIN t t2179 ON t
 JOIN t t2180 ON t JOIN t t2181 ON t JOIN t t2182 ON t JOIN t t2183 ON t
 JOIN t t2184 ON t JOIN t t2185 ON t JOIN t t2186 ON t JOIN t t2187 ON t
 JOIN t t2188 ON t JOIN t t2189 ON t JOIN t t2190 ON t JOIN t t2191 ON t
 JOIN t t2192 ON t JOIN t t2193 ON t JOIN t t2194 ON t JOIN t t2195 ON t
 JOIN t t2196 ON t JOIN t t2197 ON t JOIN t t2198 ON t JOIN t t2199 ON t
 JOIN t t2200 ON t JOIN t t2201 ON t JOIN t t2202 ON t JOIN t t2203 ON t
 JOIN t t2204 ON t JOIN t t2205 ON t JOIN t t2206 ON t JOIN t t2207 ON t
 JOIN t t2208 ON t JOIN t t2209 ON t JOIN t t2210 ON t JOIN t t2211 ON t
 JOIN t t2212 ON t JOIN t t2213 ON t JOIN t t2214 ON t JOIN t t2215 ON t
 JOIN t t2216 ON t JOIN t t2217 ON t JOIN t t2218 ON t JOIN t t2219 ON t
 JOIN t t2220 ON t JOIN t t2221 ON t JOIN t t2222 ON t JOIN t t2223 ON t
 JOIN t t2224 ON t JOIN t t2225 ON t JOIN t t2226 ON t JOIN t t2227 ON t
 JOIN t t2228 ON t JOIN t t2229 ON t JOIN t t2230 ON t JOIN t t2231 ON t
 JOIN t t2232 ON t JOIN t t2233 ON t JOIN t t2234 ON t JOIN t t2235 ON t
 JOIN t t2236 ON t JOIN t t2237 ON t JOIN t t2238 ON t JOIN t t2239 ON t
 JOIN t t2240 ON t JOIN t t2241 ON t JOIN t t2242 ON t JOIN t t2243 ON t
 JOIN t t2244 ON t JOIN t t2245 ON t JOIN t t2246 ON t JOIN t t2247 ON t
 JOIN t t2248 ON t JOIN t t2249 ON t JOIN t t2250 ON t JOIN t t2251 ON t
 JOIN t t2252 ON t JOIN t t2253 ON t JOIN t t2254 ON t JOIN t t2255 ON t
 JOIN t t2256 ON t JOIN t t2257 ON t JOIN t t2258 ON t JOIN t t2259 ON t
 JOIN t t2260 ON t JOIN t t2261 ON t JOIN t t2262 ON t JOIN t t2263 ON t
 JOIN t t2264 ON t JOIN t t2265 ON t JOIN t t2266 ON t JOIN t t2267 ON t
 JOIN t t2268 ON t JOIN t t2269 ON t JOIN t t2270 ON t JOIN t t2271 ON t
 JOIN t t2272 ON t JOIN t t2273 ON t JOIN t t2274 ON t JOIN t t2275 ON t
 JOIN t t2276 ON t JOIN t t2277 ON t JOIN t t2278 ON t JOIN t t2279 ON t
 JOIN t t2280 ON t JOIN t t2281 ON t JOIN t t2282 ON t JOIN t t2283 ON t
 JOIN t t2284 ON t JOIN t t2285 ON t JOIN t t2286 ON t JOIN t t2287 ON t
 JOIN t t2288 ON t JOIN t t2289 ON t JOIN t t2290 ON t JOIN t t2291 ON t
 JOIN t t2292 ON t JOIN t t2293 ON t JOIN t t2294 ON t JOIN t t2295 ON t
 JOIN t t2296 ON t JOIN t t2297 ON t JOIN t t2298 ON t JOIN t t2299 ON t
 JOIN t t2300 ON t JOIN t t2301 ON t JOIN t t2302 ON t JOIN t t2303 ON t
 JOIN t t2304 ON t JOIN t t2305 ON t JOIN t t2306 ON t JOIN t t2307 ON t
 JOIN t t2308 ON t JOIN t t2309 ON t JOIN t t2310 ON t JOIN t t2311 ON t
 JOIN t t2312 ON t JOIN t t2313 ON t JOIN t t2314 ON t JOIN t t2315 ON t
 JOIN t t2316 ON t JOIN t t2317 ON t JOIN t t2318 ON t JOIN t t2319 ON t
 JOIN t t2320 ON t JOIN t t2321 ON t JOIN t t2322 ON t JOIN t t2323 ON t
 JOIN t t2324 ON t JOIN t t2325 ON t JOIN t t2326 ON t JOIN t t2327 ON t
 JOIN t t2328 ON t JOIN t t2329 ON t JOIN t t2330 ON t JOIN t t2331 ON t
 JOIN t t2332 ON t JOIN t t2333 ON t JOIN t t2334 ON t JOIN t t2335 ON t
 JOIN t t2336 ON t JOIN t t2337 ON t JOIN t t2338 ON t JOIN t t2339 ON t
 JOIN t t2340 ON t JOIN t t2341 ON t JOIN t t2342 ON t JOIN t t2343 ON t
 JOIN t t2344 ON t JOIN t t2345 ON t JOIN t t2346 ON t JOIN t t2347 ON t
 JOIN t t2348 ON t JOIN t t2349 ON t JOIN t t2350 ON t JOIN t t2351 ON t
 JOIN t t2352 ON t JOIN t t2353 ON t JOIN t t2354 ON t JOIN t t2355 ON t
 JOIN t t2356 ON t JOIN t t2357 ON t JOIN t t2358 ON t JOIN t t2359 ON t
 JOIN t t2360 ON t JOIN t t2361 ON t JOIN t t2362 ON t JOIN t t2363 ON t
 JOIN t t2364 ON t JOIN t t2365 ON t JOIN t t2366 ON t JOIN t t2367 ON t
 JOIN t t2368 ON t JOIN t t2369 ON t JOIN t t2370 ON t JOIN t t2371 ON t
 JOIN t t2372 ON t JOIN t t2373 ON t JOIN t t2374 ON t JOIN t t2375 ON t
 JOIN t t2376 ON t JOIN t t2377 ON t JOIN t t2378 ON t JOIN t t2379 ON t
 JOIN t t2380 ON t JOIN t t2381 ON t JOIN t t2382 ON t JOIN t t2383 ON t
 JOIN t t2384 ON t JOIN t t2385 ON t JOIN t t2386 ON t JOIN t t2387 ON t
 JOIN t t2388 ON t JOIN t t2389 ON t JOIN t t2390 ON t JOIN t t2391 ON t
 JOIN t t2392 ON t JOIN t t2393 ON t JOIN t t2394 ON t JOIN t t2395 ON t
 JOIN t t2396 ON t JOIN t t2397 ON t JOIN t t2398 ON t JOIN t t2399 ON t
 JOIN t t2400 ON t JOIN t t2401 ON t JOIN t t2402 ON t JOIN t t2403 ON t
 JOIN t t2404 ON t JOIN t t2405 ON t JOIN t t2406 ON t JOIN t t2407 ON t
 JOIN t t2408 ON t JOIN t t2409 ON t JOIN t t2410 ON t JOIN t t2411 ON t
 JOIN t t2412 ON t JOIN t t2413 ON t JOIN t t2414 ON t JOIN t t2415 ON t
 JOIN t t2416 ON t JOIN t t2417 ON t JOIN t t2418 ON t JOIN t t2419 ON t
 JOIN t t2420 ON t JOIN t t2421 ON t JOIN t t2422 ON t JOIN t t2423 ON t
 JOIN t t2424 ON t JOIN t t2425 ON t JOIN t t2426 ON t JOIN t t2427 ON t
 JOIN t t2428 ON t JOIN t t2429 ON t JOIN t t2430 ON t JOIN t t2431 ON t
 JOIN t t2432 ON t JOIN t t2433 ON t JOIN t t2434 ON t JOIN t t2435 ON t
 JOIN t t2436 ON t JOIN t t2437 ON t JOIN t t2438 ON t JOIN t t2439 ON t
 JOIN t t2440 ON t JOIN t t2441 ON t JOIN t t2442 ON t JOIN t t2443 ON t
 JOIN t t2444 ON t JOIN t t2445 ON t JOIN t t2446 ON t JOIN t t2447 ON t
 JOIN t t2448 ON t JOIN t t2449 ON t JOIN t t2450 ON t JOIN t t2451 ON t
 JOIN t t2452 ON t JOIN t t2453 ON t JOIN t t2454 ON t JOIN t t2455 ON t
 JOIN t t2456 ON t JOIN t t2457 ON t JOIN t t2458 ON t JOIN t t2459 ON t
 JOIN t t2460 ON t JOIN t t2461 ON t JOIN t t2462 ON t JOIN t t2463 ON t
 JOIN t t2464 ON t JOIN t t2465 ON t JOIN t t2466 ON t JOIN t t2467 ON t
 JOIN t t2468 ON t JOIN t t2469 ON t JOIN t t2470 ON t JOIN t t2471 ON t
 JOIN t t2472 ON t JOIN t t2473 ON t JOIN t t2474 ON t JOIN t t2475 ON t
 JOIN t t2476 ON t JOIN t t2477 ON t JOIN t t2478 ON t JOIN t t2479 ON t
 JOIN t t2480 ON t JOIN t t2481 ON t JOIN t t2482 ON t JOIN t t2483 ON t
 JOIN t t2484 ON t JOIN t t2485 ON t JOIN t t2486 ON t JOIN t t2487 ON t
 JOIN t t2488 ON t JOIN t t2489 ON t JOIN t t2490 ON t JOIN t t2491 ON t
 JOIN t t2492 ON t JOIN t t2493 ON t JOIN t t2494 ON t JOIN t t2495 ON t
 JOIN t t2496 ON t JOIN t t2497 ON t JOIN t t2498 ON t JOIN t t2499 ON t
 JOIN t t2500 ON t JOIN t t2501 ON t JOIN t t2502 ON t JOIN t t2503 ON t
 JOIN t t2504 ON t JOIN t t2505 ON t JOIN t t2506 ON t JOIN t t2507 ON t
 JOIN t t2508 ON t JOIN t t2509 ON t JOIN t t2510 ON t JOIN t t2511 ON t
 JOIN t t2512 ON t JOIN t t2513 ON t JOIN t t2514 ON t JOIN t t2515 ON t
 JOIN t t2516 ON t JOIN t t2517 ON t JOIN t t2518 ON t JOIN t t2519 ON t
 JOIN t t2520 ON t JOIN t t2521 ON t JOIN t t2522 ON t JOIN t t2523 ON t
 JOIN t t2524 ON t JOIN t t2525 ON t JOIN t t2526 ON t JOIN t t2527 ON t
 JOIN t t2528 ON t JOIN t t2529 ON t JOIN t t2530 ON t JOIN t t2531 ON t
 JOIN t t2532 ON t JOIN t t2533 ON t JOIN t t2534 ON t JOIN t t2535 ON t
 JOIN t t2536 ON t JOIN t t2537 ON t JOIN t t2538 ON t JOIN t t2539 ON t
 JOIN t t2540 ON t JOIN t t2541 ON t JOIN t t2542 ON t JOIN t t2543 ON t
 JOIN t t2544 ON t JOIN t t2545 ON t JOIN t t2546 ON t JOIN t t2547 ON t
 JOIN t t2548 ON t JOIN t t2549 ON t JOIN t t2550 ON t JOIN t t2551 ON t
 JOIN t t2552 ON t JOIN t t2553 ON t JOIN t t2554 ON t JOIN t t2555 ON t
 JOIN t t2556 ON t JOIN t t2557 ON t JOIN t t2558 ON t JOIN t t2559 ON t
 JOIN t t2560 ON t JOIN t t2561 ON t JOIN t t2562 ON t JOIN t t2563 ON t
 JOIN t t2564 ON t JOIN t t2565 ON t JOIN t t2566 ON t JOIN t t2567 ON t
 JOIN t t2568 ON t JOIN t t2569 ON t JOIN t t2570 ON t JOIN t t2571 ON t
 JOIN t t2572 ON t JOIN t t2573 ON t JOIN t t2574 ON t JOIN t t2575 ON t
 JOIN t t2576 ON t JOIN t t2577 ON t JOIN t t2578 ON t JOIN t t2579 ON t
 JOIN t t2580 ON t JOIN t t2581 ON t JOIN t t2582 ON t JOIN t t2583 ON t
 JOIN t t2584 ON t JOIN t t2585 ON t JOIN t t2586 ON t JOIN t t2587 ON t
 JOIN t t2588 ON t JOIN t t2589 ON t JOIN t t2590 ON t JOIN t t2591 ON t
 JOIN t t2592 ON t JOIN t t2593 ON t JOIN t t2594 ON t JOIN t t2595 ON t
 JOIN t t2596 ON t JOIN t t2597 ON t JOIN t t2598 ON t JOIN t t2599 ON t
 JOIN t t2600 ON t JOIN t t2601 ON t JOIN t t2602 ON t JOIN t t2603 ON t
 JOIN t t2604 ON t JOIN t t2605 ON t JOIN t t2606 ON t JOIN t t2607 ON t
 JOIN t t2608 ON t JOIN t t2609 ON t JOIN t t2610 ON t JOIN t t2611 ON t
 JOIN t t2612 ON t JOIN t t2613 ON t JOIN t t2614 ON t JOIN t t2615 ON t
 JOIN t t2616 ON t JOIN t t2617 ON t JOIN t t2618 ON t JOIN t t2619 ON t
 JOIN t t2620 ON t JOIN t t2621 ON t JOIN t t2622 ON t JOIN t t2623 ON t
 JOIN t t2624 ON t JOIN t t2625 ON t JOIN t t2626 ON t JOIN t t2627 ON t
 JOIN t t2628 ON t JOIN t t2629 ON t JOIN t t2630 ON t JOIN t t2631 ON t
 JOIN t t2632 ON t JOIN t t2633 ON t JOIN t t2634 ON t JOIN t t2635 ON t
 JOIN t t2636 ON t JOIN t t2637 ON t JOIN t t2638 ON t JOIN t t2639 ON t
 JOIN t t2640 ON t JOIN t t2641 ON t JOIN t t2642 ON t JOIN t t2643 ON t
 JOIN t t2644 ON t JOIN t t2645 ON t JOIN t t2646 ON t JOIN t t2647 ON t
 JOIN t t2648 ON t JOIN t t2649 ON t JOIN t t2650 ON t JOIN t t2651 ON t
 JOIN t t2652 ON t JOIN t t2653 ON t JOIN t t2654 ON t JOIN t t2655 ON t
 JOIN t t2656 ON t JOIN t t2657 ON t JOIN t t2658 ON t JOIN t t2659 ON t
 JOIN t t2660 ON t JOIN t t2661 ON t JOIN t t2662 ON t JOIN t t2663 ON t
 JOIN t t2664 ON t JOIN t t2665 ON t JOIN t t2666 ON t JOIN t t2667 ON t
 JOIN t t2668 ON t JOIN t t2669 ON t JOIN t t2670 ON t JOIN t t2671 ON t
 JOIN t t2672 ON t JOIN t t2673 ON t JOIN t t2674 ON t JOIN t t2675 ON t
 JOIN t t2676 ON t JOIN t t2677 ON t JOIN t t2678 ON t JOIN t t2679 ON t
 JOIN t t2680 ON t JOIN t t2681 ON t JOIN t t2682 ON t JOIN t t2683 ON t
 JOIN t t2684 ON t JOIN t t2685 ON t JOIN t t2686 ON t JOIN t t2687 ON t
 JOIN t t2688 ON t JOIN t t2689 ON t JOIN t t2690 ON t JOIN t t2691 ON t
 JOIN t t2692 ON t JOIN t t2693 ON t JOIN t t2694 ON t JOIN t t2695 ON t
 JOIN t t2696 ON t JOIN t t2697 ON t JOIN t t2698 ON t JOIN t t2699 ON t
 JOIN t t2700 ON t JOIN t t2701 ON t JOIN t t2702 ON t JOIN t t2703 ON t
 JOIN t t2704 ON t JOIN t t2705 ON t JOIN t t2706 ON t JOIN t t2707 ON t
 JOIN t t2708 ON t JOIN t t2709 ON t JOIN t t2710 ON t JOIN t t2711 ON t
 JOIN t t2712 ON t JOIN t t2713 ON t JOIN t t2714 ON t JOIN t t2715 ON t
 JOIN t t2716 ON t JOIN t t2717 ON t JOIN t t2718 ON t JOIN t t2719 ON t
 JOIN t t2720 ON t JOIN t t2721 ON t JOIN t t2722 ON t JOIN t t2723 ON t
 JOIN t t2724 ON t JOIN t t2725 ON t JOIN t t2726 ON t JOIN t t2727 ON t
 JOIN t t2728 ON t JOIN t t2729 ON t JOIN t t2730 ON t JOIN t t2731 ON t
 JOIN t t2732 ON t JOIN t t2733 ON t JOIN t t2734 ON t JOIN t t2735 ON t
 JOIN t t2736 ON t JOIN t t2737 ON t JOIN t t2738 ON t JOIN t t2739 ON t
 JOIN t t2740 ON t JOIN t t2741 ON t JOIN t t2742 ON t JOIN t t2743 ON t
 JOIN t t2744 ON t JOIN t t2745 ON t JOIN t t2746 ON t JOIN t t2747 ON t
 JOIN t t2748 ON t JOIN t t2749 ON t JOIN t t2750 ON t JOIN t t2751 ON t
 JOIN t t2752 ON t JOIN t t2753 ON t JOIN t t2754 ON t JOIN t t2755 ON t
 JOIN t t2756 ON t JOIN t t2757 ON t JOIN t t2758 ON t JOIN t t2759 ON t
 JOIN t t2760 ON t JOIN t t2761 ON t JOIN t t2762 ON t JOIN t t2763 ON t
 JOIN t t2764 ON t JOIN t t2765 ON t JOIN t t2766 ON t JOIN t t2767 ON t
 JOIN t t2768 ON t JOIN t t2769 ON t JOIN t t2770 ON t JOIN t t2771 ON t
 JOIN t t2772 ON t JOIN t t2773 ON t JOIN t t2774 ON t JOIN t t2775 ON t
 JOIN t t2776 ON t JOIN t t2777 ON t JOIN t t2778 ON t JOIN t t2779 ON t
 JOIN t t2780 ON t JOIN t t2781 ON t JOIN t t2782 ON t JOIN t t2783 ON t
 JOIN t t2784 ON t JOIN t t2785 ON t JOIN t t2786 ON t JOIN t t2787 ON t
 JOIN t t2788 ON t JOIN t t2789 ON t JOIN t t2790 ON t JOIN t t2791 ON t
 JOIN t t2792 ON t JOIN t t2793 ON t JOIN t t2794 ON t JOIN t t2795 ON t
 JOIN t t2796 ON t JOIN t t2797 ON t JOIN t t2798 ON t JOIN t t2799 ON t
 JOIN t t2800 ON t JOIN t t2801 ON t JOIN t t2802 ON t JOIN t t2803 ON t
 JOIN t t2804 ON t JOIN t t2805 ON t JOIN t t2806 ON t JOIN t t2807 ON t
 JOIN t t2808 ON t JOIN t t2809 ON t JOIN t t2810 ON t JOIN t t2811 ON t
 JOIN t t2812 ON t JOIN t t2813 ON t JOIN t t2814 ON t JOIN t t2815 ON t
 JOIN t t2816 ON t JOIN t t2817 ON t JOIN t t2818 ON t JOIN t t2819 ON t
 JOIN t t2820 ON t JOIN t t2821 ON t JOIN t t2822 ON t JOIN t t2823 ON t
 JOIN t t2824 ON t JOIN t t2825 ON t JOIN t t2826 ON t JOIN t t2827 ON t
 JOIN t t2828 ON t JOIN t t2829 ON t JOIN t t2830 ON t JOIN t t2831 ON t
 JOIN t t2832 ON t JOIN t t2833 ON t JOIN t t2834 ON t JOIN t t2835 ON t
 JOIN t t2836 ON t JOIN t t2837 ON t JOIN t t2838 ON t JOIN t t2839 ON t
 JOIN t t2840 ON t JOIN t t2841 ON t JOIN t t2842 ON t JOIN t t2843 ON t
 JOIN t t2844 ON t JOIN t t2845 ON t JOIN t t2846 ON t JOIN t t2847 ON t
 JOIN t t2848 ON t JOIN t t2849 ON t JOIN t t2850 ON t JOIN t t2851 ON t
 JOIN t t2852 ON t JOIN t t2853 ON t JOIN t t2854 ON t JOIN t t2855 ON t
 JOIN t t2856 ON t JOIN t t2857 ON t JOIN t t2858 ON t JOIN t t2859 ON t
 JOIN t t2860 ON t JOIN t t2861 ON t JOIN t t2862 ON t JOIN t t2863 ON t
 JOIN t t2864 ON t JOIN t t2865 ON t JOIN t t2866 ON t JOIN t t2867 ON t
 JOIN t t2868 ON t JOIN t t2869 ON t JOIN t t2870 ON t JOIN t t2871 ON t
 JOIN t t2872 ON t JOIN t t2873 ON t JOIN t t2874 ON t JOIN t t2875 ON t
 JOIN t t2876 ON t JOIN t t2877 ON t JOIN t t2878 ON t JOIN t t2879 ON t
 JOIN t t2880 ON t JOIN t t2881 ON t JOIN t t2882 ON t JOIN t t2883 ON t
 JOIN t t2884 ON t JOIN t t2885 ON t JOIN t t2886 ON t JOIN t t2887 ON t
 JOIN t t2888 ON t JOIN t t2889 ON t JOIN t t2890 ON t JOIN t t2891 ON t
 JOIN t t2892 ON t JOIN t t2893 ON t JOIN t t2894 ON t JOIN t t2895 ON t
 JOIN t t2896 ON t JOIN t t2897 ON t JOIN t t2898 ON t JOIN t t2899 ON t
 JOIN t t2900 ON t JOIN t t2901 ON t JOIN t t2902 ON t JOIN t t2903 ON t
 JOIN t t2904 ON t JOIN t t2905 ON t JOIN t t2906 ON t JOIN t t2907 ON t
 JOIN t t2908 ON t JOIN t t2909 ON t JOIN t t2910 ON t JOIN t t2911 ON t
 JOIN t t2912 ON t JOIN t t2913 ON t JOIN t t2914 ON t JOIN t t2915 ON t
 JOIN t t2916 ON t JOIN t t2917 ON t JOIN t t2918 ON t JOIN t t2919 ON t
 JOIN t t2920 ON t JOIN t t2921 ON t JOIN t t2922 ON t JOIN t t2923 ON t
 JOIN t t2924 ON t JOIN t t2925 ON t JOIN t t2926 ON t JOIN t t2927 ON t
 JOIN t t2928 ON t JOIN t t2929 ON t JOIN t t2930 ON t JOIN t t2931 ON t
 JOIN t t2932 ON t JOIN t t2933 ON t JOIN t t2934 ON t JOIN t t2935 ON t
 JOIN t t2936 ON t JOIN t t2937 ON t JOIN t t2938 ON t JOIN t t2939 ON t
 JOIN t t2940 ON t JOIN t t2941 ON t JOIN t t2942 ON t JOIN t t2943 ON t
 JOIN t t2944 ON t JOIN t t2945 ON t JOIN t t2946 ON t JOIN t t2947 ON t
 JOIN t t2948 ON t JOIN t t2949 ON t JOIN t t2950 ON t JOIN t t2951 ON t
 JOIN t t2952 ON t JOIN t t2953 ON t JOIN t t2954 ON t JOIN t t2955 ON t
 JOIN t t2956 ON t JOIN t t2957 ON t JOIN t t2958 ON t JOIN t t2959 ON t
 JOIN t t2960 ON t JOIN t t2961 ON t JOIN t t2962 ON t JOIN t t2963 ON t
 JOIN t t2964 ON t JOIN t t2965 ON t JOIN t t2966 ON t JOIN t t2967 ON t
 JOIN t t2968 ON t JOIN t t2969 ON t JOIN t t2970 ON t JOIN t t2971 ON t
 JOIN t t2972 ON t JOIN t t2973 ON t JOIN t t2974 ON t JOIN t t2975 ON t
 JOIN t t2976 ON t JOIN t t2977 ON t JOIN t t2978 ON t JOIN t t2979 ON t
 JOIN t t2980 ON t JOIN t t2981 ON t JOIN t t2982 ON t JOIN t t2983 ON t
 JOIN t t2984 ON t JOIN t t2985 ON t JOIN t t2986 ON t JOIN t t2987 ON t
 JOIN t t2988 ON t JOIN t t2989 ON t JOIN t t2990 ON t JOIN t t2991 ON t
 JOIN t t2992 ON t JOIN t t2993 ON t JOIN t t2994 ON t JOIN t t2995 ON t
 JOIN t t2996 ON t JOIN t t2997 ON t JOIN t t2998 ON t JOIN t t2999 ON t
 JOIN t t3000 ON t JOIN t t3001 ON t JOIN t t3002 ON t JOIN t t3003 ON t
 JOIN t t3004 ON t JOIN t t3005 ON t JOIN t t3006 ON t JOIN t t3007 ON t
 JOIN t t3008 ON t JOIN t t3009 ON t JOIN t t3010 ON t JOIN t t3011 ON t
 JOIN t t3012 ON t JOIN t t3013 ON t JOIN t t3014 ON t JOIN t t3015 ON t
 JOIN t t3016 ON t JOIN t t3017 ON t JOIN t t3018 ON t JOIN t t3019 ON t
 JOIN t t3020 ON t JOIN t t3021 ON t JOIN t t3022 ON t JOIN t t3023 ON t
 JOIN t t3024 ON t JOIN t t3025 ON t JOIN t t3026 ON t JOIN t t3027 ON t
 JOIN t t3028 ON t JOIN t t3029 ON t JOIN t t3030 ON t JOIN t t3031 ON t
 JOIN t t3032 ON t JOIN t t3033 ON t JOIN t t3034 ON t JOIN t t3035 ON t
 JOIN t t3036 ON t JOIN t t3037 ON t JOIN t t3038 ON t JOIN t t3039 ON t
 JOIN t t3040 ON t JOIN t t3041 ON t JOIN t t3042 ON t JOIN t t3043 ON t
 JOIN t t3044 ON t JOIN t t3045 ON t JOIN t t3046 ON t JOIN t t3047 ON t
 JOIN t t3048 ON t JOIN t t3049 ON t JOIN t t3050 ON t JOIN t t3051 ON t
 JOIN t t3052 ON t JOIN t t3053 ON t JOIN t t3054 ON t JOIN t t3055 ON t
 JOIN t t3056 ON t JOIN t t3057 ON t JOIN t t3058 ON t JOIN t t3059 ON t
 JOIN t t3060 ON t JOIN t t3061 ON t JOIN t t3062 ON t JOIN t t3063 ON t
 JOIN t t3064 ON t JOIN t t3065 ON t JOIN t t3066 ON t JOIN t t3067 ON t
 JOIN t t3068 ON t JOIN t t3069 ON t JOIN t t3070 ON t JOIN t t3071 ON t
 JOIN t t3072 ON t JOIN t t3073 ON t JOIN t t3074 ON t JOIN t t3075 ON t
 JOIN t t3076 ON t JOIN t t3077 ON t JOIN t t3078 ON t JOIN t t3079 ON t
 JOIN t t3080 ON t JOIN t t3081 ON t JOIN t t3082 ON t JOIN t t3083 ON t
 JOIN t t3084 ON t JOIN t t3085 ON t JOIN t t3086 ON t JOIN t t3087 ON t
 JOIN t t3088 ON t JOIN t t3089 ON t JOIN t t3090 ON t JOIN t t3091 ON t
 JOIN t t3092 ON t JOIN t t3093 ON t JOIN t t3094 ON t JOIN t t3095 ON t
 JOIN t t3096 ON t JOIN t t3097 ON t JOIN t t3098 ON t JOIN t t3099 ON t
 JOIN t t3100 ON t JOIN t t3101 ON t JOIN t t3102 ON t JOIN t t3103 ON t
 JOIN t t3104 ON t JOIN t t3105 ON t JOIN t t3106 ON t JOIN t t3107 ON t
 JOIN t t3108 ON t JOIN t t3109 ON t JOIN t t3110 ON t JOIN t t3111 ON t
 JOIN t t3112 ON t JOIN t t3113 ON t JOIN t t3114 ON t JOIN t t3115 ON t
 JOIN t t3116 ON t JOIN t t3117 ON t JOIN t t3118 ON t JOIN t t3119 ON t
 JOIN t t3120 ON t JOIN t t3121 ON t JOIN t t3122 ON t JOIN t t3123 ON t
 JOIN t t3124 ON t JOIN t t3125 ON t JOIN t t3126 ON t JOIN t t3127 ON t
 JOIN t t3128 ON t JOIN t t3129 ON t JOIN t t3130 ON t JOIN t t3131 ON t
 JOIN t t3132 ON t JOIN t t3133 ON t JOIN t t3134 ON t JOIN t t3135 ON t
 JOIN t t3136 ON t JOIN t t3137 ON t JOIN t t3138 ON t JOIN t t3139 ON t
 JOIN t t3140 ON t JOIN t t3141 ON t JOIN t t3142 ON t JOIN t t3143 ON t
 JOIN t t3144 ON t JOIN t t3145 ON t JOIN t t3146 ON t JOIN t t3147 ON t
 JOIN t t3148 ON t JOIN t t3149 ON t JOIN t t3150 ON t JOIN t t3151 ON t
 JOIN t t3152 ON t JOIN t t3153 ON t JOIN t t3154 ON t JOIN t t3155 ON t
 JOIN t t3156 ON t JOIN t t3157 ON t JOIN t t3158 ON t JOIN t t3159 ON t
 JOIN t t3160 ON t JOIN t t3161 ON t JOIN t t3162 ON t JOIN t t3163 ON t
 JOIN t t3164 ON t JOIN t t3165 ON t JOIN t t3166 ON t JOIN t t3167 ON t
 JOIN t t3168 ON t JOIN t t3169 ON t JOIN t t3170 ON t JOIN t t3171 ON t
 JOIN t t3172 ON t JOIN t t3173 ON t JOIN t t3174 ON t JOIN t t3175 ON t
 JOIN t t3176 ON t JOIN t t3177 ON t JOIN t t3178 ON t JOIN t t3179 ON t
 JOIN t t3180 ON t JOIN t t3181 ON t JOIN t t3182 ON t JOIN t t3183 ON t
 JOIN t t3184 ON t JOIN t t3185 ON t JOIN t t3186 ON t JOIN t t3187 ON t
 JOIN t t3188 ON t JOIN t t3189 ON t JOIN t t3190 ON t JOIN t t3191 ON t
 JOIN t t3192 ON t JOIN t t3193 ON t JOIN t t3194 ON t JOIN t t3195 ON t
 JOIN t t3196 ON t JOIN t t3197 ON t JOIN t t3198 ON t JOIN t t3199 ON t
 JOIN t t3200 ON t JOIN t t3201 ON t JOIN t t3202 ON t JOIN t t3203 ON t
 JOIN t t3204 ON t JOIN t t3205 ON t JOIN t t3206 ON t JOIN t t3207 ON t
 JOIN t t3208 ON t JOIN t t3209 ON t JOIN t t3210 ON t JOIN t t3211 ON t
 JOIN t t3212 ON t JOIN t t3213 ON t JOIN t t3214 ON t JOIN t t3215 ON t
 JOIN t t3216 ON t JOIN t t3217 ON t JOIN t t3218 ON t JOIN t t3219 ON t
 JOIN t t3220 ON t JOIN t t3221 ON t JOIN t t3222 ON t JOIN t t3223 ON t
 JOIN t t3224 ON t JOIN t t3225 ON t JOIN t t3226 ON t JOIN t t3227 ON t
 JOIN t t3228 ON t JOIN t t3229 ON t JOIN t t3230 ON t JOIN t t3231 ON t
 JOIN t t3232 ON t JOIN t t3233 ON t JOIN t t3234 ON t JOIN t t3235 ON t
 JOIN t t3236 ON t JOIN t t3237 ON t JOIN t t3238 ON t JOIN t t3239 ON t
 JOIN t t3240 ON t JOIN t t3241 ON t JOIN t t3242 ON t JOIN t t3243 ON t
 JOIN t t3244 ON t JOIN t t3245 ON t JOIN t t3246 ON t JOIN t t3247 ON t
 JOIN t t3248 ON t JOIN t t3249 ON t JOIN t t3250 ON t JOIN t t3251 ON t
 JOIN t t3252 ON t JOIN t t3253 ON t JOIN t t3254 ON t JOIN t t3255 ON t
 JOIN t t3256 ON t JOIN t t3257 ON t JOIN t t3258 ON t JOIN t t3259 ON t
 JOIN t t3260 ON t JOIN t t3261 ON t JOIN t t3262 ON t JOIN t t3263 ON t
 JOIN t t3264 ON t JOIN t t3265 ON t JOIN t t3266 ON t JOIN t t3267 ON t
 JOIN t t3268 ON t JOIN t t3269 ON t JOIN t t3270 ON t JOIN t t3271 ON t
 JOIN t t3272 ON t JOIN t t3273 ON t JOIN t t3274 ON t JOIN t t3275 ON t
 JOIN t t3276 ON t JOIN t t3277 ON t JOIN t t3278 ON t JOIN t t3279 ON t
 JOIN t t3280 ON t JOIN t t3281 ON t JOIN t t3282 ON t JOIN t t3283 ON t
 JOIN t t3284 ON t JOIN t t3285 ON t JOIN t t3286 ON t JOIN t t3287 ON t
 JOIN t t3288 ON t JOIN t t3289 ON t JOIN t t3290 ON t JOIN t t3291 ON t
 JOIN t t3292 ON t JOIN t t3293 ON t JOIN t t3294 ON t JOIN t t3295 ON t
 JOIN t t3296 ON t JOIN t t3297 ON t JOIN t t3298 ON t JOIN t t3299 ON t
 JOIN t t3300 ON t JOIN t t3301 ON t JOIN t t3302 ON t JOIN t t3303 ON t
 JOIN t t3304 ON t JOIN t t3305 ON t JOIN t t3306 ON t JOIN t t3307 ON t
 JOIN t t3308 ON t JOIN t t3309 ON t JOIN t t3310 ON t JOIN t t3311 ON t
 JOIN t t3312 ON t JOIN t t3313 ON t JOIN t t3314 ON t JOIN t t3315 ON t
 JOIN t t3316 ON t JOIN t t3317 ON t JOIN t t3318 ON t JOIN t t3319 ON t
 JOIN t t3320 ON t JOIN t t3321 ON t JOIN t t3322 ON t JOIN t t3323 ON t
 JOIN t t3324 ON t JOIN t t3325 ON t JOIN t t3326 ON t JOIN t t3327 ON t
 JOIN t t3328 ON t JOIN t t3329 ON t JOIN t t3330 ON t JOIN t t3331 ON t
 JOIN t t3332 ON t JOIN t t3333 ON t JOIN t t3334 ON t JOIN t t3335 ON t
 JOIN t t3336 ON t JOIN t t3337 ON t JOIN t t3338 ON t JOIN t t3339 ON t
 JOIN t t3340 ON t JOIN t t3341 ON t JOIN t t3342 ON t JOIN t t3343 ON t
 JOIN t t3344 ON t JOIN t t3345 ON t JOIN t t3346 ON t JOIN t t3347 ON t
 JOIN t t3348 ON t JOIN t t3349 ON t JOIN t t3350 ON t JOIN t t3351 ON t
 JOIN t t3352 ON t JOIN t t3353 ON t JOIN t t3354 ON t JOIN t t3355 ON t
 JOIN t t3356 ON t JOIN t t3357 ON t JOIN t t3358 ON t JOIN t t3359 ON t
 JOIN t t3360 ON t JOIN t t3361 ON t JOIN t t3362 ON t JOIN t t3363 ON t
 JOIN t t3364 ON t JOIN t t3365 ON t JOIN t t3366 ON t JOIN t t3367 ON t
 JOIN t t3368 ON t JOIN t t3369 ON t JOIN t t3370 ON t JOIN t t3371 ON t
 JOIN t t3372 ON t JOIN t t3373 ON t JOIN t t3374 ON t JOIN t t3375 ON t
 JOIN t t3376 ON t JOIN t t3377 ON t JOIN t t3378 ON t JOIN t t3379 ON t
 JOIN t t3380 ON t JOIN t t3381 ON t JOIN t t3382 ON t JOIN t t3383 ON t
 JOIN t t3384 ON t JOIN t t3385 ON t JOIN t t3386 ON t JOIN t t3387 ON t
 JOIN t t3388 ON t JOIN t t3389 ON t JOIN t t3390 ON t JOIN t t3391 ON t
 JOIN t t3392 ON t JOIN t t3393 ON t JOIN t t3394 ON t JOIN t t3395 ON t
 JOIN t t3396 ON t JOIN t t3397 ON t JOIN t t3398 ON t JOIN t t3399 ON t
 JOIN t t3400 ON t JOIN t t3401 ON t JOIN t t3402 ON t JOIN t t3403 ON t
 JOIN t t3404 ON t JOIN t t3405 ON t JOIN t t3406 ON t JOIN t t3407 ON t
 JOIN t t3408 ON t JOIN t t3409 ON t JOIN t t3410 ON t JOIN t t3411 ON t
 JOIN t t3412 ON t JOIN t t3413 ON t JOIN t t3414 ON t JOIN t t3415 ON t
 JOIN t t3416 ON t JOIN t t3417 ON t JOIN t t3418 ON t JOIN t t3419 ON t
 JOIN t t3420 ON t JOIN t t3421 ON t JOIN t t3422 ON t JOIN t t3423 ON t
 JOIN t t3424 ON t JOIN t t3425 ON t JOIN t t3426 ON t JOIN t t3427 ON t
 JOIN t t3428 ON t JOIN t t3429 ON t JOIN t t3430 ON t JOIN t t3431 ON t
 JOIN t t3432 ON t JOIN t t3433 ON t JOIN t t3434 ON t JOIN t t3435 ON t
 JOIN t t3436 ON t JOIN t t3437 ON t JOIN t t3438 ON t JOIN t t3439 ON t
 JOIN t t3440 ON t JOIN t t3441 ON t JOIN t t3442 ON t JOIN t t3443 ON t
 JOIN t t3444 ON t JOIN t t3445 ON t JOIN t t3446 ON t JOIN t t3447 ON t
 JOIN t t3448 ON t JOIN t t3449 ON t JOIN t t3450 ON t JOIN t t3451 ON t
 JOIN t t3452 ON t JOIN t t3453 ON t JOIN t t3454 ON t JOIN t t3455 ON t
 JOIN t t3456 ON t JOIN t t3457 ON t JOIN t t3458 ON t JOIN t t3459 ON t
 JOIN t t3460 ON t JOIN t t3461 ON t JOIN t t3462 ON t JOIN t t3463 ON t
 JOIN t t3464 ON t JOIN t t3465 ON t JOIN t t3466 ON t JOIN t t3467 ON t
 JOIN t t3468 ON t JOIN t t3469 ON t JOIN t t3470 ON t JOIN t t3471 ON t
 JOIN t t3472 ON t JOIN t t3473 ON t JOIN t t3474 ON t JOIN t t3475 ON t
 JOIN t t3476 ON t JOIN t t3477 ON t JOIN t t3478 ON t JOIN t t3479 ON t
 JOIN t t3480 ON t JOIN t t3481 ON t JOIN t t3482 ON t JOIN t t3483 ON t
 JOIN t t3484 ON t JOIN t t3485 ON t JOIN t t3486 ON t JOIN t t3487 ON t
 JOIN t t3488 ON t JOIN t t3489 ON t JOIN t t3490 ON t JOIN t t3491 ON t
 JOIN t t3492 ON t JOIN t t3493 ON t JOIN t t3494 ON t JOIN t t3495 ON t
 JOIN t t3496 ON t JOIN t t3497 ON t JOIN t t3498 ON t JOIN t t3499 ON t
 JOIN t t3500 ON t JOIN t t3501 ON t JOIN t t3502 ON t JOIN t t3503 ON t
 JOIN t t3504 ON t JOIN t t3505 ON t JOIN t t3506 ON t JOIN t t3507 ON t
 JOIN t t3508 ON t JOIN t t3509 ON t JOIN t t3510 ON t JOIN t t3511 ON t
 JOIN t t3512 ON t JOIN t t3513 ON t JOIN t t3514 ON t JOIN t t3515 ON t
 JOIN t t3516 ON t JOIN t t3517 ON t JOIN t t3518 ON t JOIN t t3519 ON t
 JOIN t t3520 ON t JOIN t t3521 ON t JOIN t t3522 ON t JOIN t t3523 ON t
 JOIN t t3524 ON t JOIN t t3525 ON t JOIN t t3526 ON t JOIN t t3527 ON t
 JOIN t t3528 ON t JOIN t t3529 ON t JOIN t t3530 ON t JOIN t t3531 ON t
 JOIN t t3532 ON t JOIN t t3533 ON t JOIN t t3534 ON t JOIN t t3535 ON t
 JOIN t t3536 ON t JOIN t t3537 ON t JOIN t t3538 ON t JOIN t t3539 ON t
 JOIN t t3540 ON t JOIN t t3541 ON t JOIN t t3542 ON t JOIN t t3543 ON t
 JOIN t t3544 ON t JOIN t t3545 ON t JOIN t t3546 ON t JOIN t t3547 ON t
 JOIN t t3548 ON t JOIN t t3549 ON t JOIN t t3550 ON t JOIN t t3551 ON t
 JOIN t t3552 ON t JOIN t t3553 ON t JOIN t t3554 ON t JOIN t t3555 ON t
 JOIN t t3556 ON t JOIN t t3557 ON t JOIN t t3558 ON t JOIN t t3559 ON t
 JOIN t t3560 ON t JOIN t t3561 ON t JOIN t t3562 ON t JOIN t t3563 ON t
 JOIN t t3564 ON t JOIN t t3565 ON t JOIN t t3566 ON t JOIN t t3567 ON t
 JOIN t t3568 ON t JOIN t t3569 ON t JOIN t t3570 ON t JOIN t t3571 ON t
 JOIN t t3572 ON t JOIN t t3573 ON t JOIN t t3574 ON t JOIN t t3575 ON t
 JOIN t t3576 ON t JOIN t t3577 ON t JOIN t t3578 ON t JOIN t t3579 ON t
 JOIN t t3580 ON t JOIN t t3581 ON t JOIN t t3582 ON t JOIN t t3583 ON t
 JOIN t t3584 ON t JOIN t t3585 ON t JOIN t t3586 ON t JOIN t t3587 ON t
 JOIN t t3588 ON t JOIN t t3589 ON t JOIN t t3590 ON t JOIN t t3591 ON t
 JOIN t t3592 ON t JOIN t t3593 ON t JOIN t t3594 ON t JOIN t t3595 ON t
 JOIN t t3596 ON t JOIN t t3597 ON t JOIN t t3598 ON t JOIN t t3599 ON t
 JOIN t t3600 ON t JOIN t t3601 ON t JOIN t t3602 ON t JOIN t t3603 ON t
 JOIN t t3604 ON t JOIN t t3605 ON t JOIN t t3606 ON t JOIN t t3607 ON t
 JOIN t t3608 ON t JOIN t t3609 ON t JOIN t t3610 ON t JOIN t t3611 ON t
 JOIN t t3612 ON t JOIN t t3613 ON t JOIN t t3614 ON t JOIN t t3615 ON t
 JOIN t t3616 ON t JOIN t t3617 ON t JOIN t t3618 ON t JOIN t t3619 ON t
 JOIN t t3620 ON t JOIN t t3621 ON t JOIN t t3622 ON t JOIN t t3623 ON t
 JOIN t t3624 ON t JOIN t t3625 ON t JOIN t t3626 ON t JOIN t t3627 ON t
 JOIN t t3628 ON t JOIN t t3629 ON t JOIN t t3630 ON t JOIN t t3631 ON t
 JOIN t t3632 ON t JOIN t t3633 ON t JOIN t t3634 ON t JOIN t t3635 ON t
 JOIN t t3636 ON t JOIN t t3637 ON t JOIN t t3638 ON t JOIN t t3639 ON t
 JOIN t t3640 ON t JOIN t t3641 ON t JOIN t t3642 ON t JOIN t t3643 ON t
 JOIN t t3644 ON t JOIN t t3645 ON t JOIN t t3646 ON t JOIN t t3647 ON t
 JOIN t t3648 ON t JOIN t t3649 ON t JOIN t t3650 ON t JOIN t t3651 ON t
 JOIN t t3652 ON t JOIN t t3653 ON t JOIN t t3654 ON t JOIN t t3655 ON t
 JOIN t t3656 ON t JOIN t t3657 ON t JOIN t t3658 ON t JOIN t t3659 ON t
 JOIN t t3660 ON t JOIN t t3661 ON t JOIN t t3662 ON t JOIN t t3663 ON t
 JOIN t t3664 ON t JOIN t t3665 ON t JOIN t t3666 ON t JOIN t t3667 ON t
 JOIN t t3668 ON t JOIN t t3669 ON t JOIN t t3670 ON t JOIN t t3671 ON t
 JOIN t t3672 ON t JOIN t t3673 ON t JOIN t t3674 ON t JOIN t t3675 ON t
 JOIN t t3676 ON t JOIN t t3677 ON t JOIN t t3678 ON t JOIN t t3679 ON t
 JOIN t t3680 ON t JOIN t t3681 ON t JOIN t t3682 ON t JOIN t t3683 ON t
 JOIN t t3684 ON t JOIN t t3685 ON t JOIN t t3686 ON t JOIN t t3687 ON t
 JOIN t t3688 ON t JOIN t t3689 ON t JOIN t t3690 ON t JOIN t t3691 ON t
 JOIN t t3692 ON t JOIN t t3693 ON t JOIN t t3694 ON t JOIN t t3695 ON t
 JOIN t t3696 ON t JOIN t t3697 ON t JOIN t t3698 ON t JOIN t t3699 ON t
 JOIN t t3700 ON t JOIN t t3701 ON t JOIN t t3702 ON t JOIN t t3703 ON t
 JOIN t t3704 ON t JOIN t t3705 ON t JOIN t t3706 ON t JOIN t t3707 ON t
 JOIN t t3708 ON t JOIN t t3709 ON t JOIN t t3710 ON t JOIN t t3711 ON t
 JOIN t t3712 ON t JOIN t t3713 ON t JOIN t t3714 ON t JOIN t t3715 ON t
 JOIN t t3716 ON t JOIN t t3717 ON t JOIN t t3718 ON t JOIN t t3719 ON t
 JOIN t t3720 ON t JOIN t t3721 ON t JOIN t t3722 ON t JOIN t t3723 ON t
 JOIN t t3724 ON t JOIN t t3725 ON t JOIN t t3726 ON t JOIN t t3727 ON t
 JOIN t t3728 ON t JOIN t t3729 ON t JOIN t t3730 ON t JOIN t t3731 ON t
 JOIN t t3732 ON t JOIN t t3733 ON t JOIN t t3734 ON t JOIN t t3735 ON t
 JOIN t t3736 ON t JOIN t t3737 ON t JOIN t t3738 ON t JOIN t t3739 ON t
 JOIN t t3740 ON t JOIN t t3741 ON t JOIN t t3742 ON t JOIN t t3743 ON t
 JOIN t t3744 ON t JOIN t t3745 ON t JOIN t t3746 ON t JOIN t t3747 ON t
 JOIN t t3748 ON t JOIN t t3749 ON t JOIN t t3750 ON t JOIN t t3751 ON t
 JOIN t t3752 ON t JOIN t t3753 ON t JOIN t t3754 ON t JOIN t t3755 ON t
 JOIN t t3756 ON t JOIN t t3757 ON t JOIN t t3758 ON t JOIN t t3759 ON t
 JOIN t t3760 ON t JOIN t t3761 ON t JOIN t t3762 ON t JOIN t t3763 ON t
 JOIN t t3764 ON t JOIN t t3765 ON t JOIN t t3766 ON t JOIN t t3767 ON t
 JOIN t t3768 ON t JOIN t t3769 ON t JOIN t t3770 ON t JOIN t t3771 ON t
 JOIN t t3772 ON t JOIN t t3773 ON t JOIN t t3774 ON t JOIN t t3775 ON t
 JOIN t t3776 ON t JOIN t t3777 ON t JOIN t t3778 ON t JOIN t t3779 ON t
 JOIN t t3780 ON t JOIN t t3781 ON t JOIN t t3782 ON t JOIN t t3783 ON t
 JOIN t t3784 ON t JOIN t t3785 ON t JOIN t t3786 ON t JOIN t t3787 ON t
 JOIN t t3788 ON t JOIN t t3789 ON t JOIN t t3790 ON t JOIN t t3791 ON t
 JOIN t t3792 ON t JOIN t t3793 ON t JOIN t t3794 ON t JOIN t t3795 ON t
 JOIN t t3796 ON t JOIN t t3797 ON t JOIN t t3798 ON t JOIN t t3799 ON t
 JOIN t t3800 ON t JOIN t t3801 ON t JOIN t t3802 ON t JOIN t t3803 ON t
 JOIN t t3804 ON t JOIN t t3805 ON t JOIN t t3806 ON t JOIN t t3807 ON t
 JOIN t t3808 ON t JOIN t t3809 ON t JOIN t t3810 ON t JOIN t t3811 ON t
 JOIN t t3812 ON t JOIN t t3813 ON t JOIN t t3814 ON t JOIN t t3815 ON t
 JOIN t t3816 ON t JOIN t t3817 ON t JOIN t t3818 ON t JOIN t t3819 ON t
 JOIN t t3820 ON t JOIN t t3821 ON t JOIN t t3822 ON t JOIN t t3823 ON t
 JOIN t t3824 ON t JOIN t t3825 ON t JOIN t t3826 ON t JOIN t t3827 ON t
 JOIN t t3828 ON t JOIN t t3829 ON t JOIN t t3830 ON t JOIN t t3831 ON t
 JOIN t t3832 ON t JOIN t t3833 ON t JOIN t t3834 ON t JOIN t t3835 ON t
 JOIN t t3836 ON t JOIN t t3837 ON t JOIN t t3838 ON t JOIN t t3839 ON t
 JOIN t t3840 ON t JOIN t t3841 ON t JOIN t t3842 ON t JOIN t t3843 ON t
 JOIN t t3844 ON t JOIN t t3845 ON t JOIN t t3846 ON t JOIN t t3847 ON t
 JOIN t t3848 ON t JOIN t t3849 ON t JOIN t t3850 ON t JOIN t t3851 ON t
 JOIN t t3852 ON t JOIN t t3853 ON t JOIN t t3854 ON t JOIN t t3855 ON t
 JOIN t t3856 ON t JOIN t t3857 ON t JOIN t t3858 ON t JOIN t t3859 ON t
 JOIN t t3860 ON t JOIN t t3861 ON t JOIN t t3862 ON t JOIN t t3863 ON t
 JOIN t t3864 ON t JOIN t t3865 ON t JOIN t t3866 ON t JOIN t t3867 ON t
 JOIN t t3868 ON t JOIN t t3869 ON t JOIN t t3870 ON t JOIN t t3871 ON t
 JOIN t t3872 ON t JOIN t t3873 ON t JOIN t t3874 ON t JOIN t t3875 ON t
 JOIN t t3876 ON t JOIN t t3877 ON t JOIN t t3878 ON t JOIN t t3879 ON t
 JOIN t t3880 ON t JOIN t t3881 ON t JOIN t t3882 ON t JOIN t t3883 ON t
 JOIN t t3884 ON t JOIN t t3885 ON t JOIN t t3886 ON t JOIN t t3887 ON t
 JOIN t t3888 ON t JOIN t t3889 ON t JOIN t t3890 ON t JOIN t t3891 ON t
 JOIN t t3892 ON t JOIN t t3893 ON t JOIN t t3894 ON t JOIN t t3895 ON t
 JOIN t t3896 ON t JOIN t t3897 ON t JOIN t t3898 ON t JOIN t t3899 ON t
 JOIN t t3900 ON t JOIN t t3901 ON t JOIN t t3902 ON t JOIN t t3903 ON t
 JOIN t t3904 ON t JOIN t t3905 ON t JOIN t t3906 ON t JOIN t t3907 ON t
 JOIN t t3908 ON t JOIN t t3909 ON t JOIN t t3910 ON t JOIN t t3911 ON t
 JOIN t t3912 ON t JOIN t t3913 ON t JOIN t t3914 ON t JOIN t t3915 ON t
 JOIN t t3916 ON t JOIN t t3917 ON t JOIN t t3918 ON t JOIN t t3919 ON t
 JOIN t t3920 ON t JOIN t t3921 ON t JOIN t t3922 ON t JOIN t t3923 ON t
 JOIN t t3924 ON t JOIN t t3925 ON t JOIN t t3926 ON t JOIN t t3927 ON t
 JOIN t t3928 ON t JOIN t t3929 ON t JOIN t t3930 ON t JOIN t t3931 ON t
 JOIN t t3932 ON t JOIN t t3933 ON t JOIN t t3934 ON t JOIN t t3935 ON t
 JOIN t t3936 ON t JOIN t t3937 ON t JOIN t t3938 ON t JOIN t t3939 ON t
 JOIN t t3940 ON t JOIN t t3941 ON t JOIN t t3942 ON t JOIN t t3943 ON t
 JOIN t t3944 ON t JOIN t t3945 ON t JOIN t t3946 ON t JOIN t t3947 ON t
 JOIN t t3948 ON t JOIN t t3949 ON t JOIN t t3950 ON t JOIN t t3951 ON t
 JOIN t t3952 ON t JOIN t t3953 ON t JOIN t t3954 ON t JOIN t t3955 ON t
 JOIN t t3956 ON t JOIN t t3957 ON t JOIN t t3958 ON t JOIN t t3959 ON t
 JOIN t t3960 ON t JOIN t t3961 ON t JOIN t t3962 ON t JOIN t t3963 ON t
 JOIN t t3964 ON t JOIN t t3965 ON t JOIN t t3966 ON t JOIN t t3967 ON t
 JOIN t t3968 ON t JOIN t t3969 ON t JOIN t t3970 ON t JOIN t t3971 ON t
 JOIN t t3972 ON t JOIN t t3973 ON t JOIN t t3974 ON t JOIN t t3975 ON t
 JOIN t t3976 ON t JOIN t t3977 ON t JOIN t t3978 ON t JOIN t t3979 ON t
 JOIN t t3980 ON t JOIN t t3981 ON t JOIN t t3982 ON t JOIN t t3983 ON t
 JOIN t t3984 ON t JOIN t t3985 ON t JOIN t t3986 ON t JOIN t t3987 ON t
 JOIN t t3988 ON t JOIN t t3989 ON t JOIN t t3990 ON t JOIN t t3991 ON t
 JOIN t t3992 ON t JOIN t t3993 ON t JOIN t t3994 ON t JOIN t t3995 ON t
 JOIN t t3996 ON t JOIN t t3997 ON t JOIN t t3998 ON t JOIN t t3999 ON t
 JOIN t t4000 ON t;
--enable_result_log

DROP TABLE t;
