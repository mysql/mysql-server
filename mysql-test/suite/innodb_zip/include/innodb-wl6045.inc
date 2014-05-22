#
# Copyright (c) 2012, 2013, Oracle and/or its affiliates. All rights reserved.
#

--echo ===> Testing  size=$size
SET GLOBAL innodb_file_per_table=on;
SET GLOBAL innodb_file_format='barracuda';
--eval CREATE TABLE t1(id INT AUTO_INCREMENT PRIMARY KEY, msg VARCHAR(255)) ENGINE=INNODB ROW_FORMAT=COMPRESSED KEY_BLOCK_SIZE=$size

insert into t1 values(1,"I");
insert into t1 values(2,"AM");
insert into t1 values(3,"COMPRESSED");

--echo # Shutdown the Server
--exec echo "wait" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--shutdown_server
--source include/wait_until_disconnected.inc

--exec $INNOCHECKSUM $MYSQLD_DATADIR/test/t1.ibd
--exec $INNOCHECKSUM --write=crc32 $MYSQLD_DATADIR/test/t1.ibd
--exec $INNOCHECKSUM --strict-check=crc32 $MYSQLD_DATADIR/test/t1.ibd
--exec $INNOCHECKSUM --write=none $MYSQLD_DATADIR/test/t1.ibd
--exec $INNOCHECKSUM --strict-check=none $MYSQLD_DATADIR/test/t1.ibd

--echo # Restart the DB server
--exec echo "restart" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--enable_reconnect
--source include/wait_until_connected_again.inc
--disable_reconnect
--disable_result_log
select * from t1;
drop table t1;
