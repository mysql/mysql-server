CREATE SCHEMA IF NOT EXISTS xtest;

command ok

1 rows affected

1 rows affected

1 rows affected

## x. Prepare find with OP in [==, !=, <, <=, >, >=]
#   a. Try to execute (no params)
#   b. Execute with ...001
#   c. Execute with ...002
#   d. Execute with ...003
#   e. Deallocate


Stmt 1: Prepare find with '? == _id'
====================================
RUN recvok

Stmt 1.a. Try to execute (no params)
====================================
Got expected error: There is no argument for statement placeholder at position: 0 (code 5134)

Stmt 1.b. Execute == with '0000XXXXXXXX0000000000000001'
========================================================
doc
{"_id": "0000XXXXXXXX0000000000000001", "name": "foo"}
command ok

Stmt 1.b. Execute == with '0000XXXXXXXX0000000000000002'
========================================================
doc
{"_id": "0000XXXXXXXX0000000000000002", "name": "bar"}
command ok

Stmt 1.b. Execute == with '0000XXXXXXXX0000000000000003'
========================================================
doc
{"_id": "0000XXXXXXXX0000000000000003", "name": "buz"}
command ok

Stmt 1.d. Deallocate
====================
RUN recvok

Stmt 1: Prepare find with '_id == ?'
====================================
RUN recvok

Stmt 1.a. Try to execute (no params)
====================================
Got expected error: There is no argument for statement placeholder at position: 0 (code 5134)

Stmt 1.b. Execute == with '0000XXXXXXXX0000000000000001'
========================================================
doc
{"_id": "0000XXXXXXXX0000000000000001", "name": "foo"}
command ok

Stmt 1.b. Execute == with '0000XXXXXXXX0000000000000002'
========================================================
doc
{"_id": "0000XXXXXXXX0000000000000002", "name": "bar"}
command ok

Stmt 1.b. Execute == with '0000XXXXXXXX0000000000000003'
========================================================
doc
{"_id": "0000XXXXXXXX0000000000000003", "name": "buz"}
command ok

Stmt 1.d. Deallocate
====================
RUN recvok

Stmt 2: Prepare find with '? != _id'
====================================
RUN recvok

Stmt 2.a. Try to execute (no params)
====================================
Got expected error: There is no argument for statement placeholder at position: 0 (code 5134)

Stmt 2.b. Execute != with '0000XXXXXXXX0000000000000001'
========================================================
doc
{"_id": "0000XXXXXXXX0000000000000002", "name": "bar"}
{"_id": "0000XXXXXXXX0000000000000003", "name": "buz"}
command ok

Stmt 2.b. Execute != with '0000XXXXXXXX0000000000000002'
========================================================
doc
{"_id": "0000XXXXXXXX0000000000000001", "name": "foo"}
{"_id": "0000XXXXXXXX0000000000000003", "name": "buz"}
command ok

Stmt 2.b. Execute != with '0000XXXXXXXX0000000000000003'
========================================================
doc
{"_id": "0000XXXXXXXX0000000000000001", "name": "foo"}
{"_id": "0000XXXXXXXX0000000000000002", "name": "bar"}
command ok

Stmt 2.d. Deallocate
====================
RUN recvok

Stmt 2: Prepare find with '_id != ?'
====================================
RUN recvok

Stmt 2.a. Try to execute (no params)
====================================
Got expected error: There is no argument for statement placeholder at position: 0 (code 5134)

Stmt 2.b. Execute != with '0000XXXXXXXX0000000000000001'
========================================================
doc
{"_id": "0000XXXXXXXX0000000000000002", "name": "bar"}
{"_id": "0000XXXXXXXX0000000000000003", "name": "buz"}
command ok

Stmt 2.b. Execute != with '0000XXXXXXXX0000000000000002'
========================================================
doc
{"_id": "0000XXXXXXXX0000000000000001", "name": "foo"}
{"_id": "0000XXXXXXXX0000000000000003", "name": "buz"}
command ok

