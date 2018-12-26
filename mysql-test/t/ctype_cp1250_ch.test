
--disable_warnings
drop table if exists t1;
--enable_warnings

--disable_warnings
DROP TABLE IF EXISTS t1;
--enable_warnings

SHOW COLLATION LIKE 'cp1250_czech_cs';

SET @test_character_set= 'cp1250';
SET @test_collation= 'cp1250_general_ci';
-- source include/ctype_common.inc

SET @test_character_set= 'cp1250';
SET @test_collation= 'cp1250_czech_cs';
-- source include/ctype_common.inc



#
# Bugs: #8840: Empty string comparison and character set 'cp1250'
#

CREATE TABLE t1 (a char(16)) character set cp1250 collate cp1250_czech_cs;
INSERT INTO t1 VALUES ('');
SELECT a, length(a), a='', a=' ', a='  ' FROM t1;
DROP TABLE t1;

#
# Bug#9759 Empty result with 'LIKE' and cp1250_czech_cs
#
CREATE TABLE t1 (
  popisek varchar(30) collate cp1250_general_ci NOT NULL default '',
 PRIMARY KEY  (`popisek`)
);
INSERT INTO t1 VALUES ('2005-01-1');
SELECT * FROM t1 WHERE popisek = '2005-01-1';
SELECT * FROM t1 WHERE popisek LIKE '2005-01-1';
drop table t1;

#
# Bug#13347: empty result from query with like and cp1250 charset
#
set names cp1250;
CREATE TABLE t1
(
 id  INT AUTO_INCREMENT PRIMARY KEY,
 str VARCHAR(32)  CHARACTER SET cp1250 COLLATE cp1250_czech_cs NOT NULL default '',
 UNIQUE KEY (str)
);
			
INSERT INTO t1 VALUES (NULL, 'a');
INSERT INTO t1 VALUES (NULL, 'aa');
INSERT INTO t1 VALUES (NULL, 'aaa');
INSERT INTO t1 VALUES (NULL, 'aaaa');
INSERT INTO t1 VALUES (NULL, 'aaaaa');
INSERT INTO t1 VALUES (NULL, 'aaaaaa');
INSERT INTO t1 VALUES (NULL, 'aaaaaaa');
select * from t1 where str like 'aa%';
drop table t1;

#
# Bug#19741 segfault with cp1250 charset + like + primary key + 64bit os
#
set names cp1250;
create table t1 (a varchar(15) collate cp1250_czech_cs NOT NULL, primary key(a));
insert into t1 values("abcdefgh·");
insert into t1 values("··ËË");
select a from t1 where a like "abcdefgh·";
drop table t1;

set names cp1250 collate cp1250_czech_cs;
--source include/ctype_pad_space.inc
--source include/ctype_filesort.inc

# End of 4.1 tests

#
# Bug #48053 String::c_ptr has a race and/or does an invalid 
#            memory reference
#            (triggered by Valgrind tests)
#  (see also ctype_eucjpms.test, ctype_cp1250.test, ctype_cp1251.test)
#
--error 1649
set global LC_MESSAGES=convert((@@global.log_bin_trust_function_creators) 
    using cp1250);


--echo #
--echo # Start of 5.6 tests
--echo #

--echo #
--echo # WL#3664 WEIGHT_STRING
--echo #

--echo #
--echo # Note:
--echo # cp1250_czech_cs does not support WEIGHT_STRING in full extent
--echo #

set names cp1250 collate cp1250_czech_cs;
--source include/weight_string.inc
--source include/weight_string_euro.inc
--source include/weight_string_chde.inc

--echo #
--echo # Bugs#12635232: VALGRIND WARNINGS: IS_IPV6, IS_IPV4, INET6_ATON,
--echo # INET6_NTOA + MULTIBYTE CHARSET.
--echo #

SET NAMES cp1250;
--source include/ctype_inet.inc

--echo #
--echo # End of 5.6 tests
--echo #

