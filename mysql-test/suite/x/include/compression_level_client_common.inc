## Test to verifies compression level functionality from the client side
#

--write_file $MYSQL_TMP_DIR/compression_level_client.xpl
-->import assert_status_variable.macro
-->quiet
-->title =Verify %OPTION_COMPRESSION_ALGORITHM% at level %OPTION_COMPRESSION_LEVEL%

-->callmacro Assert_status_variable	Mysqlx_compression_level	%EXPECT_SERVER_LEVEL%

-->repeat 32
-->begin_compress GROUP
Mysqlx.Sql.StmtExecute {
  stmt: "SELECT 1 /* Example data for testing compression level */"
}
Mysqlx.Sql.StmtExecute {
  stmt: "SELECT 2 /* Example data for testing compression level */"
}
Mysqlx.Sql.StmtExecute {
  stmt: "SELECT 3 /* Example data for testing compression level */"
}
Mysqlx.Sql.StmtExecute {
  stmt: "SELECT 4 /* Example data for testing compression level */"
}
Mysqlx.Sql.StmtExecute {
  stmt: "SELECT 5 /* Example data for testing compression level */"
}
Mysqlx.Sql.StmtExecute {
  stmt: "SELECT 6 /* Example data for testing compression level */"
}
Mysqlx.Sql.StmtExecute {
  stmt: "SELECT 7 /* Example data for testing compression level */"
}
-->end_compress

-->recvresult be-quiet
-->recvresult be-quiet
-->recvresult be-quiet
-->recvresult be-quiet
-->recvresult be-quiet
-->recvresult be-quiet
-->recvresult be-quiet
-->endrepeat

-->stmtsql SHOW STATUS LIKE 'Mysqlx_bytes_received_uncompressed_frame'
-->recvtovar %UNCOMPRESSED_BYTES% Value
-->stmtsql SHOW STATUS LIKE 'Mysqlx_bytes_received_compressed_payload'
-->recvtovar %COMPRESSED_BYTES% Value
-->echo Verify [Status variable "Mysqlx_bytes_received_compressed_payload" needs to be lower than "Mysqlx_bytes_received_uncompressed_frame"]
-->assert_gt %UNCOMPRESSED_BYTES%	%COMPRESSED_BYTES%

EOF

CREATE SCHEMA xtest;
CREATE TABLE xtest.data(val VARCHAR(256));

SET GLOBAL mysqlx_compression_algorithms='DEFLATE_STREAM,LZ4_MESSAGE,ZSTD_STREAM';

