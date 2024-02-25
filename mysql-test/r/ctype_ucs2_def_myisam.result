CREATE TABLE t1 (
c1 CHAR(255) CHARACTER SET UCS2 COLLATE UCS2_BIN NOT NULL,
KEY(c1)
) ENGINE=MyISAM;
Warnings:
Warning	1287	'ucs2' is deprecated and will be removed in a future release. Please use utf8mb4 instead
Warning	4079	'ucs2_bin' is a collation of the deprecated character set ucs2. Please consider using utf8mb4 with an appropriate collation instead.
INSERT INTO t1 VALUES ('marshall\'s');
INSERT INTO t1 VALUES ('marsh');
CHECK TABLE t1 EXTENDED;
Table	Op	Msg_type	Msg_text
test.t1	check	status	OK
DROP TABLE t1;
