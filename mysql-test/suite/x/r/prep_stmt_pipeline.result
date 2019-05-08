
## 1. Verify pipelining of single prepare-execute request
#   a. prepare-execute
#   b. prepare-execute-cursor_open
## 2. Verify pipelining of mixed prepare-execute requests
#   a. prepare1-preapre2-execute1-execute2
#   b. prepare1-preapre2-execute1-deallocate1-execute2-deallocate2

 1.a. Verify pipelining of single prepare-execute request (prepeare-execute)
============================================================================
# Pipeline Mysqlx.Prepare.Prepare
# Pipeline Mysqlx.Prepare.Execute
# Receive Mysqlx.Prepare.Prepare
RUN recvok
# Receive Mysqlx.Prepare.Execute
1
1
0 rows affected

 1.b. Verify pipelining of single prepare-execute request (prepeare-execute-cursor_open)
========================================================================================
# Pipeline Mysqlx.Prepare.Prepare
# Pipeline Mysqlx.Prepare.Execute
# Pipeline Mysqlx.Cursor.Open
# Receive Mysqlx.Prepare.Prepare
RUN recvok
# Receive Mysqlx.Prepare.Execute
2
2
0 rows affected
# Receive Mysqlx.Cursor.Open
2
command ok

 2.a. Verify pipelining of mixed prepare-execute request (prepeare1-prepeare2-execute1-execute2)
================================================================================================
# Pipeline Mysqlx.Prepare.Prepare
# Pipeline Mysqlx.Prepare.Prepare
# Pipeline Mysqlx.Prepare.Execute
# Pipeline Mysqlx.Prepare.Execute
# Receive Mysqlx.Prepare.Prepare
RUN recvok
# Receive Mysqlx.Prepare.Prepare
RUN recvok
# Receive Mysqlx.Prepare.Execute
3
3
0 rows affected
# Receive Mysqlx.Prepare.Execute
4
4
0 rows affected

 2.b. Verify pipelining of mixed prepare-execute request (prepeare1-prepeare2-execute1-deallocate1-execute2-deallocate2)
========================================================================================================================
# Pipeline Mysqlx.Prepare.Prepare
# Pipeline Mysqlx.Prepare.Prepare
# Pipeline Mysqlx.Prepare.Execute
# Pipeline Mysqlx.Prepare.Deallocate
# Pipeline Mysqlx.Prepare.Execute
# Pipeline Mysqlx.Prepare.Deallocate
# Receive Mysqlx.Prepare.Prepare
RUN recvok
# Receive Mysqlx.Prepare.Prepare
RUN recvok
# Receive Mysqlx.Prepare.Execute
5
5
0 rows affected
# Receive Mysqlx.Prepare.Deallocate
RUN recvok
# Receive Mysqlx.Prepare.Execute
6
6
0 rows affected
# Receive Mysqlx.Prepare.Deallocate
RUN recvok
Mysqlx.Ok {
  msg: "bye!"
}
ok
