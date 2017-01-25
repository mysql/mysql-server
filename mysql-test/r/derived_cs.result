#
# Resolution of alias
#
select QN.a from (select 1 as a) as qn;
ERROR 42S22: Unknown column 'QN.a' in 'field list'
select QN.a from (select 1 as a) as QN;
a
1
select qn.a from (select 1 as a) as QN;
ERROR 42S22: Unknown column 'qn.a' in 'field list'
select 3 from (select 1) as qn, (select 2) as QN;
3
3
with qn as (select 1) select * from QN;
ERROR 42S02: Table 'test.QN' doesn't exist
with QN as (select 1) select * from QN;
1
1
with QN as (select 1) select * from qn;
ERROR 42S02: Table 'test.qn' doesn't exist
with qn as (select 1 as a) select QN.a from qn;
ERROR 42S22: Unknown column 'QN.a' in 'field list'
with qn as (select 1), QN as (select 2) select 3;
3
3
with qn as (select 1 as a), QN as (select 2 as a) select QN.a from QN;
a
2
