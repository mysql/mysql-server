## Test to verifies compression level ajustment on the server side
#


## Test starts here
--write_file $MYSQL_TMP_DIR/compression_level_adjustment.xpl
-->import assert_status_variable.macro
-->quiet
-->title =Compression level %OPTION_COMPRESSION_LEVEL% adjusted to %EXPECT_LEVEL% for %OPTION_COMPRESSION_ALGORITHM%

-->callmacro Assert_status_variable	Mysqlx_compression_algorithm	%OPTION_COMPRESSION_ALGORITHM%
-->callmacro Assert_status_variable	Mysqlx_compression_level	%EXPECT_LEVEL%

EOF

SET GLOBAL mysqlx_compression_algorithms='DEFLATE_STREAM,LZ4_MESSAGE,ZSTD_STREAM';

