#
# WL#8960  Partial update of LOB.
# Check backward compatibility.
#
set global innodb_compression_level = 0;
create table t1 (f1 int primary key, f2 longblob)
row_format=compressed, engine=innodb;
insert into t1 values (1, repeat('+', 40000));
set debug = '+d,lob_insert_single_zstream';
insert into t1 values (2, repeat('~', 40000));
set debug = '-d,lob_insert_single_zstream';
select f1, right(f2, 40) from t1;
f1	right(f2, 40)
1	++++++++++++++++++++++++++++++++++++++++
2	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
drop table t1;
set global innodb_compression_level = default;
