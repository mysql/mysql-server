## Test to verifies compression level functionality from the server side
#

--write_file $MYSQL_TMP_DIR/compression_level_server.xpl
-->import assert_status_variable.macro
-->quiet
-->title =Verify %OPTION_COMPRESSION_ALGORITHM% at level %OPTION_COMPRESSION_LEVEL%

-->callmacro Assert_status_variable	Mysqlx_compression_algorithm	%OPTION_COMPRESSION_ALGORITHM%
-->callmacro Assert_status_variable	Mysqlx_compression_level	%OPTION_COMPRESSION_LEVEL%

-->noquery_result
SELECT * FROM xtest.data;
-->query_result

-->callmacro Assert_status_variable	Mysqlx_bytes_received_compressed_payload	0

-->stmtsql SHOW STATUS LIKE 'Mysqlx_bytes_sent_uncompressed_frame'
-->recvtovar %UNCOMPRESSED_BYTES% Value
-->stmtsql SHOW STATUS LIKE 'Mysqlx_bytes_sent_compressed_payload'
-->recvtovar %COMPRESSED_BYTES% Value
-->echo Verify [Status variable "Mysqlx_bytes_sent_compressed_payload" needs to be lower than "Mysqlx_bytes_sent_uncompressed_frame"]
-->assert_gt %UNCOMPRESSED_BYTES%	%COMPRESSED_BYTES%

EOF

SET GLOBAL mysqlx_compression_algorithms='DEFLATE_STREAM,LZ4_MESSAGE,ZSTD_STREAM';

CREATE SCHEMA xtest;
CREATE TABLE xtest.data(val VARCHAR(256));

# The data for reciving
--disable_query_log
let $row=512;
while ($row)
{
  eval INSERT INTO xtest.data VALUES("Example data for testing compression level");
  dec $row;
}
--enable_query_log

