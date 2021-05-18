#
## Setup tests: tables and stored procedures

## A. Execute the test using zlib/group
#
CALL `recreate_tables_for_sp`();
###############################################################
Run '`empty`()' with no output parameters
###############################################################
RUN recvok
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok
###############################################################
Run 'one_results_set()' with no output parameters
###############################################################
RUN recvok
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok
###############################################################
Run 'three_results_sets()' with no output parameters
###############################################################
RUN recvok
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok
###############################################################
Run 'update_no_select()' with no output parameters
###############################################################
RUN recvok
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:2]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok
###############################################################
Run 'update_and_select()' with no output parameters
###############################################################
RUN recvok
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok
###############################################################
Run 'insert_no_select()' with no output parameters
###############################################################
RUN recvok
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:3]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok
###############################################################
Run 'insert_and_select()' with no output parameters
###############################################################
RUN recvok
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok

0 rows affected
###############################################################
Run 'out_param_no_select(10, @local_var)' with local var
###############################################################
RUN recvok
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok
RUN recvok
###############################################################
Run 'out_param_no_select(10, ?)' with out param
###############################################################
[ASSERT Mysqlx.Resultset.FetchDoneMoreOutParams]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok

0 rows affected
###############################################################
Run 'update_and_out_param(@local_var)' with local var
###############################################################
RUN recvok
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
Verify [Variable local_var not equal to zero]
RUN recvok
RUN recvok
###############################################################
Run 'update_and_out_param(?)' with out param
###############################################################
[ASSERT Mysqlx.Resultset.FetchDoneMoreOutParams]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok

0 rows affected
###############################################################
Run 'update_and_out_param_and_select(@local_var)' with local var
###############################################################
RUN recvok
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
Verify [Variable local_var not equal to zero]
RUN recvok
RUN recvok
###############################################################
Run 'update_and_out_param_and_select(?)' with out param
###############################################################
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreOutParams]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok

0 rows affected
###############################################################
Run 'insert_and_out_param(@local_var)' with local var
###############################################################
RUN recvok
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
Verify [Variable local_var not equal to zero]
RUN recvok
RUN recvok
###############################################################
Run 'insert_and_out_param(?)' with out param
###############################################################
[ASSERT Mysqlx.Resultset.FetchDoneMoreOutParams]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok

0 rows affected
###############################################################
Run 'insert_and_out_param_and_select(@local_var)' with local var
###############################################################
RUN recvok
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
Verify [Variable local_var not equal to zero]
RUN recvok
RUN recvok
###############################################################
Run 'insert_and_out_param_and_select(?)' with out param
###############################################################
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreOutParams]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok
RUN recvok
###############################################################
Run 'out_params_with_select(?, ?)' with multiple out parameters
###############################################################
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreOutParams]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok
RUN recvok
########################################################################
Run 'error()' with errors and without output parameters
########################################################################
[ASSERT Mysqlx.Error]
Warnings:
Level	Code	Message
Error	1146	Table 'xtest.tab_not_existing' doesn't exist
0 rows affected

RUN recvok
RUN recvok
########################################################################
Run 'with_warning()' with errors and without output parameters
########################################################################
[ASSERT Mysqlx.Error]
Warnings:
Level	Code	Message
Error	1264	Out of range value for column 'tiny' at row 1
0 rows affected

RUN recvok
RUN recvok
########################################################################
Run 'select_and_error()' with errors and without output parameters
########################################################################
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Error]
Warnings:
Level	Code	Message
Error	1146	Table 'xtest.tab_not_existing' doesn't exist
0 rows affected

RUN recvok
RUN recvok
########################################################################
Run 'select_and_warning()' with errors and without output parameters
########################################################################
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Error]
Warnings:
Level	Code	Message
Error	1264	Out of range value for column 'tiny' at row 1
0 rows affected

RUN recvok
RUN recvok
########################################################################
Run 'out_param_and_error(?)' with errors and with output parameters
########################################################################
[ASSERT Mysqlx.Error]
Warnings:
Level	Code	Message
Error	1146	Table 'xtest.tab_not_existing' doesn't exist
0 rows affected

RUN recvok
RUN recvok
########################################################################
Run 'select_out_param_and_error(?)' with errors and with output parameters
########################################################################
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Error]
Warnings:
Level	Code	Message
Error	1146	Table 'xtest.tab_not_existing' doesn't exist
0 rows affected

RUN recvok
RUN recvok
########################################################################
Run 'out_param_and_warning(?)' with errors and with output parameters
########################################################################
[ASSERT Mysqlx.Error]
Warnings:
Level	Code	Message
Error	1264	Out of range value for column 'tiny' at row 1
0 rows affected

RUN recvok
RUN recvok
########################################################################
Run 'select_out_param_and_warning(?)' with errors and with output parameters
########################################################################
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Error]
Warnings:
Level	Code	Message
Error	1406	Data too long for column 'c' at row 1
0 rows affected

RUN recvok
Mysqlx.Ok {
  msg: "bye!"
}
ok

## B. Execute the test using zlib/multiple
#
CALL `recreate_tables_for_sp`();
###############################################################
Run '`empty`()' with no output parameters
###############################################################
RUN recvok
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok
###############################################################
Run 'one_results_set()' with no output parameters
###############################################################
RUN recvok
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok
###############################################################
Run 'three_results_sets()' with no output parameters
###############################################################
RUN recvok
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok
###############################################################
Run 'update_no_select()' with no output parameters
###############################################################
RUN recvok
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:2]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok
###############################################################
Run 'update_and_select()' with no output parameters
###############################################################
RUN recvok
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok
###############################################################
Run 'insert_no_select()' with no output parameters
###############################################################
RUN recvok
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:3]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok
###############################################################
Run 'insert_and_select()' with no output parameters
###############################################################
RUN recvok
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok

