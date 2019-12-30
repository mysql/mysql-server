--source include/force_myisam_default.inc
--source include/have_myisam.inc

#
# Bug #5679 utf8_unicode_ci LIKE--trailing % doesn't equal zero characters
#
CREATE TABLE t (
  c char(20) NOT NULL
) ENGINE=MyISAM DEFAULT CHARACTER SET utf16 COLLATE utf16_unicode_ci;
INSERT INTO t VALUES ('a'),('ab'),('aba');
ALTER TABLE t ADD INDEX (c);
SELECT c FROM t WHERE c LIKE 'a%';
DROP TABLE t;

#
# Bug #27079 Crash while grouping empty ucs2 strings
#
CREATE TABLE t1 (
 c1 text character set utf16 collate utf16_polish_ci NOT NULL
) ENGINE=MyISAM;
insert into t1 values (''),('a');
SELECT COUNT(*), c1 FROM t1 GROUP BY c1; 
DROP TABLE IF EXISTS t1; 
