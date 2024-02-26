#
# Test that INFORMATION_SCHEMA.TABLES.UPDATE_TIME is filled
# correctly for InnoDB tables.
#
CREATE TABLE t (a INT) ENGINE=INNODB;
ANALYZE TABLE t;
Table	Op	Msg_type	Msg_text
test.t	analyze	status	OK
SELECT update_time FROM information_schema.tables WHERE table_name = 't';
UPDATE_TIME
NULL
INSERT INTO t VALUES (1);
ANALYZE TABLE t;
Table	Op	Msg_type	Msg_text
test.t	analyze	status	OK
SELECT COUNT(*) FROM information_schema.tables WHERE table_name = 't'
AND update_time IS NOT NULL OR FALSE;
COUNT(*)
1
# We cant deterministically check that the saved value is correct, but
# at least we check that it is a timestamp not older than 2 minutes.
# Usually update_time and NOW() are equal below, but on heavily loaded
# machines NOW() could be younger.
SELECT COUNT(*) FROM information_schema.tables WHERE table_name = 't'
AND TIMESTAMPDIFF(SECOND, update_time, NOW()) < 120 OR FALSE;
COUNT(*)
1
SELECT COUNT(*) FROM information_schema.innodb_buffer_page
WHERE table_name = '`test`.`t`';
COUNT(*)
1
# Emptying InnoDB buffer pool: begin
# Buffer pool size: 10485760 bytes
# Creating and dropping a table with size: 20971520 bytes
# Emptying InnoDB buffer pool: end
SELECT COUNT(*) FROM information_schema.innodb_buffer_page
WHERE table_name = '`test`.`t`';
COUNT(*)
0
ANALYZE TABLE t;
Table	Op	Msg_type	Msg_text
test.t	analyze	status	OK
SELECT COUNT(*) FROM information_schema.tables WHERE table_name = 't'
AND update_time IS NOT NULL OR FALSE;
COUNT(*)
1
# Test the behavior after restart with a prepared XA transaction
XA START 'xatrx';
INSERT INTO t VALUES (5);
XA END 'xatrx';
XA PREPARE 'xatrx';
call mtr.add_suppression("Found 1 prepared XA transactions");
# Kill and restart
ANALYZE TABLE t;
Table	Op	Msg_type	Msg_text
test.t	analyze	status	OK
SELECT update_time FROM information_schema.tables WHERE table_name = 't';
UPDATE_TIME
NULL
XA COMMIT 'xatrx';
ANALYZE TABLE t;
Table	Op	Msg_type	Msg_text
test.t	analyze	status	OK
SELECT COUNT(*) FROM information_schema.tables WHERE table_name = 't'
AND update_time IS NOT NULL OR FALSE;
COUNT(*)
1
DROP TABLE t;
