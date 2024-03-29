--let $STORED_PROC_DB_NAME=xtest

--source ../include/stored_procedures.inc
--write_file $MYSQL_TMP_DIR/prep_stmt_cursor.xpl
-->import assert_messages.macro
-->import assert_notices.macro
-->import assert_variable.macro
-->quiet

-->macro Prepare_statement_with_procedure %PROCEDURE%
Mysqlx.Prepare.Prepare {
  stmt_id: 1
  stmt: {
    type: STMT
    stmt_execute: {
      stmt: 'CALL xtest.%PROCEDURE%'
    }
  }
}
-->recvok
-->endmacro

-->macro Execute_no_args
Mysqlx.Prepare.Execute {
  stmt_id: 1
}
-->endmacro

-->macro Deallocate_statement
Mysqlx.Prepare.Deallocate {
  stmt_id: 1
}
-->recvok
-->endmacro

-->macro Execute_procedure_no_output_params_base %PROCEDURE%
-->callmacro Prepare_statement_with_procedure %PROCEDURE%
-->callmacro Execute_no_args
-->endmacro

-->macro Execute_procedure_no_output_params %PROCEDURE%
-->echo ###############################################################
-->echo Run '%PROCEDURE%' with no output parameters
-->echo ###############################################################
-->callmacro Execute_procedure_no_output_params_base %PROCEDURE%
-->endmacro

-->macro Assert_local_var
-->echo Verify [Variable local_var not equal to zero]
-->noquery_result
-->stmtsql SELECT @local_var;
-->recvtovar %TEST_VAR%
-->assert_ne 0	%TEST_VAR%
-->query_result
-->endmacro

-->macro Execute_procedure_with_local_var %PROCEDURE%
-->stmtsql SET @local_var=0;
-->recvresult
-->echo ###############################################################
-->echo Run '%PROCEDURE%' with local var
-->echo ###############################################################
-->callmacro Execute_procedure_no_output_params_base %PROCEDURE%
-->endmacro

-->macro Execute_with_args %OUT_PARAM%
Mysqlx.Prepare.Execute {
  stmt_id: 1
  args {
    type: SCALAR scalar {
      type: V_STRING v_string {
        value: "%OUT_PARAM%"
      }
    }
  }
}
-->endmacro

-->macro Execute_procedure_with_out_param %PROCEDURE% %OUT_PARAM%
-->callmacro Prepare_statement_with_procedure %PROCEDURE%
-->echo ###############################################################
-->echo Run '%PROCEDURE%' with out param
-->echo ###############################################################
-->callmacro Execute_with_args %OUT_PARAM%
-->endmacro

-->macro Execute_procedure_with_multiple_out_params %PROCEDURE% %OUT_PARAM1% %OUT_PARAM2%
-->callmacro Prepare_statement_with_procedure %PROCEDURE%
-->echo ###############################################################
-->echo Run '%PROCEDURE%' with multiple out parameters
-->echo ###############################################################
Mysqlx.Cursor.Open {
  cursor_id: 1
  stmt: {
    type: PREPARE_EXECUTE
    prepare_execute: {
      stmt_id: 1
      args {
        type: SCALAR scalar {
          type: V_STRING v_string {
            value: "%OUT_PARAM1%"
          }
        }
      }
      args {
        type: SCALAR scalar {
          type: V_STRING v_string {
            value: "%OUT_PARAM2%"
          }
        }
      }
    }
  }
}
-->endmacro

-->macro Show_warnings
-->echo Warnings:
-->stmtsql SHOW WARNINGS
-->recvresult
-->echo
-->endmacro

-->macro Execute_procedure_no_output_params_with_error %PROCEDURE%
-->callmacro Prepare_statement_with_procedure %PROCEDURE%
-->echo ########################################################################
-->echo Run '%PROCEDURE%' with errors and without output parameters
-->echo ########################################################################
-->callmacro Execute_no_args
-->endmacro

-->macro Execute_procedure_with_output_params_and_with_error %PROCEDURE% %OUT_PARAM%
-->callmacro Prepare_statement_with_procedure %PROCEDURE%
-->echo ########################################################################
-->echo Run '%PROCEDURE%' with errors and with output parameters
-->echo ########################################################################
-->callmacro Execute_with_args %OUT_PARAM%
-->endmacro

