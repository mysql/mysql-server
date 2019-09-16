
## A. Execute the test using zlib/single
#


## I. Compression and Open expectation block with no-error condition
#
# 1. Fail the expectation block and check that each message that
#    was compressed is replied with error without executing it
# 2. Close expectation block and check that compressed messages
#    are correctly decompressed (context in sync) and executed

#
# I.1
send Mysqlx.Expect.Open {
  cond {
    condition_key: 1
  }
}

RUN recvok
RUN SELECT invalid_syntax(-)
While executing SELECT invalid_syntax(-):
Got expected error: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near ')' at line 1 (code 1064)
Got expected error: Expectation failed: no_error (code 5159)
Got expected error: Expectation failed: no_error (code 5159)
Got expected error: Expectation failed: no_error (code 5159)

#
# I.2
send Mysqlx.Expect.Close {
}

Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 5159
  msg: "Expectation failed: no_error"
  sql_state: "HY000"
}

First query
First query
0 rows affected
Got expected error (code 0)
Second query
Second query
0 rows affected
Got expected error (code 0)
Last query
Last query
0 rows affected
Got expected error (code 0)
Mysqlx.Ok {
  msg: "bye!"
}
ok

## B. Execute the test using lz4/single
#


## I. Compression and Open expectation block with no-error condition
#
# 1. Fail the expectation block and check that each message that
#    was compressed is replied with error without executing it
# 2. Close expectation block and check that compressed messages
#    are correctly decompressed (context in sync) and executed

#
# I.1
send Mysqlx.Expect.Open {
  cond {
    condition_key: 1
  }
}

RUN recvok
RUN SELECT invalid_syntax(-)
While executing SELECT invalid_syntax(-):
Got expected error: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near ')' at line 1 (code 1064)
Got expected error: Expectation failed: no_error (code 5159)
Got expected error: Expectation failed: no_error (code 5159)
Got expected error: Expectation failed: no_error (code 5159)

#
# I.2
send Mysqlx.Expect.Close {
}

Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 5159
  msg: "Expectation failed: no_error"
  sql_state: "HY000"
}

First query
First query
0 rows affected
Got expected error (code 0)
Second query
Second query
0 rows affected
Got expected error (code 0)
Last query
Last query
0 rows affected
Got expected error (code 0)
Mysqlx.Ok {
  msg: "bye!"
}
ok

## C. Execute the test using zlib/multiple
#


## I. Compression and Open expectation block with no-error condition
#
# 1. Fail the expectation block and check that each message that
#    was compressed is replied with error without executing it
# 2. Close expectation block and check that compressed messages
#    are correctly decompressed (context in sync) and executed

#
# I.1
send Mysqlx.Expect.Open {
  cond {
    condition_key: 1
  }
}

RUN recvok
RUN SELECT invalid_syntax(-)
While executing SELECT invalid_syntax(-):
Got expected error: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near ')' at line 1 (code 1064)
Got expected error: Expectation failed: no_error (code 5159)
Got expected error: Expectation failed: no_error (code 5159)
Got expected error: Expectation failed: no_error (code 5159)

#
# I.2
send Mysqlx.Expect.Close {
}

Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 5159
  msg: "Expectation failed: no_error"
  sql_state: "HY000"
}

First query
First query
0 rows affected
Got expected error (code 0)
Second query
Second query
0 rows affected
Got expected error (code 0)
Last query
Last query
0 rows affected
Got expected error (code 0)
Mysqlx.Ok {
  msg: "bye!"
}
ok

## D. Execute the test using lz4/multiple
#


## I. Compression and Open expectation block with no-error condition
#
# 1. Fail the expectation block and check that each message that
#    was compressed is replied with error without executing it
# 2. Close expectation block and check that compressed messages
#    are correctly decompressed (context in sync) and executed

#
# I.1
send Mysqlx.Expect.Open {
  cond {
    condition_key: 1
  }
}

RUN recvok
RUN SELECT invalid_syntax(-)
While executing SELECT invalid_syntax(-):
Got expected error: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near ')' at line 1 (code 1064)
Got expected error: Expectation failed: no_error (code 5159)
Got expected error: Expectation failed: no_error (code 5159)
Got expected error: Expectation failed: no_error (code 5159)

#
# I.2
send Mysqlx.Expect.Close {
}

Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 5159
  msg: "Expectation failed: no_error"
  sql_state: "HY000"
}

First query
First query
0 rows affected
Got expected error (code 0)
Second query
Second query
0 rows affected
Got expected error (code 0)
Last query
Last query
0 rows affected
Got expected error (code 0)
Mysqlx.Ok {
  msg: "bye!"
}
ok

## E. Execute the test using zlib/group
#


## I. Compression and Open expectation block with no-error condition
#
# 1. Fail the expectation block and check that each message that
#    was compressed is replied with error without executing it
# 2. Close expectation block and check that compressed messages
#    are correctly decompressed (context in sync) and executed

#
# I.1
send Mysqlx.Expect.Open {
  cond {
    condition_key: 1
  }
}

