################################################################################
# inc/vcol_unsupported_storage_engines.inc                                     #
#                                                                              #
# Purpose:                                                                     #
#  Ensure that defining a virtual column for an unsupported table type         #
#  results in a graceful error.                                                #
#                                                                              #
#------------------------------------------------------------------------------#
# Original Author: Andrey Zhakov                                               #
# Original Date: 2008-09-02                                                    #
# Change Author: Oleksandr Byelkin (Monty program Ab)
# Date: 2009-03-24 
# Change: Syntax changed
################################################################################

--error ER_UNSUPPORTED_ENGINE_FOR_VIRTUAL_COLUMNS
create table t1 (a int, b int as (a+1));
create table t1 (a int not null);
--error ER_UNSUPPORTED_ENGINE_FOR_VIRTUAL_COLUMNS
alter table t1 add column b int as (a+1);
drop table t1;
