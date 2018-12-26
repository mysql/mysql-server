--echo ===> Testing  size=$size
--disable_warnings
--eval CREATE TABLE t1(id INT AUTO_INCREMENT PRIMARY KEY, msg VARCHAR(255)) ENGINE=INNODB ROW_FORMAT=COMPRESSED KEY_BLOCK_SIZE=$size
--enable_warnings

insert into t1 values(1,"I");
insert into t1 values(2,"AM");
insert into t1 values(3,"COMPRESSED");

--source include/shutdown_mysqld.inc

--exec $INNOCHECKSUM $MYSQLD_DATADIR/test/t1.ibd
--exec $INNOCHECKSUM --write=crc32 $MYSQLD_DATADIR/test/t1.ibd
--exec $INNOCHECKSUM --strict-check=crc32 $MYSQLD_DATADIR/test/t1.ibd
--exec $INNOCHECKSUM --write=none $MYSQLD_DATADIR/test/t1.ibd
--exec $INNOCHECKSUM --strict-check=none $MYSQLD_DATADIR/test/t1.ibd

--source include/start_mysqld.inc
select * from t1;
drop table t1;