RUN recvok
RUN SELECT invalid_syntax(-)
While executing SELECT invalid_syntax(-):
Got expected error: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near ')' at line 1 (code 1064)
Got expected error: Expectation failed: no_error (code 5159)
Got expected error: Expectation failed: no_error (code 5159)
Got expected error: Expectation failed: no_error (code 5159)

#
# I.2
send Mysqlx.Expect.Close {
}

Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 5159
  msg: "Expectation failed: no_error"
  sql_state: "HY000"
}

First query
First query
0 rows affected
Got expected error (code 0)
Second query
Second query
0 rows affected
Got expected error (code 0)
Last query
Last query
0 rows affected
Got expected error (code 0)
Mysqlx.Ok {
  msg: "bye!"
}
ok

## F. Execute the test using lz4/group
#


## I. Compression and Open expectation block with no-error condition
#
# 1. Fail the expectation block and check that each message that
#    was compressed is replied with error without executing it
# 2. Close expectation block and check that compressed messages
#    are correctly decompressed (context in sync) and executed

#
# I.1
send Mysqlx.Expect.Open {
  cond {
    condition_key: 1
  }
}

RUN recvok
RUN SELECT invalid_syntax(-)
While executing SELECT invalid_syntax(-):
Got expected error: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near ')' at line 1 (code 1064)
Got expected error: Expectation failed: no_error (code 5159)
Got expected error: Expectation failed: no_error (code 5159)
Got expected error: Expectation failed: no_error (code 5159)

#
# I.2
send Mysqlx.Expect.Close {
}

Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 5159
  msg: "Expectation failed: no_error"
  sql_state: "HY000"
}

First query
First query
0 rows affected
Got expected error (code 0)
Second query
Second query
0 rows affected
Got expected error (code 0)
Last query
Last query
0 rows affected
Got expected error (code 0)
Mysqlx.Ok {
  msg: "bye!"
}
ok

## G. Execute the test using zstd/single
#


## I. Compression and Open expectation block with no-error condition
#
# 1. Fail the expectation block and check that each message that
#    was compressed is replied with error without executing it
# 2. Close expectation block and check that compressed messages
#    are correctly decompressed (context in sync) and executed

#
# I.1
send Mysqlx.Expect.Open {
  cond {
    condition_key: 1
  }
}

RUN recvok
RUN SELECT invalid_syntax(-)
While executing SELECT invalid_syntax(-):
Got expected error: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near ')' at line 1 (code 1064)
Got expected error: Expectation failed: no_error (code 5159)
Got expected error: Expectation failed: no_error (code 5159)
Got expected error: Expectation failed: no_error (code 5159)

#
# I.2
send Mysqlx.Expect.Close {
}

Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 5159
  msg: "Expectation failed: no_error"
  sql_state: "HY000"
}

First query
First query
0 rows affected
Got expected error (code 0)
Second query
Second query
0 rows affected
Got expected error (code 0)
Last query
Last query
0 rows affected
Got expected error (code 0)
Mysqlx.Ok {
  msg: "bye!"
}
ok

## H. Execute the test using zstd/multiple
#


## I. Compression and Open expectation block with no-error condition
#
# 1. Fail the expectation block and check that each message that
#    was compressed is replied with error without executing it
# 2. Close expectation block and check that compressed messages
#    are correctly decompressed (context in sync) and executed

#
# I.1
send Mysqlx.Expect.Open {
  cond {
    condition_key: 1
  }
}

RUN recvok
RUN SELECT invalid_syntax(-)
While executing SELECT invalid_syntax(-):
Got expected error: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near ')' at line 1 (code 1064)
Got expected error: Expectation failed: no_error (code 5159)
Got expected error: Expectation failed: no_error (code 5159)
Got expected error: Expectation failed: no_error (code 5159)

#
# I.2
send Mysqlx.Expect.Close {
}

Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 5159
  msg: "Expectation failed: no_error"
  sql_state: "HY000"
}

First query
First query
0 rows affected
Got expected error (code 0)
Second query
Second query
0 rows affected
Got expected error (code 0)
Last query
Last query
0 rows affected
Got expected error (code 0)
Mysqlx.Ok {
  msg: "bye!"
}
ok

## I. Execute the test using zstd/group
#


## I. Compression and Open expectation block with no-error condition
#
# 1. Fail the expectation block and check that each message that
#    was compressed is replied with error without executing it
# 2. Close expectation block and check that compressed messages
#    are correctly decompressed (context in sync) and executed

#
# I.1
send Mysqlx.Expect.Open {
  cond {
    condition_key: 1
  }
}

RUN recvok
RUN SELECT invalid_syntax(-)
While executing SELECT invalid_syntax(-):
Got expected error: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near ')' at line 1 (code 1064)
Got expected error: Expectation failed: no_error (code 5159)
Got expected error: Expectation failed: no_error (code 5159)
Got expected error: Expectation failed: no_error (code 5159)

#
# I.2
send Mysqlx.Expect.Close {
}

Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 5159
  msg: "Expectation failed: no_error"
  sql_state: "HY000"
}

First query
First query
0 rows affected
Got expected error (code 0)
Second query
Second query
0 rows affected
Got expected error (code 0)
Last query
Last query
0 rows affected
Got expected error (code 0)
Mysqlx.Ok {
  msg: "bye!"
}
ok
