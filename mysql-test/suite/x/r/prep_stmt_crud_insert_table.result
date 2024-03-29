CREATE SCHEMA IF NOT EXISTS xtest;
USE xtest;
CREATE TABLE xtable (phrase VARCHAR(30));

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

Stmt 1: Try to execute
======================
Got expected error: Statement with ID=1 was not prepared (code 5110)
Verify [Status variable "Mysqlx_prep_execute" needs to have a value of 1]

Stmt 1: Try to deallocate
=========================
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

Stmt 2: Execute (no params)
===========================

1 rows affected
Verify ["SELECT count(*) as count FROM xtest.xtable WHERE phrase LIKE 'Alice%'" returns in "count" column, following value 1]
Verify [Status variable "Mysqlx_prep_execute" needs to have a value of 2]

Stmt 2: Deallocate
==================
RUN recvok

Stmt 3: Prepare correct stmt (one placeholder)
==============================================
RUN recvok
Verify [Status variable "Mysqlx_prep_prepare" needs to have a value of 3]

Stmt 3: Try to execute (no params)
==================================
Got expected error: There is no argument for statement placeholder at position: 0 (code 5134)
Verify [Status variable "Mysqlx_prep_execute" needs to have a value of 3]

Stmt 3: Execute with param
==========================

1 rows affected
Verify ["SELECT count(*) as count FROM xtest.xtable WHERE phrase LIKE 'Bob%'" returns in "count" column, following value 1]
Verify [Status variable "Mysqlx_prep_execute" needs to have a value of 4]

Stmt 3: Execute with params (one extra to ignore)
=================================================

1 rows affected
Verify ["SELECT count(*) as count FROM xtest.xtable WHERE phrase LIKE 'Christina%'" returns in "count" column, following value 1]
Verify [Status variable "Mysqlx_prep_execute" needs to have a value of 5]

Stmt 3: Deallocate
==================
RUN recvok
Verify [Status variable "Mysqlx_prep_deallocate" needs to have a value of 3]

Stmt 4: Prepare correct stmt (two placeholders)
===============================================
RUN recvok
Verify [Status variable "Mysqlx_prep_prepare" needs to have a value of 4]

Stmt 4: Execute with params
===========================

2 rows affected
Records: 2  Duplicates: 0  Warnings: 0
Verify ["SELECT count(*) as count FROM xtest.xtable WHERE phrase LIKE 'David%'" returns in "count" column, following value 1]
Verify ["SELECT count(*) as count FROM xtest.xtable WHERE phrase LIKE 'Eva%'" returns in "count" column, following value 1]
Verify [Status variable "Mysqlx_prep_execute" needs to have a value of 6]

Stmt 4: Deallocate
==================
RUN recvok
Verify [Status variable "Mysqlx_prep_deallocate" needs to have a value of 4]

Stmt 5: Prepare correct stmt (two placeholders in expr)
=======================================================
RUN recvok
Verify [Status variable "Mysqlx_prep_prepare" needs to have a value of 5]

Stmt 5: Execute with params
===========================

1 rows affected
Verify ["SELECT count(*) as count FROM xtest.xtable WHERE phrase LIKE 'Felix%'" returns in "count" column, following value 1]
Verify [Status variable "Mysqlx_prep_execute" needs to have a value of 7]

Stmt 5: Deallocate
==================
RUN recvok
Verify [Status variable "Mysqlx_prep_deallocate" needs to have a value of 5]
Mysqlx.Ok {
  msg: "bye!"
}
ok