0 rows affected
###############################################################
Run 'out_param_no_select(10, @local_var)' with local var
###############################################################
RUN recvok
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok
RUN recvok
###############################################################
Run 'out_param_no_select(10, ?)' with out param
###############################################################
[ASSERT Mysqlx.Resultset.FetchDoneMoreOutParams]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok

0 rows affected
###############################################################
Run 'update_and_out_param(@local_var)' with local var
###############################################################
RUN recvok
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
Verify [Variable local_var not equal to zero]
RUN recvok
RUN recvok
###############################################################
Run 'update_and_out_param(?)' with out param
###############################################################
[ASSERT Mysqlx.Resultset.FetchDoneMoreOutParams]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok

0 rows affected
###############################################################
Run 'update_and_out_param_and_select(@local_var)' with local var
###############################################################
RUN recvok
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
Verify [Variable local_var not equal to zero]
RUN recvok
RUN recvok
###############################################################
Run 'update_and_out_param_and_select(?)' with out param
###############################################################
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreOutParams]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok

0 rows affected
###############################################################
Run 'insert_and_out_param(@local_var)' with local var
###############################################################
RUN recvok
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
Verify [Variable local_var not equal to zero]
RUN recvok
RUN recvok
###############################################################
Run 'insert_and_out_param(?)' with out param
###############################################################
[ASSERT Mysqlx.Resultset.FetchDoneMoreOutParams]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok

0 rows affected
###############################################################
Run 'insert_and_out_param_and_select(@local_var)' with local var
###############################################################
RUN recvok
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
Verify [Variable local_var not equal to zero]
RUN recvok
RUN recvok
###############################################################
Run 'insert_and_out_param_and_select(?)' with out param
###############################################################
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreOutParams]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok
RUN recvok
###############################################################
Run 'out_params_with_select(?, ?)' with multiple out parameters
###############################################################
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreOutParams]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok
RUN recvok
########################################################################
Run 'error()' with errors and without output parameters
########################################################################
[ASSERT Mysqlx.Error]
Warnings:
Level	Code	Message
Error	1146	Table 'xtest.tab_not_existing' doesn't exist
0 rows affected

RUN recvok
RUN recvok
########################################################################
Run 'with_warning()' with errors and without output parameters
########################################################################
[ASSERT Mysqlx.Error]
Warnings:
Level	Code	Message
Error	1264	Out of range value for column 'tiny' at row 1
0 rows affected

RUN recvok
RUN recvok
########################################################################
Run 'select_and_error()' with errors and without output parameters
########################################################################
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Error]
Warnings:
Level	Code	Message
Error	1146	Table 'xtest.tab_not_existing' doesn't exist
0 rows affected

RUN recvok
RUN recvok
########################################################################
Run 'select_and_warning()' with errors and without output parameters
########################################################################
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Error]
Warnings:
Level	Code	Message
Error	1264	Out of range value for column 'tiny' at row 1
0 rows affected

RUN recvok
RUN recvok
########################################################################
Run 'out_param_and_error(?)' with errors and with output parameters
########################################################################
[ASSERT Mysqlx.Error]
Warnings:
Level	Code	Message
Error	1146	Table 'xtest.tab_not_existing' doesn't exist
0 rows affected

RUN recvok
RUN recvok
########################################################################
Run 'select_out_param_and_error(?)' with errors and with output parameters
########################################################################
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Error]
Warnings:
Level	Code	Message
Error	1146	Table 'xtest.tab_not_existing' doesn't exist
0 rows affected

RUN recvok
RUN recvok
########################################################################
Run 'out_param_and_warning(?)' with errors and with output parameters
########################################################################
[ASSERT Mysqlx.Error]
Warnings:
Level	Code	Message
Error	1264	Out of range value for column 'tiny' at row 1
0 rows affected

RUN recvok
RUN recvok
########################################################################
Run 'select_out_param_and_warning(?)' with errors and with output parameters
########################################################################
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Error]
Warnings:
Level	Code	Message
Error	1406	Data too long for column 'c' at row 1
0 rows affected

RUN recvok
Mysqlx.Ok {
  msg: "bye!"
}
ok

## C. Execute the test using zlib/single
#
CALL `recreate_tables_for_sp`();
###############################################################
Run '`empty`()' with no output parameters
###############################################################
RUN recvok
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok
###############################################################
Run 'one_results_set()' with no output parameters
###############################################################
RUN recvok
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok
###############################################################
Run 'three_results_sets()' with no output parameters
###############################################################
RUN recvok
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok
###############################################################
Run 'update_no_select()' with no output parameters
###############################################################
RUN recvok
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:2]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok
###############################################################
Run 'update_and_select()' with no output parameters
###############################################################
RUN recvok
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok
###############################################################
Run 'insert_no_select()' with no output parameters
###############################################################
RUN recvok
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:3]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok
###############################################################
Run 'insert_and_select()' with no output parameters
###############################################################
RUN recvok
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok

0 rows affected
###############################################################
Run 'out_param_no_select(10, @local_var)' with local var
###############################################################
RUN recvok
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok
RUN recvok
###############################################################
Run 'out_param_no_select(10, ?)' with out param
###############################################################
[ASSERT Mysqlx.Resultset.FetchDoneMoreOutParams]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok

