
############################################################
## Test verifies "--compression-mode" command line option:
#
# 1. Try to use any compression when server has enabled all algorithms
#    a. use "DISABLED" compression mode and verify that neither
#       client nor server uses it
#    b. use "REQUIRED" compression mode and verify that client and
#       server uses compression
#    c. use "PREFERRED" compression mode and verify that client and
#       server uses compression
# 2. Try to use any algorithm with, still server has disabled all algorithms
#    a. use "DISABLED" compression mode and verify that neither
#       client nor server uses it
#    b. use "REQUIRED" compression mode and verify that client can't
#       connect to the server
#    c. use "PREFERRED" compression mode and verify that client may
#       connect to the server, still can't use selected style
# 3. Try to use any algorithm when server has only "lz4" enabled
#    a. use "DISABLED" compression mode and verify that neither
#       client nor server uses it
#    b. use "REQUIRED" compression mode and verify that client and
#       server uses compression
#    c. use "PREFERRED" compression mode and verify that client and
#       server uses compression
# 4. Try to use any algorithm when server has only "deflate" enabled
#    a. use "DISABLED" compression mode and verify that neither
#       client nor server uses it
#    b. use "REQUIRED" compression mode and verify that client and
#       server uses compression
#    c. use "PREFERRED" compression mode and verify that client and
#       server uses compression
# 5. Try to use any algorithm when server has only "zstd" enabled
#    a. use "DISABLED" compression mode and verify that neither
#       client nor server uses it
#    b. use "REQUIRED" compression mode and verify that client and
#       server uses compression
#    c. use "PREFERRED" compression mode and verify that client and
#       server uses compression
# 6. Try to use "deflate" algorithm when server has only "lz4" enabled
#    a. use "DISABLED" compression mode and verify that neither
#       client nor server uses it
#    b. use "REQUIRED" compression mode and verify that client can't
#       connect to the server
#    c. use "PREFERRED" compression mode and verify that client may
#       connect to the server, still can't use selected style
# 7. Try to use "lz4" algorithm when server has only "deflate" enabled
#    a. use "DISABLED" compression mode and verify that neither
#       client nor server uses it
#    b. use "REQUIRED" compression mode and verify that client can't
#       connect to the server
#    c. use "PREFERRED" compression mode and verify that client may
#       connect to the server, still can't use selected style
# 8. Try to use "lz4" algorithm when server has only "zstd" enabled
#    a. use "DISABLED" compression mode and verify that neither
#       client nor server uses it
#    b. use "REQUIRED" compression mode and verify that client can't
#       connect to the server
#    c. use "PREFERRED" compression mode and verify that client may
#       connect to the server, still can't use selected style
# 9. Try to use "lz4" algorithm when server has only "lz4" enabled
#    a. use "DISABLED" compression mode and verify that neither
#       client nor server uses it
#    b. use "REQUIRED" compression mode and verify that client and
#       server uses compression
#    c. use "PREFERRED" compression mode and verify that client and
#       server uses compression
# 10. Try to use "deflate" algorithm when server has only "deflate" enabled
#    a. use "DISABLED" compression mode and verify that neither
#       client nor server uses it
#    b. use "REQUIRED" compression mode and verify that client and
#       server uses compression
#    c. use "PREFERRED" compression mode and verify that client and
#       server uses compression
# 11. Try to use "zstd" algorithm when server has only "zstd" enabled
#    a. use "DISABLED" compression mode and verify that neither
#       client nor server uses it
#    b. use "REQUIRED" compression mode and verify that client and
#       server uses compression
#    c. use "PREFERRED" compression mode and verify that client and
#       server uses compression

# 1.a
#
RUN SELECT "Send some data which might trigger server side compression" as Column1
Column1
Send some data which might trigger server side compression
0 rows affected
Got expected error: Compression is disabled or required compression style was not selected (code 2512)
Got expected error: Compression is disabled or required compression style was not selected (code 2512)
Got expected error: Compression is disabled or required compression style was not selected (code 2512)
Verify [Client handled Mysqlx.Connection.Compression message count is "==" 0]
Mysqlx.Ok {
  msg: "bye!"
}
ok

# 1.b
#
RUN SELECT "Send some data which might trigger server side compression" as Column1
Column1
Send some data which might trigger server side compression
0 rows affected
column1
some string value
0 rows affected
Verify [Client handled Mysqlx.Connection.Compression message count is ">" 0]
Mysqlx.Ok {
  msg: "bye!"
}
ok

# 1.c
#
RUN SELECT "Send some data which might trigger server side compression" as Column1
Column1
Send some data which might trigger server side compression
0 rows affected
column1
some string value
0 rows affected
Verify [Client handled Mysqlx.Connection.Compression message count is ">" 0]
Mysqlx.Ok {
  msg: "bye!"
}
ok

