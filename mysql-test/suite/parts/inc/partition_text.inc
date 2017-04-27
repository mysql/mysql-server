--echo ---- Partitioning and text data type

--error ER_BLOB_FIELD_IN_PART_FUNC_ERROR
eval create table t1 (a text not null, primary key(a(767))) engine=$engine 
partition by key (a) (
partition pa1 max_rows=20 min_rows=2,
partition pa2 max_rows=30 min_rows=3,
partition pa3 max_rows=30 min_rows=4,
partition pa4 max_rows=40 min_rows=2);

#show create table t1;
#insert into t1 values (repeat('a',1000)), ('b'), (repeat('a',500)), (repeat('b',64));
#select * from t1;
#select * from t1 where a='b';
#update t1 set a='bb' where a='b';
#delete from t1 where a='bb';
#select * from t1;
#drop table t1;

--error ER_BLOB_FIELD_IN_PART_FUNC_ERROR
eval create table t2 (a tinytext not null, primary key(a(255))) charset latin1 engine=$engine 
partition by key (a) partitions 30;

--error ER_BLOB_FIELD_IN_PART_FUNC_ERROR
eval create table t2 (a mediumtext not null, primary key(a(767))) engine=$engine 
partition by key (a) partitions 30;

--error ER_BLOB_FIELD_IN_PART_FUNC_ERROR
eval create table t2 (a longtext not null, primary key(a(767))) engine=$engine 
partition by key (a) partitions 30;

#show create table t2;
#let $count=30;
#let $letter=0;
#--echo $count inserts;
#--disable_query_log
#while ($count)
#{
#eval insert into t2 values (repeat(char(ascii('a')+$letter),$count*$count));
#dec $count;
#inc $letter;
#}
#select count(*) from t2;
#select * from t2;
#drop table t2;