0 rows affected
###############################################################
Run 'update_and_out_param(@local_var)' with local var
###############################################################
RUN recvok
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
Verify [Variable local_var not equal to zero]
RUN recvok
RUN recvok
###############################################################
Run 'update_and_out_param(?)' with out param
###############################################################
[ASSERT Mysqlx.Resultset.FetchDoneMoreOutParams]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok

0 rows affected
###############################################################
Run 'update_and_out_param_and_select(@local_var)' with local var
###############################################################
RUN recvok
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
Verify [Variable local_var not equal to zero]
RUN recvok
RUN recvok
###############################################################
Run 'update_and_out_param_and_select(?)' with out param
###############################################################
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreOutParams]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok

0 rows affected
###############################################################
Run 'insert_and_out_param(@local_var)' with local var
###############################################################
RUN recvok
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
Verify [Variable local_var not equal to zero]
RUN recvok
RUN recvok
###############################################################
Run 'insert_and_out_param(?)' with out param
###############################################################
[ASSERT Mysqlx.Resultset.FetchDoneMoreOutParams]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok

0 rows affected
###############################################################
Run 'insert_and_out_param_and_select(@local_var)' with local var
###############################################################
RUN recvok
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
Verify [Variable local_var not equal to zero]
RUN recvok
RUN recvok
###############################################################
Run 'insert_and_out_param_and_select(?)' with out param
###############################################################
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreOutParams]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok
RUN recvok
###############################################################
Run 'out_params_with_select(?, ?)' with multiple out parameters
###############################################################
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreOutParams]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok
RUN recvok
########################################################################
Run 'error()' with errors and without output parameters
########################################################################
[ASSERT Mysqlx.Error]
Warnings:
Level	Code	Message
Error	1146	Table 'xtest.tab_not_existing' doesn't exist
0 rows affected

RUN recvok
RUN recvok
########################################################################
Run 'with_warning()' with errors and without output parameters
########################################################################
[ASSERT Mysqlx.Error]
Warnings:
Level	Code	Message
Error	1264	Out of range value for column 'tiny' at row 1
0 rows affected

RUN recvok
RUN recvok
########################################################################
Run 'select_and_error()' with errors and without output parameters
########################################################################
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Error]
Warnings:
Level	Code	Message
Error	1146	Table 'xtest.tab_not_existing' doesn't exist
0 rows affected

RUN recvok
RUN recvok
########################################################################
Run 'select_and_warning()' with errors and without output parameters
########################################################################
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Error]
Warnings:
Level	Code	Message
Error	1264	Out of range value for column 'tiny' at row 1
0 rows affected

RUN recvok
RUN recvok
########################################################################
Run 'out_param_and_error(?)' with errors and with output parameters
########################################################################
[ASSERT Mysqlx.Error]
Warnings:
Level	Code	Message
Error	1146	Table 'xtest.tab_not_existing' doesn't exist
0 rows affected

RUN recvok
RUN recvok
########################################################################
Run 'select_out_param_and_error(?)' with errors and with output parameters
########################################################################
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Error]
Warnings:
Level	Code	Message
Error	1146	Table 'xtest.tab_not_existing' doesn't exist
0 rows affected

RUN recvok
RUN recvok
########################################################################
Run 'out_param_and_warning(?)' with errors and with output parameters
########################################################################
[ASSERT Mysqlx.Error]
Warnings:
Level	Code	Message
Error	1264	Out of range value for column 'tiny' at row 1
0 rows affected

RUN recvok
RUN recvok
########################################################################
Run 'select_out_param_and_warning(?)' with errors and with output parameters
########################################################################
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Error]
Warnings:
Level	Code	Message
Error	1406	Data too long for column 'c' at row 1
0 rows affected

RUN recvok
Mysqlx.Ok {
  msg: "bye!"
}
ok

## D. Execute the test using lz4/group
#
CALL `recreate_tables_for_sp`();
###############################################################
Run '`empty`()' with no output parameters
###############################################################
RUN recvok
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok
###############################################################
Run 'one_results_set()' with no output parameters
###############################################################
RUN recvok
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok
###############################################################
Run 'three_results_sets()' with no output parameters
###############################################################
RUN recvok
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok
###############################################################
Run 'update_no_select()' with no output parameters
###############################################################
RUN recvok
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:2]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok
###############################################################
Run 'update_and_select()' with no output parameters
###############################################################
RUN recvok
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok
###############################################################
Run 'insert_no_select()' with no output parameters
###############################################################
RUN recvok
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:3]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok
###############################################################
Run 'insert_and_select()' with no output parameters
###############################################################
RUN recvok
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok

0 rows affected
###############################################################
Run 'out_param_no_select(10, @local_var)' with local var
###############################################################
RUN recvok
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok
RUN recvok
###############################################################
Run 'out_param_no_select(10, ?)' with out param
###############################################################
[ASSERT Mysqlx.Resultset.FetchDoneMoreOutParams]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok

0 rows affected
###############################################################
Run 'update_and_out_param(@local_var)' with local var
###############################################################
RUN recvok
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
Verify [Variable local_var not equal to zero]
RUN recvok
RUN recvok
###############################################################
Run 'update_and_out_param(?)' with out param
###############################################################
[ASSERT Mysqlx.Resultset.FetchDoneMoreOutParams]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok

0 rows affected
###############################################################
Run 'update_and_out_param_and_select(@local_var)' with local var
###############################################################
RUN recvok
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
Verify [Variable local_var not equal to zero]
RUN recvok
RUN recvok
###############################################################
Run 'update_and_out_param_and_select(?)' with out param
###############################################################
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreOutParams]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok

