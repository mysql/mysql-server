create table bureau (
 id int(11) default null,
 c2 int(11) default null,
 c3 varchar(20),
 c4 varchar(20),
 c5 int(11) default null,
 c6 int(11) default null,
 c7 varchar(20),
 c8 varchar(20),
 c9 varchar(20),
 c10 int(11) default null,
 c11 double default null,
 c12 varchar(20),
 c13 varchar(20),
 c14 double default null,
 c15 varchar(20),
 c16 int(11) default null,
 amt_annuity varchar(20),
 key bureau (id,c2),
 key bureau_i2 (c2)
 ) engine=innodb default charset=utf8mb4 collate=utf8mb4_0900_ai_ci;

set session sql_mode='';
load data infile '../../std_data/inconsistent_scan.csv' into table bureau columns terminated by "," ignore 1 lines;

set big_tables=0;
create table t1 as
select id, c2, count(*) over w cnt,
       c3,
       c4, c5,
       c6, c7, c8,
       c9, c10, c11,
       c12, c13,
       c14, c15, c16
from bureau window w as (partition by id);

set big_tables=1;
create table t2 as
select id, c2, count(*) over w cnt,
       c3,
       c4, c5,
       c6, c7, c8,
       c9, c10, c11,
       c12, c13,
       c14, c15, c16
from bureau window w as (partition by id);

create table t3 as
select * from t1 union select * from t2;

select * from t1;
select * from t2;
select * from t3;

select count(*) from t1;
select count(*) from t2;
select count(*) from t3;

checksum table t1, t2, t3;

drop table bureau;
drop table t1, t2, t3;
