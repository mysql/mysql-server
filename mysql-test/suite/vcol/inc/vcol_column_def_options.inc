################################################################################
# inc/vcol_column_def_options.inc                                              #
#                                                                              #
# Purpose:                                                                     #
#  Testing different optional parameters specified when defining               #
#  a virtual column.                                                           #
#                                                                              #
#                                                                              #
#------------------------------------------------------------------------------#
# Original Author: Andrey Zhakov                                               #
# Original Date: 2008-09-02                                                    #
# Change Author: Oleksandr Byelkin (Monty program Ab)
# Date: 2009-03-24 
# Change: Syntax changed
################################################################################

--echo #
--echo # Section 1. Wrong column definition options
--echo #            - NOT NULL
--echo #            - NULL
--echo #            - DEFAULT <value>
--echo #            - AUTO_INCREMENT
--echo #            - [PRIMARY] KEY

--echo # NOT NULL
--error ER_PARSE_ERROR
create table t1 (a int, b int as (a+1) not null);
create table t1 (a int);
--error ER_PARSE_ERROR
alter table t1 add column b int as (a+1) not null;
drop table t1;

--echo # NULL  
--error ER_PARSE_ERROR
create table t1 (a int, b int as (a+1) null);
create table t1 (a int);
--error ER_PARSE_ERROR
alter table t1 add column b int as (a+1) null;
drop table t1;

--echo # DEFAULT
--error ER_PARSE_ERROR
create table t1 (a int, b int as (a+1) default 0);
create table t1 (a int);
--error ER_PARSE_ERROR
alter table t1 add column b int as (a+1) default 0;
drop table t1;

--echo # AUTO_INCREMENT
--error ER_PARSE_ERROR
create table t1 (a int, b int as (a+1) AUTO_INCREMENT);
create table t1 (a int);
--error ER_PARSE_ERROR
alter table t1 add column b int as (a+1) AUTO_INCREMENT;
drop table t1;

--echo # [PRIMARY] KEY
--error ER_PARSE_ERROR
create table t1 (a int, b int as (a+1) key);
--error ER_PARSE_ERROR
create table t1 (a int, b int as (a+1) primary key);
create table t1 (a int);
--error ER_PARSE_ERROR
alter table t1 add column b int as (a+1) key;
--error ER_PARSE_ERROR
alter table t1 add column b int as (a+1) primary key;
drop table t1;

--echo # Section 2. Other column definition options
--echo #            - COMMENT
--echo #            - REFERENCES (only syntax testing here)
--echo #            - STORED (only systax testing here)
create table t1 (a int, b int as (a % 2) comment 'my comment');
show create table t1;
describe t1;
drop table t1;
create table t1 (a int, b int as (a % 2));
alter table t1 modify b int as (a % 2) comment 'my comment';
show create table t1;
describe t1;
insert into t1 (a) values (1);
select * from t1;
insert into t1 values (2,default);
select a,b from t1;
create table t2 like t1;
show create table t2;
describe t2;
insert into t2 (a) values (1);
select * from t2;
insert into t2 values (2,default);
select a,b from t2;
drop table t2;
drop table t1;

create table t1 (a int, b int as (a % 2) persistent); 
show create table t1;
describe t1;
insert into t1 (a) values (1);
select * from t1;
insert into t1 values (2,default);
select a,b from t1;
drop table t1;


create table t2 (a int);
create table t1 (a int, b int as (a % 2) persistent references t2(a));
show create table t1;
drop table t1;
create table t1 (a int, b int as (a % 2));
--error ER_PARSE_ERROR
alter table t1 modify b int as (a % 2) persistent references t2(a);
show create table t1;
drop table t1;