# 2.a
#
SET GLOBAL mysqlx_compression_algorithms="";
RUN SELECT "Send some data which might trigger server side compression" as Column1
Column1
Send some data which might trigger server side compression
0 rows affected
Got expected error: Compression is disabled or required compression style was not selected (code 2512)
Got expected error: Compression is disabled or required compression style was not selected (code 2512)
Got expected error: Compression is disabled or required compression style was not selected (code 2512)
Verify [Client handled Mysqlx.Connection.Compression message count is "==" 0]
Mysqlx.Ok {
  msg: "bye!"
}
ok

# 2.b
#
Application terminated with expected error: Client's requirement for compression configuration is not supported by server or it was disabled (code 2513)
ok

# 2.c
#
RUN SELECT "Send some data which might trigger server side compression" as Column1
Column1
Send some data which might trigger server side compression
0 rows affected
Got expected error: Compression is disabled or required compression style was not selected (code 2512)
Got expected error: Compression is disabled or required compression style was not selected (code 2512)
Got expected error: Compression is disabled or required compression style was not selected (code 2512)
Verify [Client handled Mysqlx.Connection.Compression message count is "==" 0]
Mysqlx.Ok {
  msg: "bye!"
}
ok

# 3.a
#
SET GLOBAL mysqlx_compression_algorithms="LZ4_MESSAGE";
RUN SELECT "Send some data which might trigger server side compression" as Column1
Column1
Send some data which might trigger server side compression
0 rows affected
Got expected error: Compression is disabled or required compression style was not selected (code 2512)
Got expected error: Compression is disabled or required compression style was not selected (code 2512)
Got expected error: Compression is disabled or required compression style was not selected (code 2512)
Verify [Client handled Mysqlx.Connection.Compression message count is "==" 0]
Mysqlx.Ok {
  msg: "bye!"
}
ok

# 3.b
#
RUN SELECT "Send some data which might trigger server side compression" as Column1
Column1
Send some data which might trigger server side compression
0 rows affected
column1
some string value
0 rows affected
Verify [Client handled Mysqlx.Connection.Compression message count is ">" 0]
Mysqlx.Ok {
  msg: "bye!"
}
ok

# 3.c
#
RUN SELECT "Send some data which might trigger server side compression" as Column1
Column1
Send some data which might trigger server side compression
0 rows affected
column1
some string value
0 rows affected
Verify [Client handled Mysqlx.Connection.Compression message count is ">" 0]
Mysqlx.Ok {
  msg: "bye!"
}
ok

# 4.a
#
SET GLOBAL mysqlx_compression_algorithms="DEFLATE_STREAM";
RUN SELECT "Send some data which might trigger server side compression" as Column1
Column1
Send some data which might trigger server side compression
0 rows affected
Got expected error: Compression is disabled or required compression style was not selected (code 2512)
Got expected error: Compression is disabled or required compression style was not selected (code 2512)
Got expected error: Compression is disabled or required compression style was not selected (code 2512)
Verify [Client handled Mysqlx.Connection.Compression message count is "==" 0]
Mysqlx.Ok {
  msg: "bye!"
}
ok

# 4.b
#
RUN SELECT "Send some data which might trigger server side compression" as Column1
Column1
Send some data which might trigger server side compression
0 rows affected
column1
some string value
0 rows affected
Verify [Client handled Mysqlx.Connection.Compression message count is ">" 0]
Mysqlx.Ok {
  msg: "bye!"
}
ok

# 4.c
#
RUN SELECT "Send some data which might trigger server side compression" as Column1
Column1
Send some data which might trigger server side compression
0 rows affected
column1
some string value
0 rows affected
Verify [Client handled Mysqlx.Connection.Compression message count is ">" 0]
Mysqlx.Ok {
  msg: "bye!"
}
ok

# 5.a
#
SET GLOBAL mysqlx_compression_algorithms="ZSTD_STREAM";
RUN SELECT "Send some data which might trigger server side compression" as Column1
Column1
Send some data which might trigger server side compression
0 rows affected
Got expected error: Compression is disabled or required compression style was not selected (code 2512)
Got expected error: Compression is disabled or required compression style was not selected (code 2512)
Got expected error: Compression is disabled or required compression style was not selected (code 2512)
Verify [Client handled Mysqlx.Connection.Compression message count is "==" 0]
Mysqlx.Ok {
  msg: "bye!"
}
ok

# 5.b
#
RUN SELECT "Send some data which might trigger server side compression" as Column1
Column1
Send some data which might trigger server side compression
0 rows affected
column1
some string value
0 rows affected
Verify [Client handled Mysqlx.Connection.Compression message count is ">" 0]
Mysqlx.Ok {
  msg: "bye!"
}
ok