Stmt 2.b. Execute != with '0000XXXXXXXX0000000000000003'
========================================================
doc
{"_id": "0000XXXXXXXX0000000000000001", "name": "foo"}
{"_id": "0000XXXXXXXX0000000000000002", "name": "bar"}
command ok

Stmt 2.d. Deallocate
====================
RUN recvok

Stmt 3: Prepare find with '? > _id'
===================================
RUN recvok

Stmt 3.a. Try to execute (no params)
====================================
Got expected error: There is no argument for statement placeholder at position: 0 (code 5134)

Stmt 3.b. Execute > with '0000XXXXXXXX0000000000000001'
=======================================================
doc
command ok

Stmt 3.b. Execute > with '0000XXXXXXXX0000000000000002'
=======================================================
doc
{"_id": "0000XXXXXXXX0000000000000001", "name": "foo"}
command ok

Stmt 3.b. Execute > with '0000XXXXXXXX0000000000000003'
=======================================================
doc
{"_id": "0000XXXXXXXX0000000000000001", "name": "foo"}
{"_id": "0000XXXXXXXX0000000000000002", "name": "bar"}
command ok

Stmt 3.d. Deallocate
====================
RUN recvok

Stmt 3: Prepare find with '_id > ?'
===================================
RUN recvok

Stmt 3.a. Try to execute (no params)
====================================
Got expected error: There is no argument for statement placeholder at position: 0 (code 5134)

Stmt 3.b. Execute > with '0000XXXXXXXX0000000000000001'
=======================================================
doc
{"_id": "0000XXXXXXXX0000000000000002", "name": "bar"}
{"_id": "0000XXXXXXXX0000000000000003", "name": "buz"}
command ok

Stmt 3.b. Execute > with '0000XXXXXXXX0000000000000002'
=======================================================
doc
{"_id": "0000XXXXXXXX0000000000000003", "name": "buz"}
command ok

Stmt 3.b. Execute > with '0000XXXXXXXX0000000000000003'
=======================================================
doc
command ok

Stmt 3.d. Deallocate
====================
RUN recvok

Stmt 4: Prepare find with '? >= _id'
====================================
RUN recvok

Stmt 4.a. Try to execute (no params)
====================================
Got expected error: There is no argument for statement placeholder at position: 0 (code 5134)

Stmt 4.b. Execute >= with '0000XXXXXXXX0000000000000001'
========================================================
doc
{"_id": "0000XXXXXXXX0000000000000001", "name": "foo"}
command ok

Stmt 4.b. Execute >= with '0000XXXXXXXX0000000000000002'
========================================================
doc
{"_id": "0000XXXXXXXX0000000000000001", "name": "foo"}
{"_id": "0000XXXXXXXX0000000000000002", "name": "bar"}
command ok

Stmt 4.b. Execute >= with '0000XXXXXXXX0000000000000003'
========================================================
doc
{"_id": "0000XXXXXXXX0000000000000001", "name": "foo"}
{"_id": "0000XXXXXXXX0000000000000002", "name": "bar"}
{"_id": "0000XXXXXXXX0000000000000003", "name": "buz"}
command ok

Stmt 4.d. Deallocate
====================
RUN recvok

Stmt 4: Prepare find with '_id >= ?'
====================================
RUN recvok

Stmt 4.a. Try to execute (no params)
====================================
Got expected error: There is no argument for statement placeholder at position: 0 (code 5134)

Stmt 4.b. Execute >= with '0000XXXXXXXX0000000000000001'
========================================================
doc
{"_id": "0000XXXXXXXX0000000000000001", "name": "foo"}
{"_id": "0000XXXXXXXX0000000000000002", "name": "bar"}
{"_id": "0000XXXXXXXX0000000000000003", "name": "buz"}
command ok

Stmt 4.b. Execute >= with '0000XXXXXXXX0000000000000002'
========================================================
doc
{"_id": "0000XXXXXXXX0000000000000002", "name": "bar"}
{"_id": "0000XXXXXXXX0000000000000003", "name": "buz"}
command ok

Stmt 4.b. Execute >= with '0000XXXXXXXX0000000000000003'
========================================================
doc
{"_id": "0000XXXXXXXX0000000000000003", "name": "buz"}
command ok

