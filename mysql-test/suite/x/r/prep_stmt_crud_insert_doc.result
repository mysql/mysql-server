CREATE SCHEMA IF NOT EXISTS xtest;

command ok

## 1. Prepare malformed insert stmt (no schema)
#   a. Try to execute
#   b. Try to deallocate
## 2. Prepare correct insert stmt (no placeholders)
#   a. Execute (no params)
#   b. Deallocate
## 3. Prepare correct stmt (one placeholder)
#   a. Try to execute (no params)
#   b. Execute with param
#   c. Execute with params (one extra to ignore)
#   d. Deallocate
## 4. Prepare correct stmt (two placeholders)
#   a. Execute with params
#   b. Deallocate
## 5. Prepare correct stmt (two placeholders in expr)
#   a. Execute with params
#   b. Deallocate
## 6. Prepare correct stmt (placeholder on _id)
#   a. Execute with param
#   b. Deallocate
## 7. Prepare correct stmt with upsert (placeholder on "type" field)
#   a. Execute with param "dog" (insert - "dog")
#   b. Execute with param "cat" (update from "dog" to "cat")
#   c. Deallocate

Stmt 1: Prepare malformed insert stmt (no schema)
=================================================
Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 1049
  msg: "Unknown database \'nonexistent\'"
  sql_state: "42000"
}

Verify [Status variable "Mysqlx_prep_prepare" needs to have a value of 1]

Stmt 1.a. Try to execute
========================
Got expected error: Statement with ID=1 was not prepared (code 5110)
Verify [Status variable "Mysqlx_prep_execute" needs to have a value of 1]

Stmt 1.b. Try to deallocate
===========================
Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 5110
  msg: "Statement with ID=1 was not prepared"
  sql_state: "HY000"
}

Verify [Status variable "Mysqlx_prep_deallocate" needs to have a value of 1]

Stmt 2: Prepare correct insert stmt (no placeholders)
=====================================================
RUN recvok
Verify [Status variable "Mysqlx_prep_prepare" needs to have a value of 2]

Stmt 2.a. Execute (no params)
=============================

1 rows affected
auto-generated id(s): 0000XXXXXXXX0000000000000001
Verify ["SELECT count(*) as count FROM xtest.xcol WHERE doc->>'$.pupil' LIKE 'Alice%'" returns in "count" column, following value 1]
Verify [Status variable "Mysqlx_prep_execute" needs to have a value of 2]

Stmt 2.b.: Deallocate
=====================
RUN recvok

Stmt 3: Prepare correct stmt (one placeholder)
==============================================
RUN recvok
Verify [Status variable "Mysqlx_prep_prepare" needs to have a value of 3]

Stmt 3.a. Try to execute (no params)
====================================
Got expected error: There is no argument for statement placeholder at position: 0 (code 5134)
Verify [Status variable "Mysqlx_prep_execute" needs to have a value of 3]

Stmt 3.b. Execute with param
============================

1 rows affected
auto-generated id(s): 0000XXXXXXXX0000000000000002
Verify ["SELECT count(*) as count FROM xtest.xcol WHERE doc->>'$.pupil' LIKE 'Bob%'" returns in "count" column, following value 1]
Verify [Status variable "Mysqlx_prep_execute" needs to have a value of 4]

Stmt 3.c. Execute with params (one extra to ignore)
===================================================

1 rows affected
auto-generated id(s): 0000XXXXXXXX0000000000000003
Verify ["SELECT count(*) as count FROM xtest.xcol WHERE doc->>'$.pupil' LIKE 'Ch%'" returns in "count" column, following value 1]
Verify [Status variable "Mysqlx_prep_execute" needs to have a value of 5]

Stmt 3.d. Deallocate
====================
RUN recvok
Verify [Status variable "Mysqlx_prep_deallocate" needs to have a value of 3]

Stmt 4: Prepare correct stmt (two placeholders)
===============================================
RUN recvok
Verify [Status variable "Mysqlx_prep_prepare" needs to have a value of 4]

Stmt 4.a. Execute with params
=============================

