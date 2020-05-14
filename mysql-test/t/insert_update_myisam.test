--source include/force_myisam_default.inc
--source include/have_myisam.inc

#
# Bug#9725 - "disapearing query/hang" and "unknown error" with "on duplicate key update"
# INSERT INGORE...UPDATE gives bad error or breaks protocol.
#
create table t1 (a int not null unique) engine=myisam;
insert into t1 values (1),(2);
insert ignore into t1 select 1 on duplicate key update a=2;
select * from t1;
insert ignore into t1 select a from t1 as t2 on duplicate key update a=t1.a+1 ;
select * from t1;
insert into t1 select 1 on duplicate key update a=2;
select * from t1;
--error ER_NON_UNIQ_ERROR
insert into t1 select a from t1 on duplicate key update a=a+1 ;
--error ER_NON_UNIQ_ERROR
insert ignore into t1 select a from t1 on duplicate key update a=t1.a+1 ;
drop table t1;

#
# Bug#10109 - INSERT .. SELECT ... ON DUPLICATE KEY UPDATE fails
# Bogus "Duplicate columns" error message
#

CREATE TABLE t1 (
  a BIGINT(20) NOT NULL DEFAULT 0,
  PRIMARY KEY  (a)
) ENGINE=MyISAM;

INSERT INTO t1 ( a ) SELECT 0 ON DUPLICATE KEY UPDATE a = a + VALUES (a) ;

DROP TABLE t1;

#
# Bug#23233: 0 as LAST_INSERT_ID() after INSERT .. ON DUPLICATE in the
#            NO_AUTO_VALUE_ON_ZERO mode.
#
SET SQL_MODE='NO_AUTO_VALUE_ON_ZERO';
CREATE TABLE `t1` (
  `id` int(11) PRIMARY KEY auto_increment,
  `f1` varchar(10) NOT NULL UNIQUE
)engine=myisam;
INSERT IGNORE INTO t1 (f1) VALUES ("test1")
        ON DUPLICATE KEY UPDATE id=LAST_INSERT_ID(id);
INSERT IGNORE INTO t1 (f1) VALUES ("test1")
        ON DUPLICATE KEY UPDATE id=LAST_INSERT_ID(id);
SELECT LAST_INSERT_ID();
SELECT * FROM t1;
INSERT IGNORE INTO t1 (f1) VALUES ("test2")
        ON DUPLICATE KEY UPDATE id=LAST_INSERT_ID(id);
SELECT * FROM t1;
INSERT IGNORE INTO t1 (f1) VALUES ("test2")
        ON DUPLICATE KEY UPDATE id=LAST_INSERT_ID(id);
SELECT LAST_INSERT_ID();
SELECT * FROM t1;
INSERT IGNORE INTO t1 (f1) VALUES ("test3")
        ON DUPLICATE KEY UPDATE id=LAST_INSERT_ID(id);
SELECT LAST_INSERT_ID();
SELECT * FROM t1;
DROP TABLE t1;
CREATE TABLE `t1` (
  `id` int(11) PRIMARY KEY auto_increment,
  `f1` varchar(10) NOT NULL UNIQUE
);
INSERT IGNORE INTO t1 (f1) VALUES ("test1")
        ON DUPLICATE KEY UPDATE id=LAST_INSERT_ID(id);
SELECT LAST_INSERT_ID();
SELECT * FROM t1;
INSERT IGNORE INTO t1 (f1) VALUES ("test1"),("test4")
        ON DUPLICATE KEY UPDATE id=LAST_INSERT_ID(id);
SELECT LAST_INSERT_ID();
SELECT * FROM t1;
DROP TABLE t1;
