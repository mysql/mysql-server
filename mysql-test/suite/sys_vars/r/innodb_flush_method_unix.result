select @@innodb_flush_method;
@@innodb_flush_method
fsync
call mtr.add_suppression("Failed to set O_DIRECT");
CALL mtr.add_suppression("\\[ERROR\\] .*MY-\\d+.* Failed to create check sector file*");
call mtr.add_suppression("Failed to set DIRECTIO_ON");
# restart: --innodb-flush-method=5
select @@innodb_flush_method;
@@innodb_flush_method
O_DIRECT_NO_FSYNC
create table t(a serial) engine=innodb;
# restart
drop table t;