0 rows affected
###############################################################
Run 'insert_and_out_param(@local_var)' with local var
###############################################################
RUN recvok
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
Verify [Variable local_var not equal to zero]
RUN recvok
RUN recvok
###############################################################
Run 'insert_and_out_param(?)' with out param
###############################################################
[ASSERT Mysqlx.Resultset.FetchDoneMoreOutParams]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok

0 rows affected
###############################################################
Run 'insert_and_out_param_and_select(@local_var)' with local var
###############################################################
RUN recvok
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
Verify [Variable local_var not equal to zero]
RUN recvok
RUN recvok
###############################################################
Run 'insert_and_out_param_and_select(?)' with out param
###############################################################
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreOutParams]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok
RUN recvok
###############################################################
Run 'out_params_with_select(?, ?)' with multiple out parameters
###############################################################
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreOutParams]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok
RUN recvok
########################################################################
Run 'error()' with errors and without output parameters
########################################################################
[ASSERT Mysqlx.Error]
Warnings:
Level	Code	Message
Error	1146	Table 'xtest.tab_not_existing' doesn't exist
0 rows affected

RUN recvok
RUN recvok
########################################################################
Run 'with_warning()' with errors and without output parameters
########################################################################
[ASSERT Mysqlx.Error]
Warnings:
Level	Code	Message
Error	1264	Out of range value for column 'tiny' at row 1
0 rows affected

RUN recvok
RUN recvok
########################################################################
Run 'select_and_error()' with errors and without output parameters
########################################################################
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Error]
Warnings:
Level	Code	Message
Error	1146	Table 'xtest.tab_not_existing' doesn't exist
0 rows affected

RUN recvok
RUN recvok
########################################################################
Run 'select_and_warning()' with errors and without output parameters
########################################################################
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Error]
Warnings:
Level	Code	Message
Error	1264	Out of range value for column 'tiny' at row 1
0 rows affected

RUN recvok
RUN recvok
########################################################################
Run 'out_param_and_error(?)' with errors and with output parameters
########################################################################
[ASSERT Mysqlx.Error]
Warnings:
Level	Code	Message
Error	1146	Table 'xtest.tab_not_existing' doesn't exist
0 rows affected

RUN recvok
RUN recvok
########################################################################
Run 'select_out_param_and_error(?)' with errors and with output parameters
########################################################################
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Error]
Warnings:
Level	Code	Message
Error	1146	Table 'xtest.tab_not_existing' doesn't exist
0 rows affected

RUN recvok
RUN recvok
########################################################################
Run 'out_param_and_warning(?)' with errors and with output parameters
########################################################################
[ASSERT Mysqlx.Error]
Warnings:
Level	Code	Message
Error	1264	Out of range value for column 'tiny' at row 1
0 rows affected

RUN recvok
RUN recvok
########################################################################
Run 'select_out_param_and_warning(?)' with errors and with output parameters
########################################################################
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Error]
Warnings:
Level	Code	Message
Error	1406	Data too long for column 'c' at row 1
0 rows affected

RUN recvok
Mysqlx.Ok {
  msg: "bye!"
}
ok

## E. Execute the test using lz4/multiple
#
CALL `recreate_tables_for_sp`();
###############################################################
Run '`empty`()' with no output parameters
###############################################################
RUN recvok
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok
###############################################################
Run 'one_results_set()' with no output parameters
###############################################################
RUN recvok
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok
###############################################################
Run 'three_results_sets()' with no output parameters
###############################################################
RUN recvok
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok
###############################################################
Run 'update_no_select()' with no output parameters
###############################################################
RUN recvok
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:2]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok
###############################################################
Run 'update_and_select()' with no output parameters
###############################################################
RUN recvok
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok
###############################################################
Run 'insert_no_select()' with no output parameters
###############################################################
RUN recvok
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:3]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok
###############################################################
Run 'insert_and_select()' with no output parameters
###############################################################
RUN recvok
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok

0 rows affected
###############################################################
Run 'out_param_no_select(10, @local_var)' with local var
###############################################################
RUN recvok
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok
RUN recvok
###############################################################
Run 'out_param_no_select(10, ?)' with out param
###############################################################
[ASSERT Mysqlx.Resultset.FetchDoneMoreOutParams]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok

0 rows affected
###############################################################
Run 'update_and_out_param(@local_var)' with local var
###############################################################
RUN recvok
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
Verify [Variable local_var not equal to zero]
RUN recvok
RUN recvok
###############################################################
Run 'update_and_out_param(?)' with out param
###############################################################
[ASSERT Mysqlx.Resultset.FetchDoneMoreOutParams]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok

0 rows affected
###############################################################
Run 'update_and_out_param_and_select(@local_var)' with local var
###############################################################
RUN recvok
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
Verify [Variable local_var not equal to zero]
RUN recvok
RUN recvok
###############################################################
Run 'update_and_out_param_and_select(?)' with out param
###############################################################
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreOutParams]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok

0 rows affected
###############################################################
Run 'insert_and_out_param(@local_var)' with local var
###############################################################
RUN recvok
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
Verify [Variable local_var not equal to zero]
RUN recvok
RUN recvok
###############################################################
Run 'insert_and_out_param(?)' with out param
###############################################################
[ASSERT Mysqlx.Resultset.FetchDoneMoreOutParams]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok

