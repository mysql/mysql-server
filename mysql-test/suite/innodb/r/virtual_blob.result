#
# Bug#22046353 ALTER: ASSERT PAGE_SIZE.EQUALS_TO(SPACE_PAGE_SIZE),
# BTR_COPY_BLOB_PREFIX
#
CREATE TABLE t1
( f1 int primary key, f2 blob,
f3 blob generated always as (f2))
row_format=compressed, engine=innodb;
insert into t1 (f1, f2) values (1, repeat('&', 50000));
alter table t1 add index i1 (f3(200)) ;
alter table t1 row_format=compact;
drop table t1;
