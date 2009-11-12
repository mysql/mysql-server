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
# Change Author: Oleksandr Byelkin (Monty program Ab)
# Date: 2009-03-24 
# Change: Syntax changed
################################################################################

--echo # Can't define a virtual column on another virtual column
--error ER_VCOL_BASED_ON_VCOL
create table t1 (a int, b int as (a+1), c int as (b+1));
create table t1 (a int, b int as (a+1));
--error ER_VCOL_BASED_ON_VCOL
alter table t1 add column c int as (b+1);
drop table t1;

--echo # Can't rename or drop a column used in the function of a virtual column
create table t1 (a int, b int as (a+1));
--echo # On renaming/dropping a column on which a virtual field is 
--echo # defined the following error is displayed: 
--echo # "Unknown column 'a' in 'virtual column function'"
--error ER_BAD_FIELD_ERROR
alter table t1 drop column a;
--error ER_BAD_FIELD_ERROR
alter table t1 change a c int;
drop table t1;

--echo # Can't rename or drop a virtual column used by the paritition function
create table t1 (a int, b int as (a+1)) partition by hash(b);  
--error ER_BAD_FIELD_ERROR
alter table t1 drop b;                                               
--error ER_BAD_FIELD_ERROR
alter table t1 change b c int as (a+1);

