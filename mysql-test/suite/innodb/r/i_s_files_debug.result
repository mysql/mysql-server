#
# Bug#25200952 	TRANSACTIONS WITH DML WITH SUBQUERIES ON I_S TABLES CAN BLOCK CONCURRENT DDL
#
CREATE TABLE t1 (i INT);
BEGIN;
INSERT INTO t1 (SELECT COUNT(*) FROM information_schema.tables);
# Connection 1
# This create table shouldn't be blocked because of I_S query on mysql.tables in another
# connection
CREATE TABLE t2 (j INT);
commit;
DROP TABLE t1;
DROP TABLE t2;
