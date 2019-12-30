
--write_file $MYSQL_TMP_DIR/resultset.xpl
-->import assert_messages.macro
-->import assert_notices.macro

-->varlet %ID% 1
-->macro Prepare_and_execute_wo_res	%SQL%
-->noquery_result
-->quiet
-->echo [PREPARE - %SQL%]
Mysqlx.Prepare.Prepare {
  stmt_id: %ID%
  stmt: {
    type: STMT
    stmt_execute: {
      stmt: "%SQL%"
    }
  }
}
-->recvok
-->echo [Execute]
Mysqlx.Prepare.Execute {
  stmt_id: %ID%
}
-->varinc %ID% 1
-->query_result
-->noquiet
-->endmacro

##
## Purpose of this test is:
#
# * validating that resultsets are in align to the UML flow:
#
#   ...
#   loop has more resultsets or not at end of fetch
#     group resultset
#       loop has more columns
#         server --> client: ColumnMetaData
#       end
#       loop has more rows
#         server --> client: Row
#       end
#     end
#     alt has more resultsets
#       server --> client: FetchDoneMoreResultsets
#     end
#   end
#   loop has more OUT-paramsets or not at end of fetch
#     server --> client: FetchDoneMoreOutParams
#     group resultset
#       loop has more columns
#         server --> client: ColumnMetaData
#       end
#       loop has more rows
#         server --> client: Row
#       end
#     end
#   end
#   alt at end of all resultsets
#     server --> client: FetchDone
#   else cursor is opened
#     server --> client: FetchSuspended
#   end
#   ...
#
# * expecting following notices for SQL prepared-statement:
#
# |Notice\Stmt            |Select |Insert |Update |Delete |
# |-----------------------|-------|-------|-------|-------|
# |ROWS_AFFECTED          |X      |X      |X      |X      |
# |PRODUCED_MESSAGE       |       |       |X      |       |
# |GENERATED_INSERT_ID    |       |X(PK)  |       |       |
# |GENERATED_DOCUMENT_IDS |       |       |       |       |
#
-->echo
-->echo
-->echo ## I. Validate simple resultsets
-->echo #
-->echo # 1. Assert resultset for delete queries
-->echo # 2. Assert resultset for update queries
-->echo # 3. Assert resultset for insert queries (no auto_inc column)
-->echo # 4. Assert resultset for insert queries (has auto_inc column)
-->echo # 5. Assert resultset for select queries
-->echo
-->echo ## II. Validate simple multi-resultsets
-->echo #
-->echo # 1. Assert two resultsets for select queries
-->echo # 2. Assert three resultsets for select queries
-->echo
-->echo ## III. Validate simple multi/resultsets with warnings
-->echo #
-->echo # 1. Assert one resultsets for select queries
-->echo # 2. Assert two resultsets for select queries

-->echo
-->echo #
-->echo # I.1
callmacro Prepare_and_execute_wo_res	DELETE FROM xtest.xtable WHERE prio=4 or prio=3;
callmacro Assert_notice_session_state	ROWS_AFFECTED	v_unsigned_int:2;
callmacro Assert_message Mysqlx.Sql.StmtExecuteOk;

-->echo
-->echo #
-->echo # I.2
callmacro Prepare_and_execute_wo_res	UPDATE xtest.xtable SET prio=prio-1;
callmacro Assert_notice_session_state	PRODUCED_MESSAGE	type: V_STRING;
callmacro Assert_notice_session_state	ROWS_AFFECTED	v_unsigned_int:3;
callmacro Assert_message Mysqlx.Sql.StmtExecuteOk;

-->echo
-->echo #
-->echo # I.3
callmacro Prepare_and_execute_wo_res	INSERT INTO xtest.xtable(phrase, prio) VALUE('Bob has a dog 2', 3);
callmacro Assert_notice_session_state	ROWS_AFFECTED	v_unsigned_int:1;
callmacro Assert_message Mysqlx.Sql.StmtExecuteOk;

-->echo
-->echo #
-->echo # I.4
callmacro Prepare_and_execute_wo_res	INSERT INTO xtest.xtable_with_pk(description) VALUE('MyCustomRow');
callmacro Assert_notice_session_state	ROWS_AFFECTED	v_unsigned_int:1;
callmacro Assert_notice_session_state	GENERATED_INSERT_ID	v_unsigned_int: 2;
callmacro Assert_message Mysqlx.Sql.StmtExecuteOk;