# 5.c
#
RUN SELECT "Send some data which might trigger server side compression" as Column1
Column1
Send some data which might trigger server side compression
0 rows affected
column1
some string value
0 rows affected
Verify [Client handled Mysqlx.Connection.Compression message count is ">" 0]
Mysqlx.Ok {
  msg: "bye!"
}
ok

# 6.a
#
SET GLOBAL mysqlx_compression_algorithms="LZ4_MESSAGE";
RUN SELECT "Send some data which might trigger server side compression" as Column1
Column1
Send some data which might trigger server side compression
0 rows affected
Got expected error: Compression is disabled or required compression style was not selected (code 2512)
Got expected error: Compression is disabled or required compression style was not selected (code 2512)
Got expected error: Compression is disabled or required compression style was not selected (code 2512)
Verify [Client handled Mysqlx.Connection.Compression message count is "==" 0]
Mysqlx.Ok {
  msg: "bye!"
}
ok

# 6.b
#
Application terminated with expected error: Client's requirement for compression configuration is not supported by server or it was disabled (code 2513)
ok

# 6.c
#
RUN SELECT "Send some data which might trigger server side compression" as Column1
Column1
Send some data which might trigger server side compression
0 rows affected
Got expected error: Compression is disabled or required compression style was not selected (code 2512)
Got expected error: Compression is disabled or required compression style was not selected (code 2512)
Got expected error: Compression is disabled or required compression style was not selected (code 2512)
Verify [Client handled Mysqlx.Connection.Compression message count is "==" 0]
Mysqlx.Ok {
  msg: "bye!"
}
ok

# 7.a
#
SET GLOBAL mysqlx_compression_algorithms="DEFLATE_STREAM";
RUN SELECT "Send some data which might trigger server side compression" as Column1
Column1
Send some data which might trigger server side compression
0 rows affected
Got expected error: Compression is disabled or required compression style was not selected (code 2512)
Got expected error: Compression is disabled or required compression style was not selected (code 2512)
Got expected error: Compression is disabled or required compression style was not selected (code 2512)
Verify [Client handled Mysqlx.Connection.Compression message count is "==" 0]
Mysqlx.Ok {
  msg: "bye!"
}
ok

# 7.b
#
Application terminated with expected error: Client's requirement for compression configuration is not supported by server or it was disabled (code 2513)
ok

# 7.c
#
RUN SELECT "Send some data which might trigger server side compression" as Column1
Column1
Send some data which might trigger server side compression
0 rows affected
Got expected error: Compression is disabled or required compression style was not selected (code 2512)
Got expected error: Compression is disabled or required compression style was not selected (code 2512)
Got expected error: Compression is disabled or required compression style was not selected (code 2512)
Verify [Client handled Mysqlx.Connection.Compression message count is "==" 0]
Mysqlx.Ok {
  msg: "bye!"
}
ok

# 8.a
#
SET GLOBAL mysqlx_compression_algorithms="ZSTD_STREAM";
RUN SELECT "Send some data which might trigger server side compression" as Column1
Column1
Send some data which might trigger server side compression
0 rows affected
Got expected error: Compression is disabled or required compression style was not selected (code 2512)
Got expected error: Compression is disabled or required compression style was not selected (code 2512)
Got expected error: Compression is disabled or required compression style was not selected (code 2512)
Verify [Client handled Mysqlx.Connection.Compression message count is "==" 0]
Mysqlx.Ok {
  msg: "bye!"
}
ok

# 8.b
#
Application terminated with expected error: Client's requirement for compression configuration is not supported by server or it was disabled (code 2513)
ok

# 8.c
#
RUN SELECT "Send some data which might trigger server side compression" as Column1
Column1
Send some data which might trigger server side compression
0 rows affected
Got expected error: Compression is disabled or required compression style was not selected (code 2512)
Got expected error: Compression is disabled or required compression style was not selected (code 2512)
Got expected error: Compression is disabled or required compression style was not selected (code 2512)
Verify [Client handled Mysqlx.Connection.Compression message count is "==" 0]
Mysqlx.Ok {
  msg: "bye!"
}
ok

# 9.a
#
SET GLOBAL mysqlx_compression_algorithms="LZ4_MESSAGE";
RUN SELECT "Send some data which might trigger server side compression" as Column1
Column1
Send some data which might trigger server side compression
0 rows affected
Got expected error: Compression is disabled or required compression style was not selected (code 2512)
Got expected error: Compression is disabled or required compression style was not selected (code 2512)
Got expected error: Compression is disabled or required compression style was not selected (code 2512)
Verify [Client handled Mysqlx.Connection.Compression message count is "==" 0]
Mysqlx.Ok {
  msg: "bye!"
}
ok

