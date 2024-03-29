# restart
CREATE SCHEMA IF NOT EXISTS xtest;
USE xtest;
CREATE TABLE xtable (phrase VARCHAR(30));
INSERT INTO xtable (phrase) VALUES ('Alice has a cat');
INSERT INTO xtable (phrase) VALUES ('Bob has a dog');

## 1. Verify prep-stat status variables that are set to zero
## 2. Prepare statement
#   a. fails in case when invalid data are supplied
#   b. success for valid data with one placeholder
## 3. Execute prepared statement
#   a. success for execute with one parameter
#   b. success for execute with two parameters (more than needed)
#   c. fails when no parameter was supplied
#   d. fails when parameter of invalid type was supplied
#   e. fails when using invalid ID for prepared-statmenent
## 4. Deallocate prepared statement
#   a. success when using ID that was prepared
#   b. fails when using ID that was already deallocated
#   c. fails when using ID that was never prepared
## 5. Mixed tests
#   a. prepare-deallocate, reuse the same ID in next prepare
#   b. prepare two times same statement, show that previous stmt was deallocated
#   c. prepare-execute-deallocate confirm that next execute with deallocated-id is going to fail
#   d. prepare-execute stmt that returns NULL
## 6. Assert status variables for new session, they should be at zero
## 7. Assert that prep-statment id is local in context of session
## 8. Assert that prep-execute-statment id is local in context of session

 1. Prep-stmt variables are set to zero
=======================================
Verify [Status variable "Mysqlx_prep_prepare" needs to have a value of 0]
Verify [Status variable "Mysqlx_prep_execute" needs to have a value of 0]
Verify [Status variable "Mysqlx_prep_deallocate" needs to have a value of 0]
Verify [Status variable "Prepared_stmt_count" needs to have a value of 0]
Verify [Status variable "Com_stmt_prepare" needs to have a value of 0]
Verify [Status variable "Com_stmt_execute" needs to have a value of 0]
Verify [Status variable "Com_stmt_fetch" needs to have a value of 0]
Verify [Status variable "Com_stmt_close" needs to have a value of 0]

 2.a. Malformed stmt for prepare
================================
Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 1049
  msg: "Unknown database \'nonexistent\'"
  sql_state: "42000"
}

Verify [Status variable "Mysqlx_prep_prepare" needs to have a value of 1]
Verify [Status variable "Prepared_stmt_count" needs to have a value of 0]

 2.b. Correct stmt
==================
RUN recvok
Verify [Status variable "Mysqlx_prep_prepare" needs to have a value of 2]
Verify [Status variable "Prepared_stmt_count" needs to have a value of 1]

 3.a. Correct stmt execute
==========================
count('*')
2
command ok
Verify [Status variable "Mysqlx_prep_execute" needs to have a value of 1]

 3.b. Correct stmt execute (more than one expected args)
========================================================
count('*')
1
command ok
Verify [Status variable "Mysqlx_prep_execute" needs to have a value of 2]

 3.c. Wrong stmt execute (no arg)
=================================
Got expected error: There is no argument for statement placeholder at position: 0 (code 5134)
Verify [Status variable "Mysqlx_prep_execute" needs to have a value of 3]

 3.d. Correct stmt executed, still argument type is invalid (only scalar is supported)
======================================================================================
Got expected error: Argument at index '0' and of type 'Mysqlx.Datatypes.Any' is not supported for binding to prepared statement (code 5133)
Verify [Status variable "Mysqlx_prep_execute" needs to have a value of 4]

 3.e. Wrong stmt execute (wrong id)
===================================
Got expected error: Statement with ID=42 was not prepared (code 5110)
Verify [Status variable "Mysqlx_prep_execute" needs to have a value of 5]

 4.a. Correct stmt close
========================
RUN recvok
Verify [Status variable "Mysqlx_prep_deallocate" needs to have a value of 1]
Verify [Status variable "Prepared_stmt_count" needs to have a value of 0]

 4.b. Wrong stmt close (bad id) / try to close second time