-->echo
-->echo #
-->echo # I.5
callmacro Prepare_and_execute_wo_res	SELECT phrase FROM xtest.xtable;
callmacro Assert_metadata	BYTES	"phrase";
callmacro Assert_rows	4;
callmacro Assert_message	Mysqlx.Resultset.FetchDone;
callmacro Assert_notice_session_state	ROWS_AFFECTED	v_unsigned_int:0;
callmacro Assert_message Mysqlx.Sql.StmtExecuteOk;


-->echo
-->echo #
-->echo # II.1
callmacro Prepare_and_execute_wo_res	CALL two_resultsets;
callmacro Assert_metadata	BYTES	"phrase";
callmacro Assert_rows	3;
callmacro Assert_message	Mysqlx.Resultset.FetchDoneMoreResultsets;
callmacro Assert_metadata	SINT	"prio";
callmacro Assert_rows	2;
callmacro Assert_message	Mysqlx.Resultset.FetchDoneMoreResultsets;
callmacro Assert_message	Mysqlx.Resultset.FetchDone;
callmacro Assert_notice_session_state	ROWS_AFFECTED	v_unsigned_int:0;
callmacro Assert_message Mysqlx.Sql.StmtExecuteOk;

-->echo
-->echo #
-->echo # II.2
callmacro Prepare_and_execute_wo_res	CALL three_resultsets;
callmacro Assert_metadata	BYTES	"phrase";
callmacro Assert_rows	3;
callmacro Assert_message	Mysqlx.Resultset.FetchDoneMoreResultsets;
callmacro Assert_metadata	SINT	"prio";
callmacro Assert_rows	2;
callmacro Assert_message	Mysqlx.Resultset.FetchDoneMoreResultsets;
callmacro Assert_metadata	SINT	"prio";
callmacro Assert_rows	3;
callmacro Assert_message	Mysqlx.Resultset.FetchDoneMoreResultsets;
callmacro Assert_message	Mysqlx.Resultset.FetchDone;
callmacro Assert_notice_session_state	ROWS_AFFECTED	v_unsigned_int:0;
callmacro Assert_message Mysqlx.Sql.StmtExecuteOk;


-->echo
-->echo #
-->echo # III.1
callmacro Prepare_and_execute_wo_res	SELECT 1/prio as col1 FROM xtest.xtable;
callmacro Assert_metadata	DECIMAL	"col1";
callmacro Assert_rows	4;
callmacro Assert_message	Mysqlx.Resultset.FetchDone;
## ER_DIVISION_BY_ZERO == 1365
callmacro Assert_notice_warning	WARNING	1365;
callmacro Assert_notice_warning	WARNING	1365;
callmacro Assert_notice_session_state	ROWS_AFFECTED	v_unsigned_int:0;
callmacro Assert_message Mysqlx.Sql.StmtExecuteOk;

-->echo
-->echo #
-->echo # III.2
callmacro Prepare_and_execute_wo_res	CALL two_resultsets_with_errors;
callmacro Assert_metadata	DECIMAL	"col1";
callmacro Assert_rows	4;
callmacro Assert_message	Mysqlx.Resultset.FetchDoneMoreResultsets;
callmacro Assert_metadata	DECIMAL	"col1";
callmacro Assert_rows	4;
callmacro Assert_message	Mysqlx.Resultset.FetchDoneMoreResultsets;
callmacro Assert_message	Mysqlx.Resultset.FetchDone;
## ER_DIVISION_BY_ZERO == 1365
callmacro Assert_notice_warning	WARNING	1365;
callmacro Assert_notice_warning	WARNING	1365;
callmacro Assert_notice_session_state	ROWS_AFFECTED	v_unsigned_int:0;
callmacro Assert_message Mysqlx.Sql.StmtExecuteOk;

EOF


source sample_tables.inc;

DELIMITER //;

CREATE PROCEDURE two_resultsets()
BEGIN
  SELECT phrase FROM xtable limit 3;
  SELECT prio FROM xtable limit 2;
END //

CREATE PROCEDURE two_resultsets_with_errors()
BEGIN
  SELECT 1/prio as col1 FROM xtable;
  SELECT 1/prio as col1 FROM xtable;
END //

CREATE PROCEDURE three_resultsets()
BEGIN
  SELECT phrase FROM xtable limit 3;
  SELECT prio FROM xtable limit 2;
  SELECT prio FROM xtable ORDER BY prio limit 3;
END //

DELIMITER ;//
