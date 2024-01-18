
SET BINLOG_FORMAT=MIXED;

RESET BINARY LOGS AND GTIDS;

CREATE TABLE t1 (a INT PRIMARY KEY, b INT) ENGINE=INNODB;
INSERT INTO t1 VALUES (1,1),(2,2),(3,3),(4,4),(5,5),(6,6);

SET SESSION TRANSACTION ISOLATION LEVEL REPEATABLE READ;
BEGIN;
# Should be logged as statement
UPDATE t1 SET b = 2*a WHERE a > 1;
COMMIT;

SET SESSION TRANSACTION ISOLATION LEVEL READ COMMITTED;
BEGIN;
# Should be logged as rows
UPDATE t1 SET b = a * a WHERE a > 3;
COMMIT;

# Check that errors are generated when trying to use READ COMMITTED
# transaction isolation level in STATEMENT binlog mode.

SET BINLOG_FORMAT=STATEMENT;

SET SESSION TRANSACTION ISOLATION LEVEL READ UNCOMMITTED;
BEGIN;
error ER_BINLOG_STMT_MODE_AND_ROW_ENGINE;
UPDATE t1 SET b = 1*a WHERE a > 1;
COMMIT;

SET SESSION TRANSACTION ISOLATION LEVEL READ COMMITTED;
BEGIN;
error ER_BINLOG_STMT_MODE_AND_ROW_ENGINE;
UPDATE t1 SET b = 2*a WHERE a > 2;
COMMIT;

SET SESSION TRANSACTION ISOLATION LEVEL REPEATABLE READ;
BEGIN;
UPDATE t1 SET b = 3*a WHERE a > 3;
COMMIT;

SET SESSION TRANSACTION ISOLATION LEVEL SERIALIZABLE;
BEGIN;
UPDATE t1 SET b = 4*a WHERE a > 4;
COMMIT;

SET BINLOG_FORMAT=MIXED;

SET SESSION TRANSACTION ISOLATION LEVEL READ UNCOMMITTED;
BEGIN;
UPDATE t1 SET b = 1*a WHERE a > 1;
COMMIT;

SET SESSION TRANSACTION ISOLATION LEVEL READ COMMITTED;
BEGIN;
UPDATE t1 SET b = 2*a WHERE a > 2;
COMMIT;

SET SESSION TRANSACTION ISOLATION LEVEL REPEATABLE READ;
BEGIN;
UPDATE t1 SET b = 3*a WHERE a > 3;
COMMIT;

SET SESSION TRANSACTION ISOLATION LEVEL SERIALIZABLE;
BEGIN;
UPDATE t1 SET b = 4*a WHERE a > 4;
COMMIT;

SET BINLOG_FORMAT='ROW';

SET SESSION TRANSACTION ISOLATION LEVEL READ UNCOMMITTED;
BEGIN;
UPDATE t1 SET b = 1*a WHERE a > 1;
COMMIT;

SET SESSION TRANSACTION ISOLATION LEVEL READ COMMITTED;
BEGIN;
UPDATE t1 SET b = 2*a WHERE a > 2;
COMMIT;

SET SESSION TRANSACTION ISOLATION LEVEL REPEATABLE READ;
BEGIN;
UPDATE t1 SET b = 3*a WHERE a > 3;
COMMIT;

SET SESSION TRANSACTION ISOLATION LEVEL SERIALIZABLE;
BEGIN;
UPDATE t1 SET b = 4*a WHERE a > 4;
COMMIT;

source include/rpl/deprecated/show_binlog_events.inc;

DROP TABLE t1;


#
# Let us test binlog_cache_use and binlog_cache_disk_use status vars.
# Actually this test has nothing to do with innodb per se, it just requires
# transactional table.
#
flush status;
show status like "binlog_cache_use";
show status like "binlog_cache_disk_use";

create table t1 (a int) engine=innodb;

# Now we are going to create transaction which is long enough so its
# transaction binlog will be flushed to disk...
let $1=600;
disable_query_log;
begin;
while ($1)
{
 eval insert into t1 values( $1 );
 dec $1;
}
commit;
enable_query_log;
show status like "binlog_cache_use";
show status like "binlog_cache_disk_use";

# Transaction which should not be flushed to disk and so should not
# increase binlog_cache_disk_use.
begin;
delete from t1;
commit;
show status like "binlog_cache_use";
show status like "binlog_cache_disk_use";
drop table t1;
