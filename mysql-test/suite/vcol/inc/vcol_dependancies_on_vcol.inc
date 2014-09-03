################################################################################
# inc/vcol_dependencies_on_vcol.inc                                            #
#                                                                              #
# Purpose:                                                                     #
#  Testing scenarios when columns depend on virtual columns, i.e. such as      #
#   - a virtual column is based on a virtual column                            #
#   - a "real" column on which a virtual one is renamed/dropped                #
#   - a virtual column involved in partitioning is renamed/dropped             #
#                                                                              #
#------------------------------------------------------------------------------#
# Original Author: Andrey Zhakov                                               #
# Original Date: 2008-09-02                                                    #
# Change Author:                                                               #
# Change Date:                                                                 #
# Change:                                                                      #
################################################################################

--echo # Can't define a virtual column on another virtual column
--error ER_VCOL_BASED_ON_VCOL
create table t1 (a int, b int generated always as (a+1) virtual, c int generated always as (b+1) virtual);
create table t1 (a int, b int generated always as (a+1) virtual);
--error ER_VCOL_BASED_ON_VCOL
alter table t1 add column c int generated always as (b+1) virtual;
drop table t1;

--echo # Can't rename or drop a column used in the function of a virtual column
create table t1 (a int, b int generated always as (a+1) virtual);
--echo # On renaming/dropping a column on which a virtual field is 
--echo # defined the following error is displayed: 
--echo # "Unknown column 'a' in 'virtual column function'"
--error 1054
alter table t1 drop column a;
--error 1054
alter table t1 change a c int;
drop table t1;

--echo # Can't rename or drop a virtual column used by the paritition function
create table t1 (a int, b int generated always as (a+1) virtual) partition by hash(b);
--error 1054
alter table t1 drop b;                                               
--error 1054
alter table t1 change b c int generated always as (a+1) virtual;

