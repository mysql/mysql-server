--echo # ----------------------------------------------------------------------
--echo # Test of aggregate function SUM, AVG: in constrast to strings, we do not
--echo # auto-convert to numeric (double) type:
--echo # ----------------------------------------------------------------------
create table t(j json, c varchar(20));
show create table t;
insert into t values (cast('[1,2,3]' as json), '[a,b,c]');
insert into t values (cast(7 as json), '7'),  (cast(2 as json), '2');
select sum(j), sum(cast(j as unsigned)), sum(c) from t;
select avg(j), avg(cast(j as unsigned)), avg(c) from t;

--echo # ----------------------------------------------------------------------
--echo # Test of aggregate function GROUP_CONCAT
--echo # ----------------------------------------------------------------------
--echo # TODO: innodb and ndb results differ
select group_concat(j order by j), group_concat(distinct j order by j), group_concat(c order by c) from t;
insert into t values (NULL, NULL);
--echo # TODO: ndb results are inconsistent
select group_concat(j order by j), group_concat(distinct j order by j), group_concat(c order by c) from t;
drop table t;