==========================================================
Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 5110
  msg: "Statement with ID=1 was not prepared"
  sql_state: "HY000"
}

Verify [Status variable "Mysqlx_prep_deallocate" needs to have a value of 2]

 4.c. Wrong stmt close (bad id)
===============================
Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 5110
  msg: "Statement with ID=77 was not prepared"
  sql_state: "HY000"
}

Verify [Status variable "Mysqlx_prep_deallocate" needs to have a value of 3]

 5.a. Reuse deallocated ID
==========================
RUN recvok
Verify [Status variable "Prepared_stmt_count" needs to have a value of 1]
RUN recvok
RUN recvok
Verify [Status variable "Prepared_stmt_count" needs to have a value of 1]
RUN recvok
Verify [Status variable "Prepared_stmt_count" needs to have a value of 0]

 5.b. Reuse ID which deallocates previous one
=============================================
RUN recvok
Verify [Status variable "Prepared_stmt_count" needs to have a value of 1]
RUN recvok
Verify [Status variable "Prepared_stmt_count" needs to have a value of 1]
RUN recvok

 5.c. prepare-execute-deallocate confirm that next execute with deallocated-id is going to fail
===============================================================================================
Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 5110
  msg: "Statement with ID=3 was not prepared"
  sql_state: "HY000"
}

Verify [Status variable "Prepared_stmt_count" needs to have a value of 0]

 5.d. prepare-execute stmt that returns NULL
============================================
RUN recvok
NULL
null
0 rows affected
RUN recvok
Verify [Status variable "Prepared_stmt_count" needs to have a value of 0]
Verify [Status variable "Com_stmt_close" needs to have a value of 6]
Verify [Status variable "Com_stmt_prepare" needs to have a value of 7]
Verify [Status variable "Com_stmt_execute" needs to have a value of 5]
Verify [Status variable "Com_stmt_fetch" needs to have a value of 0]

 6. Assert status variables for new session, they should be at zero
===================================================================
connecting...
active session is now 's1'
Verify [Status variable "Mysqlx_prep_prepare" needs to have a value of 0]
Verify [Status variable "Mysqlx_prep_execute" needs to have a value of 0]
Verify [Status variable "Mysqlx_prep_deallocate" needs to have a value of 0]

 7. Assert that prep-statment id is local in context of session
===============================================================
RUN recvok
closing session s1
Mysqlx.Ok {
  msg: "bye!"
}
switched to session default
connecting...
active session is now 's2'
Got expected error: Statement with ID=1 was not prepared (code 5110)
RUN recvok

 8. Assert that prep-execute-statment id is local in context of session
=======================================================================
RUN recvok
1
1
0 rows affected
closing session s2
Mysqlx.Ok {
  msg: "bye!"
}
switched to session default
connecting...
active session is now 's2'
Got expected error: Statement with ID=1 was not prepared (code 5110)
RUN recvok
closing session s2
Mysqlx.Ok {
  msg: "bye!"
}
ok
# Syncing with status [ 'Mysqlx_connections_closed' == 4 ]
# Syncing with status [ 'Mysqlx_sessions' == 0 ]

## Assert global X Plugin status variables
include/assert.inc [Global plugin status Mysqlx_prep_prepare must match number of send Mysqlx.Prepare.Prepare messages]
include/assert.inc [Global plugin status Mysqlx_prep_execute must match number of send Mysqlx.Prepare.Execute messages]
include/assert.inc [Global plugin status Mysqlx_prep_deallocate must match number of send Mysqlx.Prepare.Deallocate messages]

## Assert global server status variables
include/assert.inc [Global system status Prepared_stmt_count must be set to zero, all prep-stmt must be deallocated.]
include/assert.inc [Global system status Com_stmt_prepare must be set to zero, all prep-stmt must be deallocated.]
include/assert.inc [Global system status Com_stmt_execute must be set to zero, all prep-stmt must be deallocated.]
include/assert.inc [Global system status Com_stmt_fetch must be set to zero, all prep-stmt must be deallocated.]
include/assert.inc [Global system status Com_stmt_close must be set to zero, all prep-stmt must be deallocated.]
