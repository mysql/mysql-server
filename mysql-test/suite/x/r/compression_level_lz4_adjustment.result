SET GLOBAL mysqlx_compression_algorithms='DEFLATE_STREAM,LZ4_MESSAGE,ZSTD_STREAM';
SET GLOBAL mysqlx_lz4_default_compression_level=4;
SET GLOBAL mysqlx_lz4_max_client_compression_level=16;

Compression level DEFAULT adjusted to 4 for LZ4_MESSAGE
=======================================================
Verify [Status variable "Mysqlx_compression_algorithm" needs to have a value of LZ4_MESSAGE]
Verify [Status variable "Mysqlx_compression_level" needs to have a value of 4]
Mysqlx.Ok {
  msg: "bye!"
}
ok
SET GLOBAL mysqlx_lz4_default_compression_level=4;
SET GLOBAL mysqlx_lz4_max_client_compression_level=16;

Compression level -100 adjusted to 0 for LZ4_MESSAGE
====================================================
Verify [Status variable "Mysqlx_compression_algorithm" needs to have a value of LZ4_MESSAGE]
Verify [Status variable "Mysqlx_compression_level" needs to have a value of 0]
Mysqlx.Ok {
  msg: "bye!"
}
ok
SET GLOBAL mysqlx_lz4_default_compression_level=4;
SET GLOBAL mysqlx_lz4_max_client_compression_level=DEFAULT;

Compression level 100 adjusted to 8 for LZ4_MESSAGE
===================================================
Verify [Status variable "Mysqlx_compression_algorithm" needs to have a value of LZ4_MESSAGE]
Verify [Status variable "Mysqlx_compression_level" needs to have a value of 8]
Mysqlx.Ok {
  msg: "bye!"
}
ok
SET GLOBAL mysqlx_lz4_default_compression_level=4;
SET GLOBAL mysqlx_lz4_max_client_compression_level=16;

Compression level 100 adjusted to 16 for LZ4_MESSAGE
====================================================
Verify [Status variable "Mysqlx_compression_algorithm" needs to have a value of LZ4_MESSAGE]
Verify [Status variable "Mysqlx_compression_level" needs to have a value of 16]
Mysqlx.Ok {
  msg: "bye!"
}
ok
SET GLOBAL mysqlx_lz4_default_compression_level=4;
SET GLOBAL mysqlx_lz4_max_client_compression_level=3;

Compression level DEFAULT adjusted to 3 for LZ4_MESSAGE
=======================================================
Verify [Status variable "Mysqlx_compression_algorithm" needs to have a value of LZ4_MESSAGE]
Verify [Status variable "Mysqlx_compression_level" needs to have a value of 3]
Mysqlx.Ok {
  msg: "bye!"
}
ok
SET GLOBAL mysqlx_lz4_max_client_compression_level=DEFAULT;
SET GLOBAL mysqlx_lz4_default_compression_level=DEFAULT;
SET GLOBAL mysqlx_compression_algorithms=DEFAULT;