# Check stored procedures without any output parameters
#
-->callmacro Execute_procedure_no_output_params `empty`()
-->callmacro Assert_notice_session_state	ROWS_AFFECTED	v_unsigned_int:0
-->callmacro Assert_message Mysqlx.Sql.StmtExecuteOk
-->callmacro Deallocate_statement

-->callmacro Execute_procedure_no_output_params one_results_set()
-->callmacro Assert_metadata UINT	'c1t1'
-->callmacro Assert_metadata BYTES	'c2t1'
-->callmacro Assert_rows 3
-->callmacro Assert_message Mysqlx.Resultset.FetchDoneMoreResultsets
-->callmacro Assert_message Mysqlx.Resultset.FetchDone
-->callmacro Assert_notice_session_state	ROWS_AFFECTED	v_unsigned_int:0
-->callmacro Assert_message Mysqlx.Sql.StmtExecuteOk
-->callmacro Deallocate_statement

-->callmacro Execute_procedure_no_output_params three_results_sets()
-->callmacro Assert_metadata UINT	'c1t1'
-->callmacro Assert_metadata BYTES	'c2t1'
-->callmacro Assert_rows 3
-->callmacro Assert_message Mysqlx.Resultset.FetchDoneMoreResultsets
-->callmacro Assert_metadata DATETIME	'c1t2'
-->callmacro Assert_rows 4
-->callmacro Assert_message Mysqlx.Resultset.FetchDoneMoreResultsets
-->callmacro Assert_metadata UINT	'c1t1'
-->callmacro Assert_metadata BYTES	'c2t1'
-->callmacro Assert_rows 3
-->callmacro Assert_message Mysqlx.Resultset.FetchDoneMoreResultsets
-->callmacro Assert_message Mysqlx.Resultset.FetchDone
-->callmacro Assert_notice_session_state	ROWS_AFFECTED	v_unsigned_int:0
-->callmacro Assert_message Mysqlx.Sql.StmtExecuteOk
-->callmacro Deallocate_statement

-->callmacro Execute_procedure_no_output_params update_no_select()
-->callmacro Assert_notice_session_state	ROWS_AFFECTED	v_unsigned_int:2
-->callmacro Assert_message Mysqlx.Sql.StmtExecuteOk
-->callmacro Deallocate_statement

-->callmacro Execute_procedure_no_output_params update_and_select()
-->callmacro Assert_metadata UINT	'c1t1'
-->callmacro Assert_metadata BYTES	'c2t1'
-->callmacro Assert_rows 3
-->callmacro Assert_message Mysqlx.Resultset.FetchDoneMoreResultsets
-->callmacro Assert_message Mysqlx.Resultset.FetchDone
-->callmacro Assert_notice_session_state	ROWS_AFFECTED	v_unsigned_int:0
-->callmacro Assert_message Mysqlx.Sql.StmtExecuteOk
-->callmacro Deallocate_statement

-->callmacro Execute_procedure_no_output_params insert_no_select()
-->callmacro Assert_notice_session_state	ROWS_AFFECTED	v_unsigned_int:3
-->callmacro Assert_message Mysqlx.Sql.StmtExecuteOk
-->callmacro Deallocate_statement

-->callmacro Execute_procedure_no_output_params insert_and_select()
-->callmacro Assert_metadata UINT	'c1t1'
-->callmacro Assert_metadata BYTES	'c2t1'
-->callmacro Assert_rows 9
-->callmacro Assert_message Mysqlx.Resultset.FetchDoneMoreResultsets
-->callmacro Assert_message Mysqlx.Resultset.FetchDone
-->callmacro Assert_notice_session_state	ROWS_AFFECTED	v_unsigned_int:0
-->callmacro Assert_message Mysqlx.Sql.StmtExecuteOk
-->callmacro Deallocate_statement

# Check stored procedures with parameters (both local and output)
#
-->callmacro Execute_procedure_with_local_var out_param_no_select(10, @local_var)
-->callmacro Assert_notice_session_state	ROWS_AFFECTED	v_unsigned_int:0
-->callmacro Assert_message Mysqlx.Sql.StmtExecuteOk
-->callmacro Deallocate_statement

