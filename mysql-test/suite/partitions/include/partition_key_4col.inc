--disable_query_log
# DATA DIRECTORY
eval SET @data_dir = 'DATA DIRECTORY =
''/tmp''';
let $data_directory = `select @data_dir`;

#INDEX DIRECTORY
eval SET @indx_dir = 'INDEX DIRECTORY =
''/tmp''';
let $index_directory = `select @indx_dir`;
--enable_query_log

eval create table t1 (a date not null, b varchar(50) not null, c varchar(50) not null, d enum('m', 'w'), primary key(a,b,c,d)) engine=$engine 
partition by key (a,b,c,d) (
partition pa1 $data_directory $index_directory max_rows=20 min_rows=2,
partition pa2 $data_directory $index_directory max_rows=30 min_rows=3,
partition pa3 $data_directory $index_directory max_rows=30 min_rows=4,
partition pa4 $data_directory $index_directory max_rows=40 min_rows=2);
show create table t1;
insert into t1 values 
('1975-01-01', 'abcde', 'abcde','m'), 
('1983-12-31', 'cdef', 'srtbvsr', 'w'), 
('1980-10-14', 'fgbbd', 'dtzndtz', 'w'),
('2000-06-15', 'jukg','zikhuk','m');
select * from t1;
select * from t1 where a<19851231;
drop table t1;