Stmt 4.d. Deallocate
====================
RUN recvok

Stmt 5: Prepare find with '? < _id'
===================================
RUN recvok

Stmt 5.a. Try to execute (no params)
====================================
Got expected error: There is no argument for statement placeholder at position: 0 (code 5134)

Stmt 5.b. Execute < with '0000XXXXXXXX0000000000000001'
=======================================================
doc
{"_id": "0000XXXXXXXX0000000000000002", "name": "bar"}
{"_id": "0000XXXXXXXX0000000000000003", "name": "buz"}
command ok

Stmt 5.b. Execute < with '0000XXXXXXXX0000000000000002'
=======================================================
doc
{"_id": "0000XXXXXXXX0000000000000003", "name": "buz"}
command ok

Stmt 5.b. Execute < with '0000XXXXXXXX0000000000000003'
=======================================================
doc
command ok

Stmt 5.d. Deallocate
====================
RUN recvok

Stmt 5: Prepare find with '_id < ?'
===================================
RUN recvok

Stmt 5.a. Try to execute (no params)
====================================
Got expected error: There is no argument for statement placeholder at position: 0 (code 5134)

Stmt 5.b. Execute < with '0000XXXXXXXX0000000000000001'
=======================================================
doc
command ok

Stmt 5.b. Execute < with '0000XXXXXXXX0000000000000002'
=======================================================
doc
{"_id": "0000XXXXXXXX0000000000000001", "name": "foo"}
command ok

Stmt 5.b. Execute < with '0000XXXXXXXX0000000000000003'
=======================================================
doc
{"_id": "0000XXXXXXXX0000000000000001", "name": "foo"}
{"_id": "0000XXXXXXXX0000000000000002", "name": "bar"}
command ok

Stmt 5.d. Deallocate
====================
RUN recvok

Stmt 6: Prepare find with '? <= _id'
====================================
RUN recvok

Stmt 6.a. Try to execute (no params)
====================================
Got expected error: There is no argument for statement placeholder at position: 0 (code 5134)

Stmt 6.b. Execute <= with '0000XXXXXXXX0000000000000001'
========================================================
doc
{"_id": "0000XXXXXXXX0000000000000001", "name": "foo"}
{"_id": "0000XXXXXXXX0000000000000002", "name": "bar"}
{"_id": "0000XXXXXXXX0000000000000003", "name": "buz"}
command ok

Stmt 6.b. Execute <= with '0000XXXXXXXX0000000000000002'
========================================================
doc
{"_id": "0000XXXXXXXX0000000000000002", "name": "bar"}
{"_id": "0000XXXXXXXX0000000000000003", "name": "buz"}
command ok

Stmt 6.b. Execute <= with '0000XXXXXXXX0000000000000003'
========================================================
doc
{"_id": "0000XXXXXXXX0000000000000003", "name": "buz"}
command ok

Stmt 6.d. Deallocate
====================
RUN recvok

Stmt 6: Prepare find with '_id <= ?'
====================================
RUN recvok

Stmt 6.a. Try to execute (no params)
====================================
Got expected error: There is no argument for statement placeholder at position: 0 (code 5134)

Stmt 6.b. Execute <= with '0000XXXXXXXX0000000000000001'
========================================================
doc
{"_id": "0000XXXXXXXX0000000000000001", "name": "foo"}
command ok

Stmt 6.b. Execute <= with '0000XXXXXXXX0000000000000002'
========================================================
doc
{"_id": "0000XXXXXXXX0000000000000001", "name": "foo"}
{"_id": "0000XXXXXXXX0000000000000002", "name": "bar"}
command ok

Stmt 6.b. Execute <= with '0000XXXXXXXX0000000000000003'
========================================================
doc
{"_id": "0000XXXXXXXX0000000000000001", "name": "foo"}
{"_id": "0000XXXXXXXX0000000000000002", "name": "bar"}
{"_id": "0000XXXXXXXX0000000000000003", "name": "buz"}
command ok