-->callmacro Execute_procedure_with_out_param out_param_no_select(10, ?)	@my_var
-->callmacro Assert_message Mysqlx.Resultset.FetchDoneMoreOutParams
-->callmacro Assert_metadata SINT	'res'
-->callmacro Assert_rows 1
-->callmacro Assert_message Mysqlx.Resultset.FetchDone
-->callmacro Assert_notice_session_state	ROWS_AFFECTED	v_unsigned_int:0
-->callmacro Assert_message Mysqlx.Sql.StmtExecuteOk
-->callmacro Deallocate_statement

-->callmacro Execute_procedure_with_local_var update_and_out_param(@local_var)
-->callmacro Assert_notice_session_state	ROWS_AFFECTED	v_unsigned_int:1
-->callmacro Assert_message Mysqlx.Sql.StmtExecuteOk
-->callmacro Assert_local_var
-->callmacro Deallocate_statement

-->callmacro Execute_procedure_with_out_param update_and_out_param(?)	@my_var
-->callmacro Assert_message Mysqlx.Resultset.FetchDoneMoreOutParams
-->callmacro Assert_metadata SINT	'max_val'
-->callmacro Assert_rows 1
-->callmacro Assert_message Mysqlx.Resultset.FetchDone
-->callmacro Assert_notice_session_state	ROWS_AFFECTED	v_unsigned_int:1
-->callmacro Assert_message Mysqlx.Sql.StmtExecuteOk
-->callmacro Deallocate_statement

-->callmacro Execute_procedure_with_local_var update_and_out_param_and_select(@local_var)
-->callmacro Assert_metadata DECIMAL	'AVG(c1t1)'
-->callmacro Assert_rows 1
-->callmacro Assert_message Mysqlx.Resultset.FetchDoneMoreResultsets
-->callmacro Assert_message Mysqlx.Resultset.FetchDone
-->callmacro Assert_notice_session_state	ROWS_AFFECTED	v_unsigned_int:0
-->callmacro Assert_message Mysqlx.Sql.StmtExecuteOk
-->callmacro Assert_local_var
-->callmacro Deallocate_statement

-->callmacro Execute_procedure_with_out_param update_and_out_param_and_select(?)	@my_var
-->callmacro Assert_metadata DECIMAL	'AVG(c1t1)'
-->callmacro Assert_rows 1
-->callmacro Assert_message Mysqlx.Resultset.FetchDoneMoreOutParams
-->callmacro Assert_metadata SINT	'max_val'
-->callmacro Assert_rows 1
-->callmacro Assert_message Mysqlx.Resultset.FetchDone
-->callmacro Assert_notice_session_state	ROWS_AFFECTED	v_unsigned_int:0
-->callmacro Assert_message Mysqlx.Sql.StmtExecuteOk
-->callmacro Deallocate_statement

-->callmacro Execute_procedure_with_local_var insert_and_out_param(@local_var)
-->callmacro Assert_notice_session_state	ROWS_AFFECTED	v_unsigned_int:1
-->callmacro Assert_message Mysqlx.Sql.StmtExecuteOk
-->callmacro Assert_local_var
-->callmacro Deallocate_statement

-->callmacro Execute_procedure_with_out_param insert_and_out_param(?)	@my_var
-->callmacro Assert_message Mysqlx.Resultset.FetchDoneMoreOutParams
-->callmacro Assert_metadata SINT	'max_val'
-->callmacro Assert_rows 1
-->callmacro Assert_message Mysqlx.Resultset.FetchDone
-->callmacro Assert_notice_session_state	ROWS_AFFECTED	v_unsigned_int:1
-->callmacro Assert_message Mysqlx.Sql.StmtExecuteOk
-->callmacro Deallocate_statement

-->callmacro Execute_procedure_with_local_var insert_and_out_param_and_select(@local_var)
-->callmacro Assert_metadata UINT	'MIN(c1t1)'
-->callmacro Assert_rows 1
-->callmacro Assert_message Mysqlx.Resultset.FetchDoneMoreResultsets
-->callmacro Assert_message Mysqlx.Resultset.FetchDone
-->callmacro Assert_notice_session_state	ROWS_AFFECTED	v_unsigned_int:0
-->callmacro Assert_message Mysqlx.Sql.StmtExecuteOk
-->callmacro Assert_local_var
-->callmacro Deallocate_statement

