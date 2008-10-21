################################################################################
# t/part_supported_sql_funcs_delete.inc                                        #
#                                                                              #
# Purpose:                                                                     #
#  Delete access of the tests frame for allowed sql functions                  #
#                                                                              #
#                                                                              #
#------------------------------------------------------------------------------#
# Original Author: HH                                                          #
# Original Date: 2006-11-22                                                    #
# Change Author:                                                               #
# Change Date:                                                                 #
# Change:                                                                      #
################################################################################
--echo -------------------------------------------------------------------------
--echo ---  Delete rows and partitions of tables with $sqlfunc
--echo -------------------------------------------------------------------------

eval delete from $t1 where col1=$val2;
eval delete from $t2 where col1=$val2;
eval delete from $t3 where col1=$val2;
eval delete from $t4 where col1=$val2;
eval delete from $t5 where col1=$val2;
eval delete from $t6 where col1=$val2;

eval select * from $t1 order by col1;
eval select * from $t2 order by col1;
eval select * from $t3 order by col1;
eval select * from $t4 order by colint;
eval select * from $t5 order by colint;

eval insert into $t1 values ($val2);
eval insert into $t2 values ($val2);
eval insert into $t3 values ($val2);
eval insert into $t4 values (60,$val2);
eval insert into $t5 values (60,$val2);
eval insert into $t6 values (60,$val2);

eval select * from $t1 order by col1;
eval select * from $t2 order by col1;
eval select * from $t3 order by col1;
eval select * from $t4 order by colint;
eval select * from $t5 order by colint;
eval select * from $t6 order by colint;

if (!$drop_partition_not_supported)
{
eval alter table $t1 drop partition p0; 
eval alter table $t2 drop partition p0; 
eval alter table $t4 drop partition p0; 
eval alter table $t5 drop partition p0; 
eval alter table $t6 drop partition p0;  

eval select * from $t1 order by col1;
eval select * from $t2 order by col1;
eval select * from $t3 order by col1;
eval select * from $t4 order by colint;
eval select * from $t5 order by colint;
eval select * from $t6 order by colint;
}
