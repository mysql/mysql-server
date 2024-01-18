# The purpose of this test is to test that setting autocommit does a
# commit of outstanding transactions and nothing is left pending in
# the transaction cache.

# We need a transactional engine, so let's use InnoDB
CREATE TABLE t1 (id INT) ENGINE = InnoDB;

# Testing SET AUTOCOMMIT
SET BINLOG_FORMAT = STATEMENT;

let $cleanup = COMMIT;

let $prepare = SET AUTOCOMMIT = 0;
let $statement = SET AUTOCOMMIT = 1;
source common/binlog/implicit.test;

let $prepare = SET AUTOCOMMIT = 1;
let $statement = SET AUTOCOMMIT = 1;
source common/binlog/implicit.test;

let $prepare = SET AUTOCOMMIT = 0;
let $statement = SET AUTOCOMMIT = 0;
source common/binlog/implicit.test;

let $prepare = SET AUTOCOMMIT = 1;
let $statement = SET AUTOCOMMIT = 0;
source common/binlog/implicit.test;

SET BINLOG_FORMAT = 'ROW';
let $prepare = SET AUTOCOMMIT = 0;
let $statement = SET AUTOCOMMIT = 1;
source common/binlog/implicit.test;

let $prepare = SET AUTOCOMMIT = 1;
let $statement = SET AUTOCOMMIT = 1;
source common/binlog/implicit.test;

let $prepare = SET AUTOCOMMIT = 0;
let $statement = SET AUTOCOMMIT = 0;
source common/binlog/implicit.test;

let $prepare = SET AUTOCOMMIT = 1;
let $statement = SET AUTOCOMMIT = 0;
source common/binlog/implicit.test;

RESET BINARY LOGS AND GTIDS;
SET AUTOCOMMIT = 0;
INSERT INTO t1 VALUES (1);
source include/rpl/deprecated/show_binlog_events.inc;
LOCK TABLES t1 WRITE;
source include/rpl/deprecated/show_binlog_events.inc;
INSERT INTO t1 VALUES (2);
source include/rpl/deprecated/show_binlog_events.inc;
UNLOCK TABLES;
source include/rpl/deprecated/show_binlog_events.inc;
COMMIT;
source include/rpl/deprecated/show_binlog_events.inc;

# Cleaning up
DROP TABLE t1;
