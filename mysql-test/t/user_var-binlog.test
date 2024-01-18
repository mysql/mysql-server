# Requires statement logging
-- source include/rpl/force_binlog_format_statement.inc
# TODO: Create row based version once $MYSQL_BINLOG has new RB version

# Check that user variables are binlogged correctly (BUG#3875)
create table t1 (a varchar(50));
reset binary logs and gtids;
SET TIMESTAMP=10000;
SET @`a b`='hello';
INSERT INTO t1 VALUES(@`a b`);
set @var1= "';aaa";
SET @var2=char(ascii('a'));
insert into t1 values (@var1),(@var2);
source include/rpl/deprecated/show_binlog_events.inc;

# more important than SHOW BINLOG EVENTS, mysqlbinlog (where we
# absolutely need variables names to be quoted and strings to be
# escaped).
let $MYSQLD_DATADIR= `select @@datadir`;
flush logs;
--let $mysqlbinlog_parameters= --short-form $MYSQLD_DATADIR/binlog.000001
--source include/rpl/mysqlbinlog.inc
drop table t1;

# End of 4.1 tests
--source include/rpl/restore_default_binlog_format.inc
