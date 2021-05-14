CREATE SCHEMA IF NOT EXISTS xtest;
USE xtest;
CREATE TABLE xtable (phrase VARCHAR(30));
INSERT INTO xtable (phrase) VALUES ('Alice has a cat');
INSERT INTO xtable (phrase) VALUES ('Bob has a dog');

## 1. Verify prep-stat status variables that are set to zero
## 2. Execute cursor open
#   a. success for open with one parameter
#   b. success for open with two parameters (more than needed)
#   c. fails when no parameter was supplied, try to fetch using failed cursor_id
#   d. fails when using invalid cursor ID, try to fetch using failed cursor_id
## 3. Close cursor
#   a. success when using ID that was already opened
#   b. fails when using ID that was already deallocated
#   c. fails when using ID that was never opened
## 4. Mixed tests
#   a. open(curID1), close(curID1), open(curID1) - reuse already closes ID
#   b. open(curID1), open(curID1) - open implicit closes previous cursor
#   c. open(curID1, stmtID1), open(curID2, stmtID1) - curID2 closes curID1 because they use same stmtID
#   d. prep(stmtID1), open(curID1, stmtID1), deallocate(stmtID1), failed_fetch(curID1) - closure of prepared-statement, closes the cursor
## 5. Assert status variables for new session, they should be at zero

 1. Verify cursor status variables
==================================
Verify [Status variable "Mysqlx_prep_prepare" needs to have a value of 0]
Verify [Status variable "Mysqlx_cursor_open" needs to have a value of 0]
Verify [Status variable "Mysqlx_cursor_close" needs to have a value of 0]
Verify [Status variable "Mysqlx_cursor_fetch" needs to have a value of 0]

 2.a. Open cursor with valid stmt
=================================
RUN recvok
Verify [Status variable "Mysqlx_prep_prepare" needs to have a value of 1]
count('*')
command ok
Verify [Status variable "Mysqlx_cursor_open" needs to have a value of 1]

 2.b. Open cursor with valid stmt (args more than needed)
=========================================================
count('*')
command ok
Variable_name	Value
Mysqlx_cursor_open	2
0 rows affected
Verify [Status variable "Mysqlx_cursor_open" needs to have a value of 2]

 2.c. Fails open cursor with valid stmt (missing arg)
=====================================================
Got expected error: There is no argument for statement placeholder at position: 0 (code 5134)
Got expected error: Cursor with ID=102 was not opened. (code 5111)
Verify [Status variable "Mysqlx_cursor_open" needs to have a value of 3]
Verify [Status variable "Mysqlx_cursor_fetch" needs to have a value of 1]

 2.d. Fails open cursor with invalid stmt
=========================================
Got expected error: Statement with ID=200 was not prepared. (code 5110)
Got expected error: Cursor with ID=103 was not opened. (code 5111)
Verify [Status variable "Mysqlx_cursor_open" needs to have a value of 4]

 3.a. Close already opened cursor
=================================
RUN recvok
1
command ok
RUN recvok
Verify [Status variable "Mysqlx_cursor_close" needs to have a value of 1]

 3.b. Fails close cursor (bad id) / try to close second time
============================================================
Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 5111
  msg: "Cursor with ID=100 was not opened."
  sql_state: "HY000"
}

Verify [Status variable "Mysqlx_cursor_close" needs to have a value of 2]

 3.c.  Fails close cursor (bad id/never used)
=============================================
Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 5111
  msg: "Cursor with ID=10000 was not opened."
  sql_state: "HY000"
}

Verify [Status variable "Mysqlx_cursor_close" needs to have a value of 3]

 4.a. Reuse deallocated cursor ID
=================================
RUN recvok
RUN recvok
3
command ok
RUN recvok
4
command ok

 4.b. Reuse ID without direct deallocation
==========================================
RUN recvok
col1
command ok
col1
command ok

 4.c. Same stmt can't be opened by cursor twice
===============================================
RUN recvok
col1
command ok
col1
command ok

command ok
Got expected error: Cursor with ID=106 was not opened. (code 5111)
Verify [Status variable "Mysqlx_cursor_fetch" needs to have a value of 4]

 4.d. Closure of prepared-statement, closes the cursor
======================================================
RUN recvok
col1
command ok
Got expected error: Cursor with ID=107 was not opened. (code 5111)

 5. Assert status variables for new session, they should be at zero
===================================================================
connecting...
active session is now 's1'
Verify [Status variable "Mysqlx_prep_prepare" needs to have a value of 0]
Verify [Status variable "Mysqlx_cursor_open" needs to have a value of 0]
Verify [Status variable "Mysqlx_cursor_close" needs to have a value of 0]
Verify [Status variable "Mysqlx_cursor_fetch" needs to have a value of 0]
closing session s1
Mysqlx.Ok {
  msg: "bye!"
}
ok
# Syncing with status [ 'Mysqlx_connections_closed' == 2 ]
# Syncing with status [ 'Mysqlx_sessions' == 0 ]

## Assert global variables
include/assert.inc [Global plugin variable Mysqlx_cursor_open must match number of send Mysqlx.Cursor.Open messages]
include/assert.inc [Global plugin variable Mysqlx_cursor_close must match number of send Mysqlx.Cursor.Close messages]
include/assert.inc [Global plugin variable Mysqlx_cursor_fetch must match number of send Mysqlx.Cursor.Fetch messages]
include/assert.inc [Global system status Prepared_stmt_count must be set to zero, all prep-stmt must be deallocated.]