-->callmacro Execute_procedure_with_out_param insert_and_out_param_and_select(?)	@my_var
-->callmacro Assert_metadata UINT	'MIN(c1t1)'
-->callmacro Assert_rows 1
-->callmacro Assert_message Mysqlx.Resultset.FetchDoneMoreOutParams
-->callmacro Assert_metadata SINT	'max_val'
-->callmacro Assert_rows 1
-->callmacro Assert_message Mysqlx.Resultset.FetchDone
-->callmacro Assert_notice_session_state	ROWS_AFFECTED	v_unsigned_int:0
-->callmacro Assert_message Mysqlx.Sql.StmtExecuteOk
-->callmacro Deallocate_statement

# Check stored procedures with multiple output parameters
#
-->callmacro Execute_procedure_with_multiple_out_params out_params_with_select(?, ?)	@my_var	1@my_var2
-->callmacro Assert_metadata UINT	'c1t1'
-->callmacro Assert_metadata BYTES	'c2t1'
-->callmacro Assert_rows 13
-->callmacro Assert_message Mysqlx.Resultset.FetchDoneMoreOutParams
-->callmacro Assert_metadata DATETIME	'min_val'
-->callmacro Assert_metadata DATETIME	'max_val'
-->callmacro Assert_rows 1
-->callmacro Assert_message Mysqlx.Resultset.FetchDone
-->callmacro Assert_notice_session_state	ROWS_AFFECTED	v_unsigned_int:0
-->callmacro Assert_message Mysqlx.Sql.StmtExecuteOk
-->callmacro Deallocate_statement

# Check stored procedures with error conditions when there are no output
# parameters
#
-->callmacro Execute_procedure_no_output_params_with_error error()
-->callmacro Assert_message Mysqlx.Error
-->callmacro Show_warnings
-->callmacro Deallocate_statement

-->callmacro Execute_procedure_no_output_params_with_error with_warning()
-->callmacro Assert_message Mysqlx.Error
-->callmacro Show_warnings
-->callmacro Deallocate_statement

-->callmacro Execute_procedure_no_output_params_with_error select_and_error()
-->callmacro Assert_metadata UINT	'c1t1'
-->callmacro Assert_metadata BYTES	'c2t1'
-->callmacro Assert_rows 13
-->callmacro Assert_message Mysqlx.Resultset.FetchDoneMoreResultsets
-->callmacro Assert_message Mysqlx.Error
-->callmacro Show_warnings
-->callmacro Deallocate_statement

-->callmacro Execute_procedure_no_output_params_with_error select_and_warning()
-->callmacro Assert_metadata DATETIME	'c1t2'
-->callmacro Assert_rows 4
-->callmacro Assert_message Mysqlx.Resultset.FetchDoneMoreResultsets
-->callmacro Assert_message Mysqlx.Error
-->callmacro Show_warnings
-->callmacro Deallocate_statement

# Check stored procedures with error conditions with output parameters present
#
-->callmacro Execute_procedure_with_output_params_and_with_error out_param_and_error(?)	@my_var
-->callmacro Assert_message Mysqlx.Error
-->callmacro Show_warnings
-->callmacro Deallocate_statement

-->callmacro Execute_procedure_with_output_params_and_with_error select_out_param_and_error(?)	@my_var
-->callmacro Assert_metadata SINT	'COUNT(c1t1)'
-->callmacro Assert_rows 1
-->callmacro Assert_message Mysqlx.Resultset.FetchDoneMoreResultsets
-->callmacro Assert_message Mysqlx.Error
-->callmacro Show_warnings
-->callmacro Deallocate_statement

-->callmacro Execute_procedure_with_output_params_and_with_error out_param_and_warning(?)	@my_var
-->callmacro Assert_message Mysqlx.Error
-->callmacro Show_warnings
-->callmacro Deallocate_statement

-->callmacro Execute_procedure_with_output_params_and_with_error select_out_param_and_warning(?)	@my_var
-->callmacro Assert_metadata SINT	'COUNT(c1t1)'
-->callmacro Assert_rows 1
-->callmacro Assert_message Mysqlx.Resultset.FetchDoneMoreResultsets
-->callmacro Assert_message Mysqlx.Error
-->callmacro Show_warnings
-->callmacro Deallocate_statement

EOF

