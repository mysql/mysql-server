#################################################################
#                                                               #
# Bug # 27898384: InnoDB: Assertion failure: lock0lock.cc:4226: #
#  !(lock2->trx == trx && lock_get_mode(lock2) == lock_mode)    #
#                                                               #
#################################################################
##############
# SCENARIO 1 #
##############
CREATE TABLE t1 (
id INT PRIMARY KEY,
val INT
) Engine=InnoDB;
INSERT INTO t1 (id, val) VALUES (100,100),(200,200);
BEGIN;
DELETE FROM t1 WHERE id=123;
SET TRANSACTION ISOLATION LEVEL READ COMMITTED;
BEGIN;
SET DEBUG_SYNC = 'lock_wait_will_wait SIGNAL con1_will_wait';
INSERT INTO t1 (id, val) VALUES (123,123);;
SET DEBUG_SYNC = 'now WAIT_FOR con1_will_wait';
ROLLBACK;
SELECT * FROM t1 WHERE 100<=id AND id<=200 AND val=100 FOR UPDATE;
id	val
100	100
ROLLBACK;
DROP TABLE t1;
SET DEBUG_SYNC = 'RESET';
##############
# SCENARIO 2 #
##############
CREATE TABLE t1 (
`a` int(11) NOT NULL auto_increment,
`b` int(11) default NULL,
`c` int default 7,
PRIMARY KEY  (`a`),
UNIQUE KEY `b` (`b`)
) ENGINE=innodb;
Warnings:
Warning	1681	Integer display width is deprecated and will be removed in a future release.
Warning	1681	Integer display width is deprecated and will be removed in a future release.
INSERT INTO t1 (a,b,c) VALUES (1,1,1), (2,2,2), (3,3,3), (4,4,4);
SET SESSION TRANSACTION ISOLATION LEVEL READ COMMITTED;
BEGIN;
UPDATE t1 SET b=2 WHERE a=1;
ERROR 23000: Duplicate entry '2' for key 't1.b'
SELECT a FROM t1 WHERE 2<=b AND b<=3 AND c=1 FOR SHARE;
a
BEGIN;
SET DEBUG_SYNC = 'lock_wait_will_wait SIGNAL con2_will_wait';
DELETE FROM t1 WHERE b=2;
SET DEBUG_SYNC = 'now WAIT_FOR con2_will_wait';
COMMIT;
COMMIT;
DROP TABLE t1;
SET DEBUG_SYNC = 'RESET';
########################
#                      #
# End of Bug #27898384 #
#                      #
########################
# Bug #31046834    ASSERTION FAILURE: TRX0TRX.CC:2663:TRX_ALLOWED_TWO_LATCHES THREAD 14024410520550
# Bug #31047326    ASSERTION FAILURE: TRX0TRX.CC:2663:TRX_ALLOWED_2_LATCHES THREAD 139840853837568
CREATE TABLE t1 (
id INT PRIMARY KEY,
val INT
) Engine=InnoDB;
INSERT INTO t1 (id, val) VALUES (1,1);
SET TRANSACTION ISOLATION LEVEL READ COMMITTED;
BEGIN;
SET DEBUG_SYNC = 'after_lock_clust_rec_read_check_and_lock SIGNAL con1_created_lock WAIT_FOR con2_will_wait';
SELECT * FROM t1 WHERE val=13 FOR UPDATE;
SET DEBUG_SYNC = 'now WAIT_FOR con1_created_lock';
BEGIN;
SET DEBUG_SYNC = 'lock_wait_will_wait SIGNAL con2_will_wait';
SELECT * FROM t1 WHERE id=1 FOR UPDATE;
id	val
COMMIT;
id	val
1	1
COMMIT;
DROP TABLE t1;
SET DEBUG_SYNC = 'RESET';