2 rows affected
auto-generated id(s): 0000XXXXXXXX0000000000000004
Records: 2  Duplicates: 0  Warnings: 0
Verify ["SELECT count(*) as count FROM xtest.xcol WHERE doc->>'$.pupil' LIKE 'David%'" returns in "count" column, following value 1]
Verify ["SELECT count(*) as count FROM xtest.xcol WHERE doc->>'$.pupil' LIKE 'Eva%'" returns in "count" column, following value 1]
Verify [Status variable "Mysqlx_prep_execute" needs to have a value of 6]

Stmt 4.b. Deallocate
====================
RUN recvok
Verify [Status variable "Mysqlx_prep_deallocate" needs to have a value of 4]

Stmt 5: Prepare correct stmt (two placeholders in expr)
=======================================================
RUN recvok
Verify [Status variable "Mysqlx_prep_prepare" needs to have a value of 5]

Stmt 5.a. Execute with params
=============================
Mysqlx.Notice.Frame {
  type: 3
  scope: LOCAL
  payload: "Mysqlx.Notice.SessionStateChanged { param: ROWS_AFFECTED\nvalue {\n  type: V_UINT\n  v_unsigned_int: 1\n}\n }"
}

Mysqlx.Notice.Frame {
  type: 3
  scope: LOCAL
  payload: "Mysqlx.Notice.SessionStateChanged { param: GENERATED_DOCUMENT_IDS\nvalue {\n  type: V_OCTETS\n  v_octets {\n    value: \"0000XXXXXXXX0000000000000005\"\n  }\n}\n }"
}

Mysqlx.Sql.StmtExecuteOk {
}

Verify ["SELECT count(*) as count FROM xtest.xcol WHERE doc->>'$.pupil' LIKE 'Felix%'" returns in "count" column, following value 1]
Verify [Status variable "Mysqlx_prep_execute" needs to have a value of 7]

Stmt 5.b. Deallocate
====================
RUN recvok
Verify [Status variable "Mysqlx_prep_deallocate" needs to have a value of 5]

Stmt 6: Prepare correct stmt (placeholder on _id)
=================================================
RUN recvok
Verify [Status variable "Mysqlx_prep_prepare" needs to have a value of 6]

Stmt 6.a. Execute with params
=============================
Mysqlx.Notice.Frame {
  type: 3
  scope: LOCAL
  payload: "Mysqlx.Notice.SessionStateChanged { param: ROWS_AFFECTED\nvalue {\n  type: V_UINT\n  v_unsigned_int: 1\n}\n }"
}

Mysqlx.Sql.StmtExecuteOk {
}

Verify ["SELECT count(*) as count FROM xtest.xcol WHERE doc->>'$.pupil' LIKE 'Felix%'" returns in "count" column, following value 1]
Verify [Status variable "Mysqlx_prep_execute" needs to have a value of 8]

Stmt 6.b. Deallocate
====================
RUN recvok
Verify [Status variable "Mysqlx_prep_deallocate" needs to have a value of 6]

Stmt 7: Prepare correct stmt with upsert (placeholder on "type" field)
======================================================================
RUN recvok
Verify [Status variable "Mysqlx_prep_prepare" needs to have a value of 7]

Stmt 7.a. Execute with param "dog" (insert - "dog")
===================================================
Mysqlx.Notice.Frame {
  type: 3
  scope: LOCAL
  payload: "Mysqlx.Notice.SessionStateChanged { param: ROWS_AFFECTED\nvalue {\n  type: V_UINT\n  v_unsigned_int: 1\n}\n }"
}

Mysqlx.Sql.StmtExecuteOk {
}

Verify [Status variable "Mysqlx_prep_execute" needs to have a value of 9]

Stmt 7.b. Execute with param "cat" (update from "dog" to "cat")
===============================================================
Mysqlx.Notice.Frame {
  type: 3
  scope: LOCAL
  payload: "Mysqlx.Notice.SessionStateChanged { param: ROWS_AFFECTED\nvalue {\n  type: V_UINT\n  v_unsigned_int: 2\n}\n }"
}

Mysqlx.Sql.StmtExecuteOk {
}

Verify [Status variable "Mysqlx_prep_execute" needs to have a value of 10]

Stmt 7.c. Deallocate
====================
RUN recvok
Verify [Status variable "Mysqlx_prep_deallocate" needs to have a value of 7]
Mysqlx.Ok {
  msg: "bye!"
}
ok
