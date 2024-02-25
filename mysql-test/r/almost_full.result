call mtr.add_suppression("The table 't1' is full");
drop table if exists t1;
set global myisam_data_pointer_size=2;
CREATE TABLE t1 (a int auto_increment primary key not null, b longtext) ENGINE=MyISAM;
DELETE FROM t1 WHERE a=1 or a=5;
INSERT INTO t1 SET b=repeat('a',600);
ERROR HY000: The table 't1' is full
CHECK TABLE t1 EXTENDED;
Table	Op	Msg_type	Msg_text
test.t1	check	warning	Datafile is almost full, 65448 of 65534 used
test.t1	check	status	OK
UPDATE t1 SET b=repeat('a', 800) where a=10;
ERROR HY000: The table 't1' is full
CHECK TABLE t1 EXTENDED;
Table	Op	Msg_type	Msg_text
test.t1	check	warning	Datafile is almost full, 65448 of 65534 used
test.t1	check	status	OK
INSERT INTO t1 SET b=repeat('a',400);
CHECK TABLE t1 EXTENDED;
Table	Op	Msg_type	Msg_text
test.t1	check	warning	Datafile is almost full, 65448 of 65534 used
test.t1	check	status	OK
DELETE FROM t1 WHERE a=2 or a=6;
UPDATE t1 SET b=repeat('a', 600) where a=11;
CHECK TABLE t1 EXTENDED;
Table	Op	Msg_type	Msg_text
test.t1	check	warning	Datafile is almost full, 65448 of 65534 used
test.t1	check	status	OK
drop table t1;
set global myisam_data_pointer_size=default;
