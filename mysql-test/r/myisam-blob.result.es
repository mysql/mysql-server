drop table if exists t1;
CREATE TABLE t1 (data LONGBLOB) ENGINE=myisam;
INSERT INTO t1 (data) VALUES (NULL);
UPDATE t1 set data=repeat('a',18*1024*1024);
Warnings:
Warning	1301	Result of repeat() was larger than max_allowed_packet (24) - truncated
select length(data) from t1;
length(data)
NULL
delete from t1 where left(data,1)='a';
check table t1;
Table	Op	Msg_type	Msg_text
test.t1	check	status	OK
truncate table t1;
INSERT INTO t1 (data) VALUES (repeat('a',1*1024*1024));
Warnings:
Warning	1301	Result of repeat() was larger than max_allowed_packet (24) - truncated
INSERT INTO t1 (data) VALUES (repeat('b',16*1024*1024-1024));
Warnings:
Warning	1301	Result of repeat() was larger than max_allowed_packet (24) - truncated
delete from t1 where left(data,1)='b';
check table t1;
Table	Op	Msg_type	Msg_text
test.t1	check	status	OK
UPDATE t1 set data=repeat('c',17*1024*1024);
Warnings:
Warning	1301	Result of repeat() was larger than max_allowed_packet (24) - truncated
Warning	1301	Result of repeat() was larger than max_allowed_packet (24) - truncated
check table t1;
Table	Op	Msg_type	Msg_text
test.t1	check	status	OK
delete from t1 where left(data,1)='c';
check table t1;
Table	Op	Msg_type	Msg_text
test.t1	check	status	OK
INSERT INTO t1 set data=repeat('a',18*1024*1024);
Warnings:
Warning	1301	Result of repeat() was larger than max_allowed_packet (24) - truncated
select length(data) from t1;
length(data)
NULL
NULL
NULL
alter table t1 modify data blob;
select length(data) from t1;
length(data)
NULL
NULL
NULL
drop table t1;
CREATE TABLE t1 (data BLOB) ENGINE=myisam;
INSERT INTO t1 (data) VALUES (NULL);
UPDATE t1 set data=repeat('a',18*1024*1024);
Warnings:
Warning	1301	Result of repeat() was larger than max_allowed_packet (24) - truncated
select length(data) from t1;
length(data)
NULL
drop table t1;
