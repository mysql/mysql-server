SET GLOBAL mysqlx_compression_algorithms='DEFLATE_STREAM,LZ4_MESSAGE,ZSTD_STREAM';
SET GLOBAL mysqlx_deflate_default_compression_level=4;
SET GLOBAL mysqlx_deflate_max_client_compression_level=5;

Compression level DEFAULT adjusted to 4 for DEFLATE_STREAM
==========================================================
Verify [Status variable "Mysqlx_compression_algorithm" needs to have a value of DEFLATE_STREAM]
Verify [Status variable "Mysqlx_compression_level" needs to have a value of 4]
Mysqlx.Ok {
  msg: "bye!"
}
ok
SET GLOBAL mysqlx_deflate_default_compression_level=4;
SET GLOBAL mysqlx_deflate_max_client_compression_level=5;

Compression level -100 adjusted to 1 for DEFLATE_STREAM
=======================================================
Verify [Status variable "Mysqlx_compression_algorithm" needs to have a value of DEFLATE_STREAM]
Verify [Status variable "Mysqlx_compression_level" needs to have a value of 1]
Mysqlx.Ok {
  msg: "bye!"
}
ok
SET GLOBAL mysqlx_deflate_default_compression_level=4;
SET GLOBAL mysqlx_deflate_max_client_compression_level=DEFAULT;

Compression level 100 adjusted to 5 for DEFLATE_STREAM
======================================================
Verify [Status variable "Mysqlx_compression_algorithm" needs to have a value of DEFLATE_STREAM]
Verify [Status variable "Mysqlx_compression_level" needs to have a value of 5]
Mysqlx.Ok {
  msg: "bye!"
}
ok
SET GLOBAL mysqlx_deflate_default_compression_level=4;
SET GLOBAL mysqlx_deflate_max_client_compression_level=9;

Compression level 100 adjusted to 9 for DEFLATE_STREAM
======================================================
Verify [Status variable "Mysqlx_compression_algorithm" needs to have a value of DEFLATE_STREAM]
Verify [Status variable "Mysqlx_compression_level" needs to have a value of 9]
Mysqlx.Ok {
  msg: "bye!"
}
ok
SET GLOBAL mysqlx_deflate_default_compression_level=5;
SET GLOBAL mysqlx_deflate_max_client_compression_level=3;

Compression level DEFAULT adjusted to 3 for DEFLATE_STREAM
==========================================================
Verify [Status variable "Mysqlx_compression_algorithm" needs to have a value of DEFLATE_STREAM]
Verify [Status variable "Mysqlx_compression_level" needs to have a value of 3]
Mysqlx.Ok {
  msg: "bye!"
}
ok
SET GLOBAL mysqlx_deflate_max_client_compression_level=DEFAULT;
SET GLOBAL mysqlx_deflate_default_compression_level=DEFAULT;
SET GLOBAL mysqlx_compression_algorithms=DEFAULT;
