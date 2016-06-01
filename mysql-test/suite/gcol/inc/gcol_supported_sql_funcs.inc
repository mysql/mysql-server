################################################################################
# inc/gcol_supported_sql_funcs.inc                                             #
#                                                                              #
# Purpose:                                                                     #
#  Tests frame for allowed sql functions                                       #
#                                                                              #
#                                                                              #
#------------------------------------------------------------------------------#
# Original Author: Andrey Zhakov                                               #
# Original Date: 2008-08-31                                                    #
# Change Author:                                                               #
# Change Date:                                                                 #
# Change:                                                                      #
################################################################################

--enable_warnings
set sql_warnings = 1;
eval create table t1 ($cols);
show create table t1;
if ($rows)
{
# Allow out-of-range errors
--error 0,1264,1690,3020
  eval insert into t1 values ($values1);
  dec $rows;
}
if ($rows)
{
--error 0,1292,1690,3020
  eval insert into t1 values ($values2);
  dec $rows;
}
if ($rows)
{
--error 0,1690,3020
  eval insert into t1 values ($values3);
  dec $rows;
}
if ($rows)
{
  eval insert into t1 values ($values4);
  dec $rows;
}
--sorted_result
select * from t1;
drop table t1;
set sql_warnings = 0;
