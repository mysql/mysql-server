include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
[connection master]
CALL mtr.add_suppression("Unsafe statement written to the binary log using statement format since BINLOG_FORMAT = STATEMENT");
SET @@session.binlog_direct_non_transactional_updates := if(floor(rand()*10)%2,'ON','OFF');
CREATE TABLE t (a INT) ENGINE=innodb;
CREATE TABLE tm (a INT) ENGINE=myisam;
=== COMMIT ===
XA START 'xa_trx';
INSERT INTO tm VALUES (1);
INSERT INTO t VALUES (1);
XA END 'xa_trx';
XA PREPARE 'xa_trx';
XA COMMIT 'xa_trx' ;
XA START 'xa_trx';
INSERT INTO t VALUES (2);
INSERT INTO tm VALUES (2);
XA END 'xa_trx';
XA PREPARE 'xa_trx';
XA COMMIT 'xa_trx' ;
XA START 'xa_trx';
INSERT INTO tm VALUES (3);
XA END 'xa_trx';
XA PREPARE 'xa_trx';
XA COMMIT 'xa_trx' ;
include/sync_slave_sql_with_master.inc
[connection master]
=== COMMIT ONE PHASE ===
XA START 'xa_trx';
INSERT INTO tm VALUES (1);
INSERT INTO t VALUES (1);
XA END 'xa_trx';
XA COMMIT 'xa_trx' ONE PHASE;
XA START 'xa_trx';
INSERT INTO t VALUES (2);
INSERT INTO tm VALUES (2);
XA END 'xa_trx';
XA COMMIT 'xa_trx' ONE PHASE;
XA START 'xa_trx';
INSERT INTO tm VALUES (3);
XA END 'xa_trx';
XA COMMIT 'xa_trx' ONE PHASE;
include/sync_slave_sql_with_master.inc
[connection master]
=== ROLLBACK with PREPARE ===
XA START 'xa_trx';
INSERT INTO tm VALUES (1);
INSERT INTO t VALUES (1);
XA END 'xa_trx';
XA PREPARE 'xa_trx';
xa rollback 'xa_trx' ;
Warnings:
Warning	1196	Some non-transactional changed tables couldn't be rolled back
XA START 'xa_trx';
INSERT INTO t VALUES (2);
INSERT INTO tm VALUES (2);
XA END 'xa_trx';
XA PREPARE 'xa_trx';
xa rollback 'xa_trx' ;
Warnings:
Warning	1196	Some non-transactional changed tables couldn't be rolled back
XA START 'xa_trx';
INSERT INTO tm VALUES (3);
XA END 'xa_trx';
XA PREPARE 'xa_trx';
xa rollback 'xa_trx' ;
Warnings:
Warning	1196	Some non-transactional changed tables couldn't be rolled back
include/sync_slave_sql_with_master.inc
[connection master]
=== ROLLBACK with no PREPARE ===
XA START 'xa_trx';
INSERT INTO tm VALUES (1);
INSERT INTO t VALUES (1);
XA END 'xa_trx';
xa rollback 'xa_trx' ;
Warnings:
Warning	1196	Some non-transactional changed tables couldn't be rolled back
XA START 'xa_trx';
INSERT INTO t VALUES (2);
INSERT INTO tm VALUES (2);
XA END 'xa_trx';
xa rollback 'xa_trx' ;
Warnings:
Warning	1196	Some non-transactional changed tables couldn't be rolled back
XA START 'xa_trx';
INSERT INTO tm VALUES (3);
XA END 'xa_trx';
xa rollback 'xa_trx' ;
Warnings:
Warning	1196	Some non-transactional changed tables couldn't be rolled back
include/sync_slave_sql_with_master.inc
include/diff_tables.inc [master:tm, slave:tm]
[connection master]
DROP TABLE t, tm;
include/sync_slave_sql_with_master.inc
include/rpl_end.inc
