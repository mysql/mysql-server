################################################################################
# inc/partition_syntax_2.inc                                                   #
#                                                                              #
# Purpose:                                                                     #
#   Auxiliary script, only useful when sourced by inc/partition_syntax.inc.    #
#                                                                              #
#------------------------------------------------------------------------------#
# Original Author: mleich                                                      #
# Original Date: 2006-05-11                                                    #
# Change Author:                                                               #
# Change Date:                                                                 #
# Change:                                                                      #
################################################################################

if (`SELECT @@session.default_storage_engine NOT IN('ndbcluster')`)
{
   --error ER_UNIQUE_KEY_NEED_ALL_FIELDS_IN_PF
   eval CREATE TABLE t1 (
   $column_list,
   $unique_index
   )
   $partition_scheme;
   --error ER_UNIQUE_KEY_NEED_ALL_FIELDS_IN_PF
   eval CREATE TABLE t1 (
   $column_list,
   PRIMARY KEY(f_int1,f_int2), $unique_index
   )
   $partition_scheme;
}
if (`SELECT @@session.default_storage_engine IN('ndbcluster')`)
{
   eval CREATE TABLE t1 (
   $column_list,
   $unique_index
   )
   $partition_scheme;
   eval $insert_all;
   --source suite/parts/inc/partition_check.inc
   DROP TABLE t1;
   eval CREATE TABLE t1 (
   $column_list,
   PRIMARY KEY(f_int1,f_int2), $unique_index
   )
   $partition_scheme;
   eval $insert_all;
   --source suite/parts/inc/partition_check.inc
   DROP TABLE t1;
}
