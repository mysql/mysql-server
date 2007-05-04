################################################################################
# include/partition_syntax_2.inc                                               #
#                                                                              #
# Purpose:                                                                     #
#   Auxiliary script, only useful when sourced by include/partition_syntax.inc.#
#                                                                              #
#------------------------------------------------------------------------------#
# Original Author: ML                                                          #
# Original Date: 2006-05-11                                                    #
# Change Author:                                                               #
# Change Date:                                                                 #
# Change:                                                                      #
################################################################################

if (`SELECT @@session.storage_engine NOT IN('ndbcluster')`)
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
if (`SELECT @@session.storage_engine IN('ndbcluster')`)
{
   eval CREATE TABLE t1 (
   $column_list,
   $unique_index
   )
   $partition_scheme;
   eval $insert_all;
   if ($fixed_bug18735)
   {
   --source suite/parts/inc/partition_check.inc
   # --source include/partition_check.inc
   }
   DROP TABLE t1;
   eval CREATE TABLE t1 (
   $column_list,
   PRIMARY KEY(f_int1,f_int2), $unique_index
   )
   $partition_scheme;
   eval $insert_all;
   if ($fixed_bug18735)
   {
   --source suite/parts/inc/partition_check.inc
   # --source include/partition_check.inc
   }
   DROP TABLE t1;
}