0 rows affected
###############################################################
Run 'insert_and_out_param_and_select(@local_var)' with local var
###############################################################
RUN recvok
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
Verify [Variable local_var not equal to zero]
RUN recvok
RUN recvok
###############################################################
Run 'insert_and_out_param_and_select(?)' with out param
###############################################################
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreOutParams]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok
RUN recvok
###############################################################
Run 'out_params_with_select(?, ?)' with multiple out parameters
###############################################################
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreOutParams]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok
RUN recvok
########################################################################
Run 'error()' with errors and without output parameters
########################################################################
[ASSERT Mysqlx.Error]
Warnings:
Level	Code	Message
Error	1146	Table 'xtest.tab_not_existing' doesn't exist
0 rows affected

RUN recvok
RUN recvok
########################################################################
Run 'with_warning()' with errors and without output parameters
########################################################################
[ASSERT Mysqlx.Error]
Warnings:
Level	Code	Message
Error	1264	Out of range value for column 'tiny' at row 1
0 rows affected

RUN recvok
RUN recvok
########################################################################
Run 'select_and_error()' with errors and without output parameters
########################################################################
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Error]
Warnings:
Level	Code	Message
Error	1146	Table 'xtest.tab_not_existing' doesn't exist
0 rows affected

RUN recvok
RUN recvok
########################################################################
Run 'select_and_warning()' with errors and without output parameters
########################################################################
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Error]
Warnings:
Level	Code	Message
Error	1264	Out of range value for column 'tiny' at row 1
0 rows affected

RUN recvok
RUN recvok
########################################################################
Run 'out_param_and_error(?)' with errors and with output parameters
########################################################################
[ASSERT Mysqlx.Error]
Warnings:
Level	Code	Message
Error	1146	Table 'xtest.tab_not_existing' doesn't exist
0 rows affected

RUN recvok
RUN recvok
########################################################################
Run 'select_out_param_and_error(?)' with errors and with output parameters
########################################################################
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Error]
Warnings:
Level	Code	Message
Error	1146	Table 'xtest.tab_not_existing' doesn't exist
0 rows affected

RUN recvok
RUN recvok
########################################################################
Run 'out_param_and_warning(?)' with errors and with output parameters
########################################################################
[ASSERT Mysqlx.Error]
Warnings:
Level	Code	Message
Error	1264	Out of range value for column 'tiny' at row 1
0 rows affected

RUN recvok
RUN recvok
########################################################################
Run 'select_out_param_and_warning(?)' with errors and with output parameters
########################################################################
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Error]
Warnings:
Level	Code	Message
Error	1406	Data too long for column 'c' at row 1
0 rows affected

RUN recvok
Mysqlx.Ok {
  msg: "bye!"
}
ok

## F. Execute the test using lz4/single
#
CALL `recreate_tables_for_sp`();
###############################################################
Run '`empty`()' with no output parameters
###############################################################
RUN recvok
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok
###############################################################
Run 'one_results_set()' with no output parameters
###############################################################
RUN recvok
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok
###############################################################
Run 'three_results_sets()' with no output parameters
###############################################################
RUN recvok
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok
###############################################################
Run 'update_no_select()' with no output parameters
###############################################################
RUN recvok
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:2]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok
###############################################################
Run 'update_and_select()' with no output parameters
###############################################################
RUN recvok
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok
###############################################################
Run 'insert_no_select()' with no output parameters
###############################################################
RUN recvok
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:3]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok
###############################################################
Run 'insert_and_select()' with no output parameters
###############################################################
RUN recvok
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok

0 rows affected
###############################################################
Run 'out_param_no_select(10, @local_var)' with local var
###############################################################
RUN recvok
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok
RUN recvok
###############################################################
Run 'out_param_no_select(10, ?)' with out param
###############################################################
[ASSERT Mysqlx.Resultset.FetchDoneMoreOutParams]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok

0 rows affected
###############################################################
Run 'update_and_out_param(@local_var)' with local var
###############################################################
RUN recvok
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
Verify [Variable local_var not equal to zero]
RUN recvok
RUN recvok
###############################################################
Run 'update_and_out_param(?)' with out param
###############################################################
[ASSERT Mysqlx.Resultset.FetchDoneMoreOutParams]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok

0 rows affected
###############################################################
Run 'update_and_out_param_and_select(@local_var)' with local var
###############################################################
RUN recvok
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
Verify [Variable local_var not equal to zero]
RUN recvok
RUN recvok
###############################################################
Run 'update_and_out_param_and_select(?)' with out param
###############################################################
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreOutParams]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok

0 rows affected
###############################################################
Run 'insert_and_out_param(@local_var)' with local var
###############################################################
RUN recvok
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
Verify [Variable local_var not equal to zero]
RUN recvok
RUN recvok
###############################################################
Run 'insert_and_out_param(?)' with out param
###############################################################
[ASSERT Mysqlx.Resultset.FetchDoneMoreOutParams]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok

0 rows affected
###############################################################
Run 'insert_and_out_param_and_select(@local_var)' with local var
###############################################################
RUN recvok
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
Verify [Variable local_var not equal to zero]
RUN recvok
RUN recvok
###############################################################
Run 'insert_and_out_param_and_select(?)' with out param
###############################################################
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreOutParams]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok
RUN recvok
###############################################################
Run 'out_params_with_select(?, ?)' with multiple out parameters
###############################################################
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreOutParams]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok
RUN recvok
########################################################################
Run 'error()' with errors and without output parameters
########################################################################
[ASSERT Mysqlx.Error]
Warnings:
Level	Code	Message
Error	1146	Table 'xtest.tab_not_existing' doesn't exist
0 rows affected

