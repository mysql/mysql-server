#### suite/funcs_1/datadict/basics_mixed2.inc
#
# Auxiliary script to be sourced by suite/funcs_1/datadict/is_basics_mixed1.inc
#
# contains all tables from INFORMATION_SCHEMA
#
# Usage example(snip of script):
#    let $dd_part1= CREATE TABLE information_schema.;
#    let $dd_part2= ( c1 INT );
#    --source suite/funcs_1/datadict/basics_mixed2.inc
#
# We expect to get
# ERROR 42000: Access denied for user 'root'@'localhost'
#              to database 'information_schema'
# for every statement.
#
# Author:
# 2008-01-23 mleich WL#4203 Reorganize and fix the data dictionary tests of
#                           testsuite funcs_1
#                   Create this script based on older scripts and new code.
#

--error ER_DBACCESS_DENIED_ERROR
eval $dd_part1 schemata                              $dd_part2;
--error ER_DBACCESS_DENIED_ERROR
eval $dd_part1 tables                                $dd_part2;
--error ER_DBACCESS_DENIED_ERROR
eval $dd_part1 columns                               $dd_part2;
--error ER_DBACCESS_DENIED_ERROR
eval $dd_part1 character_sets                        $dd_part2;
--error ER_DBACCESS_DENIED_ERROR
eval $dd_part1 collations                            $dd_part2;
--error ER_DBACCESS_DENIED_ERROR
eval $dd_part1 collation_character_set_applicability $dd_part2;
--error ER_DBACCESS_DENIED_ERROR
eval $dd_part1 routines                              $dd_part2;
--error ER_DBACCESS_DENIED_ERROR
eval $dd_part1 statistics                            $dd_part2;
--error ER_DBACCESS_DENIED_ERROR
eval $dd_part1 views                                 $dd_part2;
--error ER_DBACCESS_DENIED_ERROR
eval $dd_part1 user_privileges                       $dd_part2;
--error ER_DBACCESS_DENIED_ERROR
eval $dd_part1 schema_privileges                     $dd_part2;
--error ER_DBACCESS_DENIED_ERROR
eval $dd_part1 table_privileges                      $dd_part2;
--error ER_DBACCESS_DENIED_ERROR
eval $dd_part1 column_privileges                     $dd_part2;
--error ER_DBACCESS_DENIED_ERROR
eval $dd_part1 table_constraints                     $dd_part2;
--error ER_DBACCESS_DENIED_ERROR
eval $dd_part1 key_column_usage                      $dd_part2;
--error ER_DBACCESS_DENIED_ERROR
eval $dd_part1 triggers                              $dd_part2;

