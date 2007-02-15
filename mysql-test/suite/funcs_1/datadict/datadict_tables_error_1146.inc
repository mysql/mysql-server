#### suite/funcs_1/datadict/datadict_tables.inc

# contains all tables from INFORMATION_SCHEMA

# usage:

# --source suite/funcs_1/datadict/datadict_tables_err_<no>.inc
#
# up to a change of "mysqltest", which makes lines like "eval --error $err_no"
# possible we will have some different files with the same content except the
# error numbers.

--source include/show_msg.inc

let $message= known error 1146:;
--source include/show_msg.inc

--error 1146
eval $dd_part1 schemata                              $dd_part2;
--error 1146
eval $dd_part1 tables                                $dd_part2;
--error 1146
eval $dd_part1 columns                               $dd_part2;
--error 1146
eval $dd_part1 character_sets                        $dd_part2;
--error 1146
eval $dd_part1 collations                            $dd_part2;
--error 1146
eval $dd_part1 collation_character_set_applicability $dd_part2;
--error 1146
eval $dd_part1 routines                              $dd_part2;
--error 1146
eval $dd_part1 statistics                            $dd_part2;
--error 1146
eval $dd_part1 views                                 $dd_part2;
--error 1146
eval $dd_part1 user_privileges                       $dd_part2;
--error 1146
eval $dd_part1 schema_privileges                     $dd_part2;
--error 1146
eval $dd_part1 table_privileges                      $dd_part2;
--error 1146
eval $dd_part1 column_privileges                     $dd_part2;
--error 1146
eval $dd_part1 table_constraints                     $dd_part2;
--error 1146
eval $dd_part1 key_column_usage                      $dd_part2;
--error 1146
eval $dd_part1 triggers                              $dd_part2;