RUN recvok
RUN recvok
########################################################################
Run 'with_warning()' with errors and without output parameters
########################################################################
[ASSERT Mysqlx.Error]
Warnings:
Level	Code	Message
Error	1264	Out of range value for column 'tiny' at row 1
0 rows affected

RUN recvok
RUN recvok
########################################################################
Run 'select_and_error()' with errors and without output parameters
########################################################################
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Error]
Warnings:
Level	Code	Message
Error	1146	Table 'xtest.tab_not_existing' doesn't exist
0 rows affected

RUN recvok
RUN recvok
########################################################################
Run 'select_and_warning()' with errors and without output parameters
########################################################################
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Error]
Warnings:
Level	Code	Message
Error	1264	Out of range value for column 'tiny' at row 1
0 rows affected

RUN recvok
RUN recvok
########################################################################
Run 'out_param_and_error(?)' with errors and with output parameters
########################################################################
[ASSERT Mysqlx.Error]
Warnings:
Level	Code	Message
Error	1146	Table 'xtest.tab_not_existing' doesn't exist
0 rows affected

RUN recvok
RUN recvok
########################################################################
Run 'select_out_param_and_error(?)' with errors and with output parameters
########################################################################
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Error]
Warnings:
Level	Code	Message
Error	1146	Table 'xtest.tab_not_existing' doesn't exist
0 rows affected

RUN recvok
RUN recvok
########################################################################
Run 'out_param_and_warning(?)' with errors and with output parameters
########################################################################
[ASSERT Mysqlx.Error]
Warnings:
Level	Code	Message
Error	1264	Out of range value for column 'tiny' at row 1
0 rows affected

RUN recvok
RUN recvok
########################################################################
Run 'select_out_param_and_warning(?)' with errors and with output parameters
########################################################################
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Error]
Warnings:
Level	Code	Message
Error	1406	Data too long for column 'c' at row 1
0 rows affected

RUN recvok
Mysqlx.Ok {
  msg: "bye!"
}
ok

## G. Execute the test using zstd/group
#
CALL `recreate_tables_for_sp`();
###############################################################
Run '`empty`()' with no output parameters
###############################################################
RUN recvok
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok
###############################################################
Run 'one_results_set()' with no output parameters
###############################################################
RUN recvok
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok
###############################################################
Run 'three_results_sets()' with no output parameters
###############################################################
RUN recvok
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok
###############################################################
Run 'update_no_select()' with no output parameters
###############################################################
RUN recvok
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:2]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok
###############################################################
Run 'update_and_select()' with no output parameters
###############################################################
RUN recvok
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok
###############################################################
Run 'insert_no_select()' with no output parameters
###############################################################
RUN recvok
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:3]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok
###############################################################
Run 'insert_and_select()' with no output parameters
###############################################################
RUN recvok
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok

0 rows affected
###############################################################
Run 'out_param_no_select(10, @local_var)' with local var
###############################################################
RUN recvok
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok
RUN recvok
###############################################################
Run 'out_param_no_select(10, ?)' with out param
###############################################################
[ASSERT Mysqlx.Resultset.FetchDoneMoreOutParams]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok

0 rows affected
###############################################################
Run 'update_and_out_param(@local_var)' with local var
###############################################################
RUN recvok
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
Verify [Variable local_var not equal to zero]
RUN recvok
RUN recvok
###############################################################
Run 'update_and_out_param(?)' with out param
###############################################################
[ASSERT Mysqlx.Resultset.FetchDoneMoreOutParams]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok

0 rows affected
###############################################################
Run 'update_and_out_param_and_select(@local_var)' with local var
###############################################################
RUN recvok
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
Verify [Variable local_var not equal to zero]
RUN recvok
RUN recvok
###############################################################
Run 'update_and_out_param_and_select(?)' with out param
###############################################################
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreOutParams]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok

0 rows affected
###############################################################
Run 'insert_and_out_param(@local_var)' with local var
###############################################################
RUN recvok
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
Verify [Variable local_var not equal to zero]
RUN recvok
RUN recvok
###############################################################
Run 'insert_and_out_param(?)' with out param
###############################################################
[ASSERT Mysqlx.Resultset.FetchDoneMoreOutParams]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok

0 rows affected
###############################################################
Run 'insert_and_out_param_and_select(@local_var)' with local var
###############################################################
RUN recvok
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
Verify [Variable local_var not equal to zero]
RUN recvok
RUN recvok
###############################################################
Run 'insert_and_out_param_and_select(?)' with out param
###############################################################
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreOutParams]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok
RUN recvok
###############################################################
Run 'out_params_with_select(?, ?)' with multiple out parameters
###############################################################
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreOutParams]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok
RUN recvok
########################################################################
Run 'error()' with errors and without output parameters
########################################################################
[ASSERT Mysqlx.Error]
Warnings:
Level	Code	Message
Error	1146	Table 'xtest.tab_not_existing' doesn't exist
0 rows affected

RUN recvok
RUN recvok
########################################################################
Run 'with_warning()' with errors and without output parameters
########################################################################
[ASSERT Mysqlx.Error]
Warnings:
Level	Code	Message
Error	1264	Out of range value for column 'tiny' at row 1
0 rows affected

RUN recvok
RUN recvok
########################################################################
Run 'select_and_error()' with errors and without output parameters
########################################################################
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Error]
Warnings:
Level	Code	Message
Error	1146	Table 'xtest.tab_not_existing' doesn't exist
0 rows affected

RUN recvok
RUN recvok
########################################################################
Run 'select_and_warning()' with errors and without output parameters
########################################################################
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Error]
Warnings:
Level	Code	Message
Error	1264	Out of range value for column 'tiny' at row 1
0 rows affected