Stmt 6.d. Deallocate
====================
RUN recvok
Mysqlx.Ok {
  msg: "bye!"
}
ok
SET @id1 = '0000616daea30000000000000001',
@id2 = '0000616daea30000000000000002',
@id3 = '0000616daea30000000000000003';
PREPARE s1 FROM "SELECT doc FROM xtest.expr WHERE JSON_UNQUOTE(JSON_EXTRACT(doc, '$._id')) = ?";
EXECUTE s1 using @id1;
doc
{"_id": "0000616daea30000000000000001", "name": "foo"}
EXECUTE s1 using @id2;
doc
{"_id": "0000616daea30000000000000002", "name": "bar"}
EXECUTE s1 using @id3;
doc
{"_id": "0000616daea30000000000000003", "name": "buz"}
DEALLOCATE PREPARE s1;
PREPARE s1 FROM "SELECT doc FROM xtest.expr WHERE JSON_UNQUOTE(JSON_EXTRACT(doc, '$._id')) != ?";
EXECUTE s1 using @id1;
doc
{"_id": "0000616daea30000000000000002", "name": "bar"}
{"_id": "0000616daea30000000000000003", "name": "buz"}
EXECUTE s1 using @id2;
doc
{"_id": "0000616daea30000000000000001", "name": "foo"}
{"_id": "0000616daea30000000000000003", "name": "buz"}
EXECUTE s1 using @id3;
doc
{"_id": "0000616daea30000000000000001", "name": "foo"}
{"_id": "0000616daea30000000000000002", "name": "bar"}
DEALLOCATE PREPARE s1;
PREPARE s1 FROM "SELECT doc FROM xtest.expr WHERE JSON_UNQUOTE(JSON_EXTRACT(doc, '$._id')) > ?";
EXECUTE s1 using @id1;
doc
{"_id": "0000616daea30000000000000002", "name": "bar"}
{"_id": "0000616daea30000000000000003", "name": "buz"}
EXECUTE s1 using @id2;
doc
{"_id": "0000616daea30000000000000003", "name": "buz"}
EXECUTE s1 using @id3;
doc
DEALLOCATE PREPARE s1;
PREPARE s1 FROM "SELECT doc FROM xtest.expr WHERE JSON_UNQUOTE(JSON_EXTRACT(doc, '$._id')) >= ?";
EXECUTE s1 using @id1;
doc
{"_id": "0000616daea30000000000000001", "name": "foo"}
{"_id": "0000616daea30000000000000002", "name": "bar"}
{"_id": "0000616daea30000000000000003", "name": "buz"}
EXECUTE s1 using @id2;
doc
{"_id": "0000616daea30000000000000002", "name": "bar"}
{"_id": "0000616daea30000000000000003", "name": "buz"}
EXECUTE s1 using @id3;
doc
{"_id": "0000616daea30000000000000003", "name": "buz"}
DEALLOCATE PREPARE s1;
PREPARE s1 FROM "SELECT doc FROM xtest.expr WHERE JSON_UNQUOTE(JSON_EXTRACT(doc, '$._id')) < ?";
EXECUTE s1 using @id1;
doc
EXECUTE s1 using @id2;
doc
{"_id": "0000616daea30000000000000001", "name": "foo"}
EXECUTE s1 using @id3;
doc
{"_id": "0000616daea30000000000000001", "name": "foo"}
{"_id": "0000616daea30000000000000002", "name": "bar"}
DEALLOCATE PREPARE s1;
PREPARE s1 FROM "SELECT doc FROM xtest.expr WHERE JSON_UNQUOTE(JSON_EXTRACT(doc, '$._id')) <= ?";
EXECUTE s1 using @id1;
doc
{"_id": "0000616daea30000000000000001", "name": "foo"}
EXECUTE s1 using @id2;
doc
{"_id": "0000616daea30000000000000001", "name": "foo"}
{"_id": "0000616daea30000000000000002", "name": "bar"}
EXECUTE s1 using @id3;
doc
{"_id": "0000616daea30000000000000001", "name": "foo"}
{"_id": "0000616daea30000000000000002", "name": "bar"}
{"_id": "0000616daea30000000000000003", "name": "buz"}
DEALLOCATE PREPARE s1;
