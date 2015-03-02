################################################################################
# inc/gcol_unsupported_storage_engines.inc                                     #
#                                                                              #
# Purpose:                                                                     #
#  Ensure that defining a generated column for an unsupported table type       #
#  results in a graceful error.                                                #
#                                                                              #
#------------------------------------------------------------------------------#
# Original Author: Andrey Zhakov                                               #
# Original Date: 2008-09-02                                                    #
# Change Author:                                                               #
# Change Date:                                                                 #
# Change:                                                                      #
################################################################################

--error ER_UNSUPPORTED_ACTION_ON_GENERATED_COLUMN
create table t1 (a int, b int generated always as (a+1) virtual);
create table t1 (a int);
--error ER_UNSUPPORTED_ACTION_ON_GENERATED_COLUMN
alter table t1 add column b int generated always as (a+1) virtual;
drop table t1;
