#
# WL#8985:  Refactor compressed BLOB code to facilitate partial
# fetch/update
#
set global innodb_compression_level = 0;
create table t1 (f1 int primary key, f2 blob) engine=innodb, row_format = compressed;
insert into t1 values (1, repeat('-', 32000));
start transaction;
update t1 set f2 = repeat('+', 64000) where f1 = 1;
rollback;
drop table t1;
set global innodb_compression_level=default;
