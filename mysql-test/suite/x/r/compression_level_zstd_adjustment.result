SET GLOBAL mysqlx_compression_algorithms='DEFLATE_STREAM,LZ4_MESSAGE,ZSTD_STREAM';
SET GLOBAL mysqlx_zstd_default_compression_level=4;
SET GLOBAL mysqlx_zstd_max_client_compression_level=22;

Compression level DEFAULT adjusted to 4 for ZSTD_STREAM
=======================================================
Verify [Status variable "Mysqlx_compression_algorithm" needs to have a value of ZSTD_STREAM]
Verify [Status variable "Mysqlx_compression_level" needs to have a value of 4]
Mysqlx.Ok {
  msg: "bye!"
}
ok
SET GLOBAL mysqlx_zstd_default_compression_level=4;
SET GLOBAL mysqlx_zstd_max_client_compression_level=22;

Compression level -150000 adjusted to -131072 for ZSTD_STREAM
=============================================================
Verify [Status variable "Mysqlx_compression_algorithm" needs to have a value of ZSTD_STREAM]
Verify [Status variable "Mysqlx_compression_level" needs to have a value of -131072]
Mysqlx.Ok {
  msg: "bye!"
}
ok
SET GLOBAL mysqlx_zstd_default_compression_level=4;
SET GLOBAL mysqlx_zstd_max_client_compression_level=DEFAULT;

Compression level 100 adjusted to 11 for ZSTD_STREAM
====================================================
Verify [Status variable "Mysqlx_compression_algorithm" needs to have a value of ZSTD_STREAM]
Verify [Status variable "Mysqlx_compression_level" needs to have a value of 11]
Mysqlx.Ok {
  msg: "bye!"
}
ok
SET GLOBAL mysqlx_zstd_default_compression_level=4;
SET GLOBAL mysqlx_zstd_max_client_compression_level=22;

Compression level 100 adjusted to 22 for ZSTD_STREAM
====================================================
Verify [Status variable "Mysqlx_compression_algorithm" needs to have a value of ZSTD_STREAM]
Verify [Status variable "Mysqlx_compression_level" needs to have a value of 22]
Mysqlx.Ok {
  msg: "bye!"
}
ok
SET GLOBAL mysqlx_zstd_default_compression_level=10;
SET GLOBAL mysqlx_zstd_max_client_compression_level=5;

Compression level DEFAULT adjusted to 5 for ZSTD_STREAM
=======================================================
Verify [Status variable "Mysqlx_compression_algorithm" needs to have a value of ZSTD_STREAM]
Verify [Status variable "Mysqlx_compression_level" needs to have a value of 5]
Mysqlx.Ok {
  msg: "bye!"
}
ok
SET GLOBAL mysqlx_zstd_default_compression_level=10;
SET GLOBAL mysqlx_zstd_max_client_compression_level=5;

Compression level 0 adjusted to 1 for ZSTD_STREAM
=================================================
Verify [Status variable "Mysqlx_compression_algorithm" needs to have a value of ZSTD_STREAM]
Verify [Status variable "Mysqlx_compression_level" needs to have a value of 1]
Mysqlx.Ok {
  msg: "bye!"
}
ok
SET GLOBAL mysqlx_zstd_default_compression_level=10;
SET GLOBAL mysqlx_zstd_max_client_compression_level=-1;

Compression level 0 adjusted to -1 for ZSTD_STREAM
==================================================
Verify [Status variable "Mysqlx_compression_algorithm" needs to have a value of ZSTD_STREAM]
Verify [Status variable "Mysqlx_compression_level" needs to have a value of -1]
Mysqlx.Ok {
  msg: "bye!"
}
ok
SET GLOBAL mysqlx_zstd_max_client_compression_level=DEFAULT;
SET GLOBAL mysqlx_zstd_default_compression_level=DEFAULT;
SET GLOBAL mysqlx_compression_algorithms=DEFAULT;
