--source include/have_case_sensitive_file_system.inc

--echo #
--echo # Resolution of alias
--echo #

--error 1054
select QN.a from (select 1 as a) as qn;
select QN.a from (select 1 as a) as QN;
--error 1054
select qn.a from (select 1 as a) as QN;
select 3 from (select 1) as qn, (select 2) as QN;

--error 1146
with qn as (select 1) select * from QN;
with QN as (select 1) select * from QN;
--error 1146
with QN as (select 1) select * from qn;
--error 1054
with qn as (select 1 as a) select QN.a from qn;
with qn as (select 1), QN as (select 2) select 3;
with qn as (select 1 as a), QN as (select 2 as a) select QN.a from QN;
