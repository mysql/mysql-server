#### suite/funcs_1/datadict/datadict_tables.inc

# contains all tables from INFORMATION_SCHEMA

# usage:

# --source suite/funcs_1/datadict/datadict_tables.inc


#--disable_query_log
#eval SET @aux= 'This testcase shows the error number $error_no';
#let $message= `SELECT @aux`;
#--enable_query_log
--source include/show_msg.inc

--disable_abort_on_error
eval $dd_part1 schemata                              $dd_part2;
eval $dd_part1 tables                                $dd_part2;
eval $dd_part1 columns                               $dd_part2;
eval $dd_part1 character_sets                        $dd_part2;
eval $dd_part1 collations                            $dd_part2;
eval $dd_part1 collation_character_set_applicability $dd_part2;
eval $dd_part1 routines                              $dd_part2;
eval $dd_part1 statistics                            $dd_part2;
eval $dd_part1 views                                 $dd_part2;
eval $dd_part1 user_privileges                       $dd_part2;
eval $dd_part1 schema_privileges                     $dd_part2;
eval $dd_part1 table_privileges                      $dd_part2;
eval $dd_part1 column_privileges                     $dd_part2;
eval $dd_part1 table_constraints                     $dd_part2;
eval $dd_part1 key_column_usage                      $dd_part2;
eval $dd_part1 triggers                              $dd_part2;
--enable_abort_on_error