RUN recvok
RUN recvok
########################################################################
Run 'out_param_and_error(?)' with errors and with output parameters
########################################################################
[ASSERT Mysqlx.Error]
Warnings:
Level	Code	Message
Error	1146	Table 'xtest.tab_not_existing' doesn't exist
0 rows affected

RUN recvok
RUN recvok
########################################################################
Run 'select_out_param_and_error(?)' with errors and with output parameters
########################################################################
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Error]
Warnings:
Level	Code	Message
Error	1146	Table 'xtest.tab_not_existing' doesn't exist
0 rows affected

RUN recvok
RUN recvok
########################################################################
Run 'out_param_and_warning(?)' with errors and with output parameters
########################################################################
[ASSERT Mysqlx.Error]
Warnings:
Level	Code	Message
Error	1264	Out of range value for column 'tiny' at row 1
0 rows affected

RUN recvok
RUN recvok
########################################################################
Run 'select_out_param_and_warning(?)' with errors and with output parameters
########################################################################
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Error]
Warnings:
Level	Code	Message
Error	1406	Data too long for column 'c' at row 1
0 rows affected

RUN recvok
Mysqlx.Ok {
  msg: "bye!"
}
ok

## H. Execute the test using zstd/multiple
#
CALL `recreate_tables_for_sp`();
###############################################################
Run '`empty`()' with no output parameters
###############################################################
RUN recvok
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok
###############################################################
Run 'one_results_set()' with no output parameters
###############################################################
RUN recvok
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok
###############################################################
Run 'three_results_sets()' with no output parameters
###############################################################
RUN recvok
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok
###############################################################
Run 'update_no_select()' with no output parameters
###############################################################
RUN recvok
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:2]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok
###############################################################
Run 'update_and_select()' with no output parameters
###############################################################
RUN recvok
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok
###############################################################
Run 'insert_no_select()' with no output parameters
###############################################################
RUN recvok
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:3]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok
###############################################################
Run 'insert_and_select()' with no output parameters
###############################################################
RUN recvok
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok

0 rows affected
###############################################################
Run 'out_param_no_select(10, @local_var)' with local var
###############################################################
RUN recvok
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok
RUN recvok
###############################################################
Run 'out_param_no_select(10, ?)' with out param
###############################################################
[ASSERT Mysqlx.Resultset.FetchDoneMoreOutParams]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok

0 rows affected
###############################################################
Run 'update_and_out_param(@local_var)' with local var
###############################################################
RUN recvok
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
Verify [Variable local_var not equal to zero]
RUN recvok
RUN recvok
###############################################################
Run 'update_and_out_param(?)' with out param
###############################################################
[ASSERT Mysqlx.Resultset.FetchDoneMoreOutParams]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok

0 rows affected
###############################################################
Run 'update_and_out_param_and_select(@local_var)' with local var
###############################################################
RUN recvok
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
Verify [Variable local_var not equal to zero]
RUN recvok
RUN recvok
###############################################################
Run 'update_and_out_param_and_select(?)' with out param
###############################################################
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreOutParams]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok

0 rows affected
###############################################################
Run 'insert_and_out_param(@local_var)' with local var
###############################################################
RUN recvok
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
Verify [Variable local_var not equal to zero]
RUN recvok
RUN recvok
###############################################################
Run 'insert_and_out_param(?)' with out param
###############################################################
[ASSERT Mysqlx.Resultset.FetchDoneMoreOutParams]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok

0 rows affected
###############################################################
Run 'insert_and_out_param_and_select(@local_var)' with local var
###############################################################
RUN recvok
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
Verify [Variable local_var not equal to zero]
RUN recvok
RUN recvok
###############################################################
Run 'insert_and_out_param_and_select(?)' with out param
###############################################################
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreOutParams]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok
RUN recvok
###############################################################
Run 'out_params_with_select(?, ?)' with multiple out parameters
###############################################################
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreOutParams]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok
RUN recvok
########################################################################
Run 'error()' with errors and without output parameters
########################################################################
[ASSERT Mysqlx.Error]
Warnings:
Level	Code	Message
Error	1146	Table 'xtest.tab_not_existing' doesn't exist
0 rows affected

RUN recvok
RUN recvok
########################################################################
Run 'with_warning()' with errors and without output parameters
########################################################################
[ASSERT Mysqlx.Error]
Warnings:
Level	Code	Message
Error	1264	Out of range value for column 'tiny' at row 1
0 rows affected

RUN recvok
RUN recvok
########################################################################
Run 'select_and_error()' with errors and without output parameters
########################################################################
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Error]
Warnings:
Level	Code	Message
Error	1146	Table 'xtest.tab_not_existing' doesn't exist
0 rows affected

RUN recvok
RUN recvok
########################################################################
Run 'select_and_warning()' with errors and without output parameters
########################################################################
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Error]
Warnings:
Level	Code	Message
Error	1264	Out of range value for column 'tiny' at row 1
0 rows affected

RUN recvok
RUN recvok
########################################################################
Run 'out_param_and_error(?)' with errors and with output parameters
########################################################################
[ASSERT Mysqlx.Error]
Warnings:
Level	Code	Message
Error	1146	Table 'xtest.tab_not_existing' doesn't exist
0 rows affected

RUN recvok
RUN recvok
########################################################################
Run 'select_out_param_and_error(?)' with errors and with output parameters
########################################################################
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Error]
Warnings:
Level	Code	Message
Error	1146	Table 'xtest.tab_not_existing' doesn't exist
0 rows affected

