#### suite/funcs_1/datadict/datadict_tables.inc

# contains all tables from INFORMATION_SCHEMA

# usage:

# --source suite/funcs_1/datadict/datadict_tables.inc

--source include/show_msg.inc

eval $dd_part1 schemata                              $dd_part2;

#FIXME: splitting the "SELECT * FROM tables" in two parts until
#FIXME: Bug #12397: wrong values shown in column CREATE_OPTIONS of INFORMATION_SCHEMA.TABLES
#FIXME: is solved, like done in the _master.test, cannot be done here, so we replace here
#FIXME: the result for ALL rows.
#  9 AVG_ROW_LENGTH
# 10 DATA_LENGTH
# 11 MAX_DATA_LENGTH
## 12 INDEX_LENGTH
# 13 DATA_FREE
# 15 CREATE_TIME
# 16 UPDATE_TIME
# 20 CREATE_OPTIONS
--replace_column  9 "#ARL#"  10 "#DL#"  11 "#MDL#"  12 "#IL#"  13 "#DF#"  15 "YYYY-MM-DD hh:mm:ss"  16 "YYYY-MM-DD hh:mm:ss"  20 "#CO#"
eval $dd_part1 tables                                $dd_part2;

--source suite/funcs_1/datadict/datadict_bug_12777.inc
eval $dd_part1 columns                               $dd_part2;
eval $dd_part1 character_sets                        $dd_part2;
eval $dd_part1 collations                            where collation_name <> 'utf8_general_cs' $dd_part2;
eval $dd_part1 collation_character_set_applicability where collation_name <> 'utf8_general_cs' $dd_part2;
--replace_column 16 <Created> 17 <Last_Altered>
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

# later planned new tables for INFORMATION_SCHEMA (not before version 5.0.11)
#
# (see Reference Manual: 22.1.16. Other INFORMATION_SCHEMA Tables):
#
# parameters
# referential_constraints
#
# check them here although they currently does not exist, but using this we
# immedeatly get notice when they are implemented

#### DON'T FORGET TO ADD THE NEW TABLES TO THE CORRESPONDING FILES
#### datadict_tables_error_<errno>.test !

--error 1109
eval $dd_part1 parameters $dd_part2;

--error 0,1109
eval $dd_part1 referential_constraints $dd_part2;
