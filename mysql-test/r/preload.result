drop table if exists t1, t2;
create table t1 (
a int not null auto_increment,
b char(16) not null,
primary key (a),
key (b)
);
create table t2(
a int not null auto_increment,
b char(16) not null,
primary key (a),
key (b)
);
insert into t1(b) values 
('test0'),
('test1'),
('test2'),
('test3'),
('test4'),
('test5'),
('test6'),
('test7');
insert into t2(b) select b from t1;
insert into t1(b) select b from t2;
insert into t2(b) select b from t1;
insert into t1(b) select b from t2;
insert into t2(b) select b from t1;
insert into t1(b) select b from t2;
insert into t2(b) select b from t1;
insert into t1(b) select b from t2;
insert into t2(b) select b from t1;
insert into t1(b) select b from t2;
insert into t2(b) select b from t1;
insert into t1(b) select b from t2;
insert into t2(b) select b from t1;
insert into t1(b) select b from t2;
insert into t2(b) select b from t1;
insert into t1(b) select b from t2;
insert into t2(b) select b from t1;
insert into t1(b) select b from t2;
select count(*) from t1;
count(*)
33448
select count(*) from t2;
count(*)
20672
flush tables;
flush status;
show status like "key_read%";
Variable_name	Value
Key_read_requests	0
Key_reads	0
select count(*) from t1 where b = 'test1';
count(*)
4181
show status like "key_read%";
Variable_name	Value
Key_read_requests	294
Key_reads	60
select count(*) from t1 where b = 'test1';
count(*)
4181
show status like "key_read%";
Variable_name	Value
Key_read_requests	588
Key_reads	60
flush tables;
flush status;
select @@preload_buffer_size;
@@preload_buffer_size
32768
load index into cache t1;
Table	Op	Msg_type	Msg_text
test.t1	preload_keys	status	OK
show status like "key_read%";
Variable_name	Value
Key_read_requests	774
Key_reads	774
select count(*) from t1 where b = 'test1';
count(*)
4181
show status like "key_read%";
Variable_name	Value
Key_read_requests	1068
Key_reads	774
flush tables;
flush status;
show status like "key_read%";
Variable_name	Value
Key_read_requests	0
Key_reads	0
set session preload_buffer_size=256*1024;
select @@preload_buffer_size;
@@preload_buffer_size
262144
load index into cache t1 ignore leaves;
Table	Op	Msg_type	Msg_text
test.t1	preload_keys	status	OK
show status like "key_read%";
Variable_name	Value
Key_read_requests	17
Key_reads	17
select count(*) from t1 where b = 'test1';
count(*)
4181
show status like "key_read%";
Variable_name	Value
Key_read_requests	311
Key_reads	75
flush tables;
flush status;
show status like "key_read%";
Variable_name	Value
Key_read_requests	0
Key_reads	0
set session preload_buffer_size=1*1024;
select @@preload_buffer_size;
@@preload_buffer_size
1024
load index into cache t1, t2 key (primary,b) ignore leaves;
Table	Op	Msg_type	Msg_text
test.t1	preload_keys	status	OK
test.t2	preload_keys	status	OK
show status like "key_read%";
Variable_name	Value
Key_read_requests	785
Key_reads	785
select count(*) from t1 where b = 'test1';
count(*)
4181
select count(*) from t2 where b = 'test1';
count(*)
2584
show status like "key_read%";
Variable_name	Value
Key_read_requests	1266
Key_reads	821
flush tables;
flush status;
show status like "key_read%";
Variable_name	Value
Key_read_requests	0
Key_reads	0
load index into cache t3, t2 key (primary,b) ;
Table	Op	Msg_type	Msg_text
test.t3	preload_keys	Error	Table 'test.t3' doesn't exist
test.t3	preload_keys	status	Operation failed
test.t2	preload_keys	status	OK
show status like "key_read%";
Variable_name	Value
Key_read_requests	478
Key_reads	478
flush tables;
flush status;
show status like "key_read%";
Variable_name	Value
Key_read_requests	0
Key_reads	0
load index into cache t3 key (b), t2 key (c) ;
Table	Op	Msg_type	Msg_text
test.t3	preload_keys	Error	Table 'test.t3' doesn't exist
test.t3	preload_keys	status	Operation failed
test.t2	preload_keys	Error	Key 'c' doesn't exist in table 't2'
test.t2	preload_keys	status	Operation failed
show status like "key_read%";
Variable_name	Value
Key_read_requests	0
Key_reads	0
drop table t1, t2;
