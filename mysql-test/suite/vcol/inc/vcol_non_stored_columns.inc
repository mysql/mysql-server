################################################################################
# inc/vcol_non_stored_columns.inc                                              #
#                                                                              #
# Purpose:                                                                     #
#  Ensure that MySQL behaviour is consistent irrelevant of                     #
#    - the place of a non-stored column among other columns,                   #
#    - the total number of non-stored fields.                                  #
#                                                                              #
#------------------------------------------------------------------------------#
# Original Author: Andrey Zhakov                                               #
# Original Date: 2008-09-04                                                    #
# Change Author: Oleksandr Byelkin (Monty program Ab)
# Date: 2009-03-24 
# Change: Syntax changed
################################################################################

--echo # Case 1. All non-stored columns.
--echo # This scenario is currently impossible due to the fact that virtual columns
--echo # with a constant expression are not allowed.

--echo # Case 2. CREATE
--echo #  - Column1: "real"
--echo #  - Column 2: virtual non-stored
create table t1 (a int, b int as (-a));
insert into t1 values (1,default);
select * from t1;
insert into t1 values (2,default);
select * from t1;
drop table t1;

--echo # Case 3. CREATE
--echo #  - Column1: "real"
--echo #  - Column 2: virtual stored
create table t1 (a int, b int as (-a) persistent);
insert into t1 values (1,default);
select * from t1;
insert into t1 values (2,default);
select * from t1;
drop table t1;

--echo # Case 4. CREATE
--echo #  - Column1: virtual non-stored
--echo #  - Column2: "real"
create table t1 (a int as (-b), b int);
insert into t1 values (default,1);
select * from t1;
insert into t1 values (default,2);
select * from t1;
drop table t1;

--echo # Case 5. CREATE
--echo #  - Column1: virtual stored
--echo #  - Column2: "real"
create table t1 (a int as (-b) persistent, b int);
insert into t1 values (default,1);
select * from t1;
insert into t1 values (default,2);
select * from t1;
drop table t1;

--echo # Case 6. CREATE
--echo #  - Column1: "real"
--echo #  - Column2: virtual non-stored
--echo #  - Column3: virtual stored
create table t1 (a int, b int as (-a), c int as (-a) persistent);
insert into t1 values (1,default,default);
select * from t1;
insert into t1 values (2,default,default);
select * from t1;
drop table t1;

--echo # Case 7. ALTER. Modify virtual stored -> virtual non-stored
create table t1 (a int, b int as (a % 2) persistent);
--error ER_UNSUPPORTED_ACTION_ON_VIRTUAL_COLUMN
alter table t1 modify b int as (a % 2);
show create table t1;
drop table t1;

--echo # Case 8. ALTER. Modify virtual non-stored -> virtual stored
create table t1 (a int, b int as (a % 2));
--error ER_UNSUPPORTED_ACTION_ON_VIRTUAL_COLUMN
alter table t1 modify b int as (a % 2) persistent;
show create table t1;
drop table t1;

--echo # Case 9. CREATE LIKE
--echo #  - Column1: "real"
--echo #  - Column2: virtual non-stored
--echo #  - Column3: virtual stored
create table t1 (a int, b int as (-a), c int as (-a) persistent);
create table t2 like t1;
insert into t2 values (1,default,default);
select * from t2;
insert into t2 values (2,default,default);
select * from t2;
drop table t2;
drop table t1;

--echo # Case 10. ALTER. Dropping a virtual non-stored column.
--echo #  - Column1: virtual non-stored
--echo #  - Column2: "real"
create table t1 (a int as (-b), b int, c varchar(5));
insert into t1 values (default,1,'v1');
insert into t1 values (default,2,'v2');
select * from t1;
alter table t1 drop column a;
select * from t1;
show create table t1;
drop table t1;

--echo # Case 11. ALTER. Dropping a virtual stored column.
--echo #  - Column1: virtual stored
--echo #  - Column2: "real"
create table t1 (a int as (-b) persistent, b int, c char(5));
insert into t1 values (default,1,'v1');
insert into t1 values (default,2,'v2');
select * from t1;
alter table t1 drop column a;
select * from t1;
show create table t1;
drop table t1;

--echo # Case 12. ALTER. Adding a new virtual non-stored column.
create table t1 (a int, b datetime);
insert into t1 values (1,'2008-09-04');
insert into t1 values (2,'2008-09-05');
select * from t1;
alter table t1 add column c int as (dayofyear(b)) after a;
select * from t1;
show create table t1;
drop table t1;

--echo # Case 13. ALTER. Adding a new virtual stored column.
create table t1 (a int, b datetime);
insert into t1 values (1,'2008-09-04');
insert into t1 values (2,'2008-09-05');
select * from t1;
alter table t1 add column c int as (dayofyear(b)) persistent after a;
select * from t1;
show create table t1;
drop table t1;

--echo # Case 14. ALTER. Changing the expression of a virtual stored column.
create table t1 (a int, b datetime, c int as (week(b)) persistent);
insert into t1 values (1,'2008-09-04',default);
insert into t1 values (2,'2008-09-05',default);
select * from t1;
alter table t1 change column c c int as (week(b,1)) persistent;
select * from t1;
show create table t1;
drop table t1;

--echo # Case 15. ALTER. Changing the expression of a virtual non-stored column.
create table t1 (a int, b datetime, c int as (week(b)));
insert into t1 values (1,'2008-09-04',default);
insert into t1 values (2,'2008-09-05',default);
select * from t1;
alter table t1 change column c c int as (week(b,1));
select * from t1;
show create table t1;
drop table t1;

