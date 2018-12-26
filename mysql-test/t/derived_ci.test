--source include/have_case_insensitive_file_system.inc

--echo #
--echo # Resolution of alias
--echo #

select QN.a from (select 1 as a) as qn;
select QN.a from (select 1 as a) as QN;
select qn.a from (select 1 as a) as QN;
--error 1066
select 3 from (select 1) as qn, (select 2) as QN;

with qn as (select 1) select * from QN;
with QN as (select 1) select * from QN;
with QN as (select 1) select * from qn;
with qn as (select 1 as a) select QN.a from qn;
--error 1066
with qn as (select 1), QN as (select 2) select 3;
--error 1066
with qn as (select 1 as a), QN as (select 2 as a) select QN.a from QN;
