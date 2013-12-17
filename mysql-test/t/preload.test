# Test for preload_keys of Myisam.The InnoDB storage
# engine doesn't support this preload_keys
# Hence MTR starts mysqld with MyISAM as default

--source include/force_myisam_default.inc
--source include/have_myisam.inc

#
# Testing of PRELOAD
#

--disable_warnings
drop table if exists t1, t2;
--enable_warnings


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
select count(*) from t2;

flush tables; flush status;
show status like "key_read%";

select count(*) from t1 where b = 'test1';
show status like "key_read%";
select count(*) from t1 where b = 'test1';
show status like "key_read%";

flush tables; flush status;
select @@preload_buffer_size;
load index into cache t1;
show status like "key_read%";
select count(*) from t1 where b = 'test1';
show status like "key_read%";

flush tables; flush status;
show status like "key_read%";
set session preload_buffer_size=256*1024;
select @@preload_buffer_size;
load index into cache t1 ignore leaves;
show status like "key_read%";
select count(*) from t1 where b = 'test1';
show status like "key_read%";

flush tables; flush status; 
show status like "key_read%";
set session preload_buffer_size=1*1024;
select @@preload_buffer_size;
load index into cache t1, t2 key (primary,b) ignore leaves;
show status like "key_read%";
select count(*) from t1 where b = 'test1';
select count(*) from t2 where b = 'test1';
show status like "key_read%";

flush tables; flush status;
show status like "key_read%";
load index into cache t3, t2 key (primary,b) ;
show status like "key_read%";

flush tables; flush status;
show status like "key_read%";
load index into cache t3 key (b), t2 key (c) ;
show status like "key_read%";

drop table t1, t2;

# End of 4.1 tests
