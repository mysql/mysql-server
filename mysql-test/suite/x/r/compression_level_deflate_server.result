SET GLOBAL mysqlx_compression_algorithms='DEFLATE_STREAM,LZ4_MESSAGE,ZSTD_STREAM';
CREATE SCHEMA xtest;
CREATE TABLE xtest.data(val VARCHAR(256));
SET GLOBAL mysqlx_deflate_max_client_compression_level=9;

Verify DEFLATE_STREAM at level 1
================================
Verify [Status variable "Mysqlx_compression_algorithm" needs to have a value of DEFLATE_STREAM]
Verify [Status variable "Mysqlx_compression_level" needs to have a value of 1]
Verify [Status variable "Mysqlx_bytes_received_compressed_payload" needs to have a value of 0]
Verify [Status variable "Mysqlx_bytes_sent_compressed_payload" needs to be lower than "Mysqlx_bytes_sent_uncompressed_frame"]
Mysqlx.Ok {
  msg: "bye!"
}
ok

Verify DEFLATE_STREAM at level 2
================================
Verify [Status variable "Mysqlx_compression_algorithm" needs to have a value of DEFLATE_STREAM]
Verify [Status variable "Mysqlx_compression_level" needs to have a value of 2]
Verify [Status variable "Mysqlx_bytes_received_compressed_payload" needs to have a value of 0]
Verify [Status variable "Mysqlx_bytes_sent_compressed_payload" needs to be lower than "Mysqlx_bytes_sent_uncompressed_frame"]
Mysqlx.Ok {
  msg: "bye!"
}
ok

Verify DEFLATE_STREAM at level 3
================================
Verify [Status variable "Mysqlx_compression_algorithm" needs to have a value of DEFLATE_STREAM]
Verify [Status variable "Mysqlx_compression_level" needs to have a value of 3]
Verify [Status variable "Mysqlx_bytes_received_compressed_payload" needs to have a value of 0]
Verify [Status variable "Mysqlx_bytes_sent_compressed_payload" needs to be lower than "Mysqlx_bytes_sent_uncompressed_frame"]
Mysqlx.Ok {
  msg: "bye!"
}
ok

Verify DEFLATE_STREAM at level 4
================================
Verify [Status variable "Mysqlx_compression_algorithm" needs to have a value of DEFLATE_STREAM]
Verify [Status variable "Mysqlx_compression_level" needs to have a value of 4]
Verify [Status variable "Mysqlx_bytes_received_compressed_payload" needs to have a value of 0]
Verify [Status variable "Mysqlx_bytes_sent_compressed_payload" needs to be lower than "Mysqlx_bytes_sent_uncompressed_frame"]
Mysqlx.Ok {
  msg: "bye!"
}
ok

Verify DEFLATE_STREAM at level 5
================================
Verify [Status variable "Mysqlx_compression_algorithm" needs to have a value of DEFLATE_STREAM]
Verify [Status variable "Mysqlx_compression_level" needs to have a value of 5]
Verify [Status variable "Mysqlx_bytes_received_compressed_payload" needs to have a value of 0]
Verify [Status variable "Mysqlx_bytes_sent_compressed_payload" needs to be lower than "Mysqlx_bytes_sent_uncompressed_frame"]
Mysqlx.Ok {
  msg: "bye!"
}
ok

Verify DEFLATE_STREAM at level 6
================================
Verify [Status variable "Mysqlx_compression_algorithm" needs to have a value of DEFLATE_STREAM]
Verify [Status variable "Mysqlx_compression_level" needs to have a value of 6]
Verify [Status variable "Mysqlx_bytes_received_compressed_payload" needs to have a value of 0]
Verify [Status variable "Mysqlx_bytes_sent_compressed_payload" needs to be lower than "Mysqlx_bytes_sent_uncompressed_frame"]
Mysqlx.Ok {
  msg: "bye!"
}
ok

Verify DEFLATE_STREAM at level 7
================================
Verify [Status variable "Mysqlx_compression_algorithm" needs to have a value of DEFLATE_STREAM]
Verify [Status variable "Mysqlx_compression_level" needs to have a value of 7]
Verify [Status variable "Mysqlx_bytes_received_compressed_payload" needs to have a value of 0]
Verify [Status variable "Mysqlx_bytes_sent_compressed_payload" needs to be lower than "Mysqlx_bytes_sent_uncompressed_frame"]
Mysqlx.Ok {
  msg: "bye!"
}
ok

Verify DEFLATE_STREAM at level 8
================================
Verify [Status variable "Mysqlx_compression_algorithm" needs to have a value of DEFLATE_STREAM]
Verify [Status variable "Mysqlx_compression_level" needs to have a value of 8]
Verify [Status variable "Mysqlx_bytes_received_compressed_payload" needs to have a value of 0]
Verify [Status variable "Mysqlx_bytes_sent_compressed_payload" needs to be lower than "Mysqlx_bytes_sent_uncompressed_frame"]
Mysqlx.Ok {
  msg: "bye!"
}
ok

Verify DEFLATE_STREAM at level 9
================================
Verify [Status variable "Mysqlx_compression_algorithm" needs to have a value of DEFLATE_STREAM]
Verify [Status variable "Mysqlx_compression_level" needs to have a value of 9]
Verify [Status variable "Mysqlx_bytes_received_compressed_payload" needs to have a value of 0]
Verify [Status variable "Mysqlx_bytes_sent_compressed_payload" needs to be lower than "Mysqlx_bytes_sent_uncompressed_frame"]
Mysqlx.Ok {
  msg: "bye!"
}
ok
SET GLOBAL mysqlx_deflate_max_client_compression_level=DEFAULT;
SET GLOBAL mysqlx_compression_algorithms=DEFAULT;
