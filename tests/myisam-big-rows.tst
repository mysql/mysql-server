#
# Test rows with length above > 16M
# Note that for this to work, you should start mysqld with
# --max_allowed_packet=32M
#

drop table if exists t1;
create table t1 (a tinyint not null auto_increment, b longblob not null, primary key (a)) checksum=1;

insert into t1 (b) values(repeat(char(65),10));
insert into t1 (b) values(repeat(char(66),10));
insert into t1 (b) values(repeat(char(67),10));
update t1 set b=repeat(char(68),16777216) where a=1;
check table t1;
update t1 set b=repeat(char(69),16777000) where a=2;
update t1 set b=repeat(char(70),167) where a=3;
update t1 set b=repeat(char(71),16778000) where a=1;
update t1 set b=repeat(char(72),16778000) where a=3;
select a,length(b) from t1;
set @a=1;
insert into t1 (b) values (repeat(char(73+@a),16777200+@a));
set @a=@a+1;
insert into t1 (b) values (repeat(char(73+@a),16777200+@a));
set @a=@a+1;
insert into t1 (b) values (repeat(char(73+@a),16777200+@a));
set @a=@a+1;
insert into t1 (b) values (repeat(char(73+@a),16777200+@a));
set @a=@a+1;
insert into t1 (b) values (repeat(char(73+@a),16777200+@a));
set @a=@a+1;
insert into t1 (b) values (repeat(char(73+@a),16777200+@a));
set @a=@a+1;
insert into t1 (b) values (repeat(char(73+@a),16777200+@a));
set @a=@a+1;
insert into t1 (b) values (repeat(char(73+@a),16777200+@a));
set @a=@a+1;
insert into t1 (b) values (repeat(char(73+@a),16777200+@a));
set @a=@a+1;
insert into t1 (b) values (repeat(char(73+@a),16777200+@a));
set @a=@a+1;
insert into t1 (b) values (repeat(char(73+@a),16777200+@a));
set @a=@a+1;
insert into t1 (b) values (repeat(char(73+@a),16777200+@a));
set @a=@a+1;
insert into t1 (b) values (repeat(char(73+@a),16777200+@a));
set @a=@a+1;
insert into t1 (b) values (repeat(char(73+@a),16777200+@a));
set @a=@a+1;
insert into t1 (b) values (repeat(char(73+@a),16777200+@a));
set @a=@a+1;
insert into t1 (b) values (repeat(char(73+@a),16777200+@a));
set @a=@a+1;
insert into t1 (b) values (repeat(char(73+@a),16777200+@a));
set @a=@a+1;
insert into t1 (b) values (repeat(char(73+@a),16777200+@a));
update t1 set b=('A') where a=5;
delete from t1 where a=7;
set @a=@a+1;
insert into t1 (b) values (repeat(char(73+@a),16777200+@a));
update t1 set b=repeat(char(73+@a+1),17000000+@a) where a=last_insert_id();

select a,mid(b,1,5),length(b) from t1;
check table t1;
repair table t1;
check table t1;
select a from table where b<>repeat(mid(b,1,1),length(b));
delete from t1 where (a & 1);
select a from table where b<>repeat(mid(b,1,1),length(b));
check table t1;
repair table t1;
check table t1;
drop table t1;
