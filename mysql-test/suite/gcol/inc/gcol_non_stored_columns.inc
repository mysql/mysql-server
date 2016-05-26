################################################################################
# inc/gcol_non_stored_columns.inc                                              #
#                                                                              #
# Purpose:                                                                     #
#  Ensure that MySQL behaviour is consistent irrelevant of                     #
#    - the place of a non-stored column among other columns,                   #
#    - the total number of non-stored fields.                                  #
#                                                                              #
#------------------------------------------------------------------------------#
# Original Author: Andrey Zhakov                                               #
# Original Date: 2008-09-04                                                    #
# Change Author:                                                               #
# Change Date:                                                                 #
# Change:                                                                      #
################################################################################

--echo # Case 1. All non-stored columns.
eval create $opt_tmp table t1 (a int generated always as (2+3) virtual);
insert into t1 values (default);
select * from t1;
insert into t1 values (default);
select * from t1;
drop table t1;
--echo # Case 2. CREATE
--echo #  - Column1: "real"
--echo #  - Column 2: virtual non-stored
eval create $opt_tmp table t1 (a int, b int generated always as (-a) virtual);
insert into t1 values (1,default);
select * from t1;
insert into t1 values (2,default);
select * from t1 order by a;
drop table t1;

--echo # Case 3. CREATE
--echo #  - Column1: "real"
--echo #  - Column 2: virtual stored
eval create $opt_tmp table t1 (a int, b int generated always as (-a) stored);
insert into t1 values (1,default);
select * from t1;
insert into t1 values (2,default);
select * from t1 order by a;
drop table t1;

--echo # Case 4. CREATE
--echo #  - Column1: virtual non-stored
--echo #  - Column2: "real"
eval create $opt_tmp table t1 (a int generated always as (-b) virtual, b int);
insert into t1 values (default,1);
select * from t1;
insert into t1 values (default,2);
select * from t1 order by a;
drop table t1;

--echo # Case 5. CREATE
--echo #  - Column1: virtual stored
--echo #  - Column2: "real"
eval create $opt_tmp table t1 (a int generated always as (-b) stored, b int);
insert into t1 values (default,1);
select * from t1;
insert into t1 values (default,2);
select * from t1 order by a;
drop table t1;

--echo # Case 6. CREATE
--echo #  - Column1: "real"
--echo #  - Column2: virtual non-stored
--echo #  - Column3: virtual stored
eval create $opt_tmp table t1 (a int, b int generated always as (-a), c int generated always as (-a) stored);
insert into t1 values (1,default,default);
select * from t1;
insert into t1 values (2,default,default);
select * from t1 order by a;
drop table t1;

--echo # Case 7. ALTER. Modify virtual stored -> virtual non-stored
eval create $opt_tmp table t1 (a int, b int generated always as (a % 2) stored);
--error ER_UNSUPPORTED_ACTION_ON_GENERATED_COLUMN
alter table t1 modify b int generated always as (a % 2) virtual;
show create table t1;
drop table t1;

--echo # Case 8. ALTER. Modify virtual non-stored -> virtual stored
eval create $opt_tmp table t1 (a int, b int generated always as (a % 2) virtual);
--error ER_UNSUPPORTED_ACTION_ON_GENERATED_COLUMN
alter table t1 modify b int generated always as (a % 2) stored;
show create table t1;
drop table t1;

--echo # Case 9. CREATE LIKE
--echo #  - Column1: "real"
--echo #  - Column2: virtual non-stored
--echo #  - Column3: virtual stored
eval create $opt_tmp table t1 (a int, b int generated always as (-a), c int generated always as (-a) stored);
eval create $opt_tmp table t2 like t1;
insert into t2 values (1,default,default);
select * from t2;
insert into t2 values (2,default,default);
select * from t2 order by a;
drop table t2;
drop table t1;

--echo # Case 10. ALTER. Dropping a virtual non-stored column.
--echo #  - Column1: virtual non-stored
--echo #  - Column2: "real"
eval create $opt_tmp table t1 (a int generated always as (-b) virtual, b int, c varchar(5));
insert into t1 values (default,1,'v1');
insert into t1 values (default,2,'v2');
select * from t1 order by b;
alter table t1 drop column a;
select * from t1 order by b;
show create table t1;
drop table t1;

--echo # Case 11. ALTER. Dropping a virtual stored column.
--echo #  - Column1: virtual stored
--echo #  - Column2: "real"
eval create $opt_tmp table t1 (a int generated always as (-b) stored, b int, c char(5));
insert into t1 values (default,1,'v1');
insert into t1 values (default,2,'v2');
select * from t1 order by b;
alter table t1 drop column a;
select * from t1 order by b;
show create table t1;
drop table t1;

--echo # Case 12. ALTER. Adding a new virtual non-stored column.
eval create $opt_tmp table t1 (a int, b datetime);
insert into t1 values (1,'2008-09-04');
insert into t1 values (2,'2008-09-05');
select * from t1 order by a;
alter table t1 add column c int generated always as (dayofyear(b)) virtual after a;
select * from t1 order by a;
show create table t1;
drop table t1;

--echo # Case 13. ALTER. Adding a new virtual stored column.
eval create $opt_tmp table t1 (a int, b datetime);
insert into t1 values (1,'2008-09-04');
insert into t1 values (2,'2008-09-05');
select * from t1 order by a;
alter table t1 add column c int generated always as (dayofyear(b)) stored after a;
select * from t1 order by a;
show create table t1;
drop table t1;

--echo # Case 14. ALTER. Changing the expression of a virtual stored column.
eval create $opt_tmp table t1 (a int, b datetime, c int generated always as (week(b)) stored);
insert into t1 values (1,'2008-09-04',default);
insert into t1 values (2,'2008-09-05',default);
select * from t1 order by a;
alter table t1 change column c c int generated always as (week(b,1)) stored;
select * from t1 order by a;
show create table t1;
drop table t1;

--echo # Case 15. ALTER. Changing the expression of a virtual non-stored column.
eval create $opt_tmp table t1 (a int, b datetime, c int generated always as (week(b)) virtual);
insert into t1 values (1,'2008-09-04',default);
insert into t1 values (2,'2008-09-05',default);
select * from t1 order by a;
alter table t1 change column c c int generated always as (week(b,1)) virtual;
select * from t1 order by a;
show create table t1;
drop table t1;