# 9.b
#
RUN SELECT "Send some data which might trigger server side compression" as Column1
Column1
Send some data which might trigger server side compression
0 rows affected
column1
some string value
0 rows affected
Verify [Client handled Mysqlx.Connection.Compression message count is ">" 0]
Mysqlx.Ok {
  msg: "bye!"
}
ok

# 9.c
#
RUN SELECT "Send some data which might trigger server side compression" as Column1
Column1
Send some data which might trigger server side compression
0 rows affected
column1
some string value
0 rows affected
Verify [Client handled Mysqlx.Connection.Compression message count is ">" 0]
Mysqlx.Ok {
  msg: "bye!"
}
ok

# 10.a
#
SET GLOBAL mysqlx_compression_algorithms="LZ4_MESSAGE";
RUN SELECT "Send some data which might trigger server side compression" as Column1
Column1
Send some data which might trigger server side compression
0 rows affected
Got expected error: Compression is disabled or required compression style was not selected (code 2512)
Got expected error: Compression is disabled or required compression style was not selected (code 2512)
Got expected error: Compression is disabled or required compression style was not selected (code 2512)
Verify [Client handled Mysqlx.Connection.Compression message count is "==" 0]
Mysqlx.Ok {
  msg: "bye!"
}
ok

# 7.b
#
RUN SELECT "Send some data which might trigger server side compression" as Column1
Column1
Send some data which might trigger server side compression
0 rows affected
column1
some string value
0 rows affected
Verify [Client handled Mysqlx.Connection.Compression message count is ">" 0]
Mysqlx.Ok {
  msg: "bye!"
}
ok

# 7.c
#
RUN SELECT "Send some data which might trigger server side compression" as Column1
Column1
Send some data which might trigger server side compression
0 rows affected
column1
some string value
0 rows affected
Verify [Client handled Mysqlx.Connection.Compression message count is ">" 0]
Mysqlx.Ok {
  msg: "bye!"
}
ok

# 8.a
#
SET GLOBAL mysqlx_compression_algorithms="DEFLATE_STREAM";
RUN SELECT "Send some data which might trigger server side compression" as Column1
Column1
Send some data which might trigger server side compression
0 rows affected
Got expected error: Compression is disabled or required compression style was not selected (code 2512)
Got expected error: Compression is disabled or required compression style was not selected (code 2512)
Got expected error: Compression is disabled or required compression style was not selected (code 2512)
Verify [Client handled Mysqlx.Connection.Compression message count is "==" 0]
Mysqlx.Ok {
  msg: "bye!"
}
ok

# 10.b
#
RUN SELECT "Send some data which might trigger server side compression" as Column1
Column1
Send some data which might trigger server side compression
0 rows affected
column1
some string value
0 rows affected
Verify [Client handled Mysqlx.Connection.Compression message count is ">" 0]
Mysqlx.Ok {
  msg: "bye!"
}
ok

# 10.c
#
RUN SELECT "Send some data which might trigger server side compression" as Column1
Column1
Send some data which might trigger server side compression
0 rows affected
column1
some string value
0 rows affected
Verify [Client handled Mysqlx.Connection.Compression message count is ">" 0]
Mysqlx.Ok {
  msg: "bye!"
}
ok

# 11.a
#
SET GLOBAL mysqlx_compression_algorithms="ZSTD_STREAM";
RUN SELECT "Send some data which might trigger server side compression" as Column1
Column1
Send some data which might trigger server side compression
0 rows affected
Got expected error: Compression is disabled or required compression style was not selected (code 2512)
Got expected error: Compression is disabled or required compression style was not selected (code 2512)
Got expected error: Compression is disabled or required compression style was not selected (code 2512)
Verify [Client handled Mysqlx.Connection.Compression message count is "==" 0]
Mysqlx.Ok {
  msg: "bye!"
}
ok

# 11.b
#
RUN SELECT "Send some data which might trigger server side compression" as Column1
Column1
Send some data which might trigger server side compression
0 rows affected
column1
some string value
0 rows affected
Verify [Client handled Mysqlx.Connection.Compression message count is ">" 0]
Mysqlx.Ok {
  msg: "bye!"
}
ok

# 11.c
#
RUN SELECT "Send some data which might trigger server side compression" as Column1
Column1
Send some data which might trigger server side compression
0 rows affected
column1
some string value
0 rows affected
Verify [Client handled Mysqlx.Connection.Compression message count is ">" 0]
Mysqlx.Ok {
  msg: "bye!"
}
ok
SET GLOBAL mysqlx_compression_algorithms=DEFAULT;
