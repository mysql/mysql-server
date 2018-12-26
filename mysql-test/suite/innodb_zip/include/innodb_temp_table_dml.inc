# insert test
insert into t1 values (100, 1.1, 'pune');
insert into t1 values (99, 1.2, 'mumbai');
insert into t1 values (98, 1.3, 'jaipur');
insert into t1 values (97, 1.4, 'delhi');
insert into t1 values (96, 1.5, 'ahmedabad');
select * from t1;
select * from t1 where i = 98;
select * from t1 where i < 100;
#
# add index using alter table table
explain select * from t1 where f > 1.29999;
alter table t1 add index sec_index(f);
explain select * from t1 where f > 1.29999;
select * from t1 where f > 1.29999;
#
explain select * from t1 where i = 100;
alter table t1 add unique index pri_index(i);
explain select * from t1 where i = 100;
select * from t1 where i = 100;
#
# delete test
delete from t1 where i < 97;
select * from t1;
insert into t1 values (96, 1.5, 'kolkata');
select * from t1;
#
# update test
update t1 set f = 1.44 where c = 'delhi';
select * from t1;
#
# truncate table
truncate table t1;
insert into t1 values (100, 1.1, 'pune');
insert into t1 values (99, 1.2, 'mumbai');
insert into t1 values (98, 1.3, 'jaipur');
insert into t1 values (97, 1.4, 'delhi');
insert into t1 values (96, 1.5, 'ahmedabad');
select * from t1;

