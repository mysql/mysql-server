################################################################################
# inc/gcol_keys.inc                                                            #
#                                                                              #
# Purpose:                                                                     #
#  Testing keys, indexes defined upon generated columns.                         #
#                                                                              #
#                                                                              #
#                                                                              #
#------------------------------------------------------------------------------#
# Original Author: Andrey Zhakov                                               #
# Original Date: 2008-09-02                                                    #
# Change Author:                                                               #
# Change Date:                                                                 #
# Change:                                                                      #
################################################################################


--echo #            - UNIQUE KEY
--echo #            - INDEX
--echo #            - FULLTEXT INDEX
--echo #            - SPATIAL INDEX (not supported)
--echo #            - FOREIGN INDEX (partially supported)
--echo #            - CHECK (allowed but not used)

--echo # UNIQUE
--error ER_KEY_BASED_ON_GENERATED_COLUMN
create table t1 (a int, b int generated always as (a*2) virtual unique);
create table t1 (a int, b int generated always as (a*2) stored unique);
show create table t1;
describe t1;
drop table t1;

--error ER_KEY_BASED_ON_GENERATED_COLUMN
create table t1 (a int, b int generated always as (a*2) virtual, unique key (b));
create table t1 (a int, b int generated always as (a*2) stored, unique (b));
show create table t1;
describe t1;
drop table t1;

create table t1 (a int, b int generated always as (a*2) virtual);
--error ER_KEY_BASED_ON_GENERATED_COLUMN
alter table t1 add unique key (b);
drop table t1;
create table t1 (a int, b int generated always as (a*2) stored);
alter table t1 add unique key (b);
drop table t1;

--echo # Testing data manipulation operations involving UNIQUE keys 
--echo # on generated columns can be found in:
--echo #  - gcol_ins_upd.inc
--echo #  - gcol_select.inc

--echo # 
--echo # INDEX
--error ER_KEY_BASED_ON_GENERATED_COLUMN
create table t1 (a int, b int generated always as (a*2) virtual, index (b));
--error ER_KEY_BASED_ON_GENERATED_COLUMN
create table t1 (a int, b int generated always as (a*2) virtual, index (a,b));

create table t1 (a int, b int generated always as (a*2) stored, index (b));
show create table t1;
describe t1;
drop table t1;

create table t1 (a int, b int generated always as (a*2) stored, index (a,b));
show create table t1;
describe t1;
drop table t1;

create table t1 (a int, b int generated always as (a*2) virtual);
--error ER_KEY_BASED_ON_GENERATED_COLUMN
alter table t1 add index (b);
--error ER_KEY_BASED_ON_GENERATED_COLUMN
alter table t1 add index (a,b);
drop table t1;

create table t1 (a int, b int generated always as (a*2) stored);
alter table t1 add index (b);
drop table t1;

create table t1 (a int, b int generated always as (a*2) stored);
alter table t1 add index (a,b);
create table t2 like t1;
drop table t2;
drop table t1;

--echo # Testing data manipulation operations involving INDEX
--echo # on generated columns can be found in:
--echo #  - gcol_select.inc

--echo #
--echo # TODO: FULLTEXT INDEX

--echo # SPATIAL INDEX
if (!$skip_spatial_index_check)
{
  --echo # Error "All parts of a SPATIAL index must be geometrical"
  --error 1687
  create table t1 (a int, b int generated always as (a+1) stored, spatial index (b));
  create table t1 (a int, b int generated always as (a+1) stored);
  --error 1687
  alter table t1 add spatial index (b);
  drop table t1;
}

--echo # FOREIGN KEY

--echo # Rejected FK options.
--error ER_WRONG_FK_OPTION_FOR_GENERATED_COLUMN 
create table t1 (a int, b int generated always as (a+1) stored,
                 foreign key (b) references t2(a) on update set null);
--error ER_WRONG_FK_OPTION_FOR_GENERATED_COLUMN 
create table t1 (a int, b int generated always as (a+1) stored,
                 foreign key (b) references t2(a) on update cascade);
--error ER_WRONG_FK_OPTION_FOR_GENERATED_COLUMN 
create table t1 (a int, b int generated always as (a+1) stored,
                 foreign key (b) references t2(a) on delete set null);

create table t1 (a int, b int generated always as (a+1) stored);
--error ER_WRONG_FK_OPTION_FOR_GENERATED_COLUMN 
alter table t1 add foreign key (b) references t2(a) on update set null;
--error ER_WRONG_FK_OPTION_FOR_GENERATED_COLUMN 
alter table t1 add foreign key (b) references t2(a) on update cascade;
--error ER_WRONG_FK_OPTION_FOR_GENERATED_COLUMN 
alter table t1 add foreign key (b) references t2(a) on delete set null;
drop table t1;

--error ER_KEY_BASED_ON_GENERATED_COLUMN
create table t1 (a int, b int generated always as (a+1) virtual,
                 foreign key (b) references t2(a));

create table t1 (a int, b int generated always as (a+1) virtual);
--error ER_KEY_BASED_ON_GENERATED_COLUMN
alter table t1 add foreign key (b) references t2(a);
drop table t1;

--echo # Allowed FK options.
create table t2 (a int primary key, b char(5));
create table t1 (a int, b int generated always as (a % 10) stored,
                 foreign key (b) references t2(a) on update restrict);
drop table t1;
create table t1 (a int, b int generated always as (a % 10) stored,
                 foreign key (b) references t2(a) on update no action);
drop table t1;
create table t1 (a int, b int generated always as (a % 10) stored,
                 foreign key (b) references t2(a) on delete restrict);
drop table t1;
create table t1 (a int, b int generated always as (a % 10) stored,
                 foreign key (b) references t2(a) on delete cascade);
drop table t1;
create table t1 (a int, b int generated always as (a % 10) stored,
                 foreign key (b) references t2(a) on delete no action);
drop table t1;

--echo 
--echo # Testing data manipulation operations involving FOREIGN KEY 
--echo # on generated columns can be found in:
--echo #  - gcol_ins_upd.inc
--echo #  - gcol_select.inc

--echo #
--echo # TODO: CHECK