RUN recvok
RUN recvok
########################################################################
Run 'out_param_and_warning(?)' with errors and with output parameters
########################################################################
[ASSERT Mysqlx.Error]
Warnings:
Level	Code	Message
Error	1264	Out of range value for column 'tiny' at row 1
0 rows affected

RUN recvok
RUN recvok
########################################################################
Run 'select_out_param_and_warning(?)' with errors and with output parameters
########################################################################
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Error]
Warnings:
Level	Code	Message
Error	1406	Data too long for column 'c' at row 1
0 rows affected

RUN recvok
Mysqlx.Ok {
  msg: "bye!"
}
ok

## I. Execute the test using zstd/single
#
CALL `recreate_tables_for_sp`();
###############################################################
Run '`empty`()' with no output parameters
###############################################################
RUN recvok
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok
###############################################################
Run 'one_results_set()' with no output parameters
###############################################################
RUN recvok
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok
###############################################################
Run 'three_results_sets()' with no output parameters
###############################################################
RUN recvok
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok
###############################################################
Run 'update_no_select()' with no output parameters
###############################################################
RUN recvok
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:2]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok
###############################################################
Run 'update_and_select()' with no output parameters
###############################################################
RUN recvok
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok
###############################################################
Run 'insert_no_select()' with no output parameters
###############################################################
RUN recvok
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:3]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok
###############################################################
Run 'insert_and_select()' with no output parameters
###############################################################
RUN recvok
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok

0 rows affected
###############################################################
Run 'out_param_no_select(10, @local_var)' with local var
###############################################################
RUN recvok
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok
RUN recvok
###############################################################
Run 'out_param_no_select(10, ?)' with out param
###############################################################
[ASSERT Mysqlx.Resultset.FetchDoneMoreOutParams]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok

0 rows affected
###############################################################
Run 'update_and_out_param(@local_var)' with local var
###############################################################
RUN recvok
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
Verify [Variable local_var not equal to zero]
RUN recvok
RUN recvok
###############################################################
Run 'update_and_out_param(?)' with out param
###############################################################
[ASSERT Mysqlx.Resultset.FetchDoneMoreOutParams]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok

0 rows affected
###############################################################
Run 'update_and_out_param_and_select(@local_var)' with local var
###############################################################
RUN recvok
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
Verify [Variable local_var not equal to zero]
RUN recvok
RUN recvok
###############################################################
Run 'update_and_out_param_and_select(?)' with out param
###############################################################
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreOutParams]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok

0 rows affected
###############################################################
Run 'insert_and_out_param(@local_var)' with local var
###############################################################
RUN recvok
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
Verify [Variable local_var not equal to zero]
RUN recvok
RUN recvok
###############################################################
Run 'insert_and_out_param(?)' with out param
###############################################################
[ASSERT Mysqlx.Resultset.FetchDoneMoreOutParams]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok

0 rows affected
###############################################################
Run 'insert_and_out_param_and_select(@local_var)' with local var
###############################################################
RUN recvok
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
Verify [Variable local_var not equal to zero]
RUN recvok
RUN recvok
###############################################################
Run 'insert_and_out_param_and_select(?)' with out param
###############################################################
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreOutParams]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok
RUN recvok
###############################################################
Run 'out_params_with_select(?, ?)' with multiple out parameters
###############################################################
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreOutParams]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN recvok
RUN recvok
########################################################################
Run 'error()' with errors and without output parameters
########################################################################
[ASSERT Mysqlx.Error]
Warnings:
Level	Code	Message
Error	1146	Table 'xtest.tab_not_existing' doesn't exist
0 rows affected

RUN recvok
RUN recvok
########################################################################
Run 'with_warning()' with errors and without output parameters
########################################################################
[ASSERT Mysqlx.Error]
Warnings:
Level	Code	Message
Error	1264	Out of range value for column 'tiny' at row 1
0 rows affected

RUN recvok
RUN recvok
########################################################################
Run 'select_and_error()' with errors and without output parameters
########################################################################
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Error]
Warnings:
Level	Code	Message
Error	1146	Table 'xtest.tab_not_existing' doesn't exist
0 rows affected

RUN recvok
RUN recvok
########################################################################
Run 'select_and_warning()' with errors and without output parameters
########################################################################
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Error]
Warnings:
Level	Code	Message
Error	1264	Out of range value for column 'tiny' at row 1
0 rows affected

RUN recvok
RUN recvok
########################################################################
Run 'out_param_and_error(?)' with errors and with output parameters
########################################################################
[ASSERT Mysqlx.Error]
Warnings:
Level	Code	Message
Error	1146	Table 'xtest.tab_not_existing' doesn't exist
0 rows affected

RUN recvok
RUN recvok
########################################################################
Run 'select_out_param_and_error(?)' with errors and with output parameters
########################################################################
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Error]
Warnings:
Level	Code	Message
Error	1146	Table 'xtest.tab_not_existing' doesn't exist
0 rows affected

RUN recvok
RUN recvok
########################################################################
Run 'out_param_and_warning(?)' with errors and with output parameters
########################################################################
[ASSERT Mysqlx.Error]
Warnings:
Level	Code	Message
Error	1264	Out of range value for column 'tiny' at row 1
0 rows affected

RUN recvok
RUN recvok
########################################################################
Run 'select_out_param_and_warning(?)' with errors and with output parameters
########################################################################
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Error]
Warnings:
Level	Code	Message
Error	1406	Data too long for column 'c' at row 1
0 rows affected

RUN recvok
Mysqlx.Ok {
  msg: "bye!"
}
ok
