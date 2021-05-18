CREATE SCHEMA xtest;

Insert: doc with _id (literal)
------------------------------

command ok

0 rows affected

1 rows affected
Got expected error (code 0)
doc
{"_id": "1", "one": 1}
0 rows affected

Insert: doc without _id (literal)
---------------------------------

command ok

0 rows affected

1 rows affected
auto-generated id(s): 0000XXXXXXXX0000000000000001
Got expected error (code 0)
doc
{"_id": "0000XXXXXXXX0000000000000001", "two": 2}
0 rows affected

Insert: doc with _id (object)
-----------------------------

command ok

0 rows affected

1 rows affected
Got expected error (code 0)
doc
{"_id": "1", "one": 1}
0 rows affected

Insert: doc without _id (object)
--------------------------------

command ok

0 rows affected

1 rows affected
auto-generated id(s): 0000XXXXXXXX0000000000000002
Got expected error (code 0)
doc
{"_id": "0000XXXXXXXX0000000000000002", "two": 2}
0 rows affected

Insert: doc with _id (placeholder)
----------------------------------

command ok

0 rows affected

1 rows affected
Got expected error (code 0)
doc
{"_id": "1", "one": 1}
0 rows affected

Insert: doc without _id (placeholder)
-------------------------------------

command ok

0 rows affected

1 rows affected
auto-generated id(s): 0000XXXXXXXX0000000000000003
Got expected error (code 0)
doc
{"_id": "0000XXXXXXXX0000000000000003", "two": 2}
0 rows affected

Insert: 2 doc without _id (literal)
-----------------------------------

command ok

0 rows affected

2 rows affected
auto-generated id(s): 0000XXXXXXXX0000000000000004,0000XXXXXXXX0000000000000005
Records: 2  Duplicates: 0  Warnings: 0
Got expected error (code 0)
doc
{"_id": "0000XXXXXXXX0000000000000004", "two": 2}
{"_id": "0000XXXXXXXX0000000000000005", "three": 3}
0 rows affected

Insert: 2 doc without _id (literal/object)
------------------------------------------

command ok

0 rows affected

2 rows affected
auto-generated id(s): 0000XXXXXXXX0000000000000006,0000XXXXXXXX0000000000000007
Records: 2  Duplicates: 0  Warnings: 0
Got expected error (code 0)
doc
{"_id": "0000XXXXXXXX0000000000000006", "three": 3}
{"_id": "0000XXXXXXXX0000000000000007", "two": 2}
0 rows affected

Insert: 2 doc with/without _id (literal)
----------------------------------------

command ok

0 rows affected

2 rows affected
auto-generated id(s): 0000XXXXXXXX0000000000000008
Records: 2  Duplicates: 0  Warnings: 0
Got expected error (code 0)
doc
{"_id": "0000XXXXXXXX0000000000000008", "three": 3}
{"_id": "1", "one": 1}
0 rows affected

Insert: 2 doc with/without _id (object)
---------------------------------------

command ok

0 rows affected

2 rows affected
auto-generated id(s): 0000XXXXXXXX0000000000000009
Records: 2  Duplicates: 0  Warnings: 0
Got expected error (code 0)
doc
{"_id": "0000XXXXXXXX0000000000000009", "two": 2}
{"_id": "1", "one": 1}
0 rows affected

Insert: bad doc (literal) no insert
-----------------------------------

command ok

0 rows affected
Got expected error: Invalid JSON text: "not a JSON text, may need CAST" at position 0 in value for column 'coll.doc'. (code 3140)
doc
0 rows affected

Insert: 2 doc (literal) one bad, no insert
------------------------------------------

command ok

0 rows affected
Got expected error: Invalid JSON text: "not a JSON text, may need CAST" at position 0 in value for column 'coll.doc'. (code 3140)
doc
0 rows affected

Insert: change of "unique_prefix"
---------------------------------

0 rows affected

command ok

0 rows affected

2 rows affected
auto-generated id(s): 00ffXXXXXXXX000000000000000a,00ffXXXXXXXX000000000000000b
Records: 2  Duplicates: 0  Warnings: 0
Got expected error (code 0)
doc
{"_id": "00ffXXXXXXXX000000000000000a", "two": 2}
{"_id": "00ffXXXXXXXX000000000000000b", "three": 3}
0 rows affected

Insert: change of "auto_increment_increment"
--------------------------------------------

0 rows affected

command ok

0 rows affected

2 rows affected
auto-generated id(s): 00ffXXXXXXXX0000000000000101,00ffXXXXXXXX0000000000000201
Records: 2  Duplicates: 0  Warnings: 0
Got expected error (code 0)
doc
{"_id": "00ffXXXXXXXX0000000000000101", "two": 2}
{"_id": "00ffXXXXXXXX0000000000000201", "three": 3}
0 rows affected

Insert: change of "auto_increment_offset"
-----------------------------------------

0 rows affected

command ok

0 rows affected

2 rows affected
auto-generated id(s): 00ffXXXXXXXX0000000000000280,00ffXXXXXXXX0000000000000380
Records: 2  Duplicates: 0  Warnings: 0
Got expected error (code 0)
doc
{"_id": "00ffXXXXXXXX0000000000000280", "two": 2}
{"_id": "00ffXXXXXXXX0000000000000380", "three": 3}
0 rows affected

Insert: reset of "unique_prefix", "auto_increment_increment" and "auto_increment_offset"
----------------------------------------------------------------------------------------

0 rows affected

0 rows affected

0 rows affected

command ok

0 rows affected

2 rows affected
auto-generated id(s): 0000XXXXXXXX0000000000000381,0000XXXXXXXX0000000000000382
Records: 2  Duplicates: 0  Warnings: 0
Got expected error (code 0)
doc
{"_id": "0000XXXXXXXX0000000000000381", "two": 2}
{"_id": "0000XXXXXXXX0000000000000382", "three": 3}
0 rows affected

Notice: insert doc with _id
---------------------------

command ok

0 rows affected
Mysqlx.Notice.Frame {
  type: 3
  scope: LOCAL
  payload: "Mysqlx.Notice.SessionStateChanged { param: ROWS_AFFECTED\nvalue {\n  type: V_UINT\n  v_unsigned_int: 1\n}\n }"
}

Mysqlx.Sql.StmtExecuteOk {
}


Notice: insert doc without _id
------------------------------

command ok

0 rows affected
Mysqlx.Notice.Frame {
  type: 3
  scope: LOCAL
  payload: "Mysqlx.Notice.SessionStateChanged { param: ROWS_AFFECTED\nvalue {\n  type: V_UINT\n  v_unsigned_int: 1\n}\n }"
}

Mysqlx.Notice.Frame {
  type: 3
  scope: LOCAL
  payload: "Mysqlx.Notice.SessionStateChanged { param: GENERATED_DOCUMENT_IDS\nvalue {\n  type: V_OCTETS\n  v_octets {\n    value: \"0000XXXXXXXX0000000000000383\"\n  }\n}\n }"
}

Mysqlx.Sql.StmtExecuteOk {
}


Notice: insert 2 doc with/without _id
-------------------------------------

command ok

0 rows affected
Mysqlx.Notice.Frame {
  type: 3
  scope: LOCAL
  payload: "Mysqlx.Notice.SessionStateChanged { param: PRODUCED_MESSAGE\nvalue {\n  type: V_STRING\n  v_string {\n    value: \"Records: 2  Duplicates: 0  Warnings: 0\"\n  }\n}\n }"
}

Mysqlx.Notice.Frame {
  type: 3
  scope: LOCAL
  payload: "Mysqlx.Notice.SessionStateChanged { param: ROWS_AFFECTED\nvalue {\n  type: V_UINT\n  v_unsigned_int: 2\n}\n }"
}

Mysqlx.Notice.Frame {
  type: 3
  scope: LOCAL
  payload: "Mysqlx.Notice.SessionStateChanged { param: GENERATED_DOCUMENT_IDS\nvalue {\n  type: V_OCTETS\n  v_octets {\n    value: \"0000XXXXXXXX0000000000000384\"\n  }\n}\n }"
}

Mysqlx.Sql.StmtExecuteOk {
}


Notice: insert 2 doc without _id
--------------------------------

command ok

0 rows affected
Mysqlx.Notice.Frame {
  type: 3
  scope: LOCAL
  payload: "Mysqlx.Notice.SessionStateChanged { param: PRODUCED_MESSAGE\nvalue {\n  type: V_STRING\n  v_string {\n    value: \"Records: 2  Duplicates: 0  Warnings: 0\"\n  }\n}\n }"
}

Mysqlx.Notice.Frame {
  type: 3
  scope: LOCAL
  payload: "Mysqlx.Notice.SessionStateChanged { param: ROWS_AFFECTED\nvalue {\n  type: V_UINT\n  v_unsigned_int: 2\n}\n }"
}

Mysqlx.Notice.Frame {
  type: 3
  scope: LOCAL
  payload: "Mysqlx.Notice.SessionStateChanged { param: GENERATED_DOCUMENT_IDS\nvalue {\n  type: V_OCTETS\n  v_octets {\n    value: \"0000XXXXXXXX0000000000000385\"\n  }\n}\nvalue {\n  type: V_OCTETS\n  v_octets {\n    value: \"0000XXXXXXXX0000000000000386\"\n  }\n}\n }"
}

Mysqlx.Sql.StmtExecuteOk {
}


Notice: insert bad doc
----------------------

command ok

0 rows affected
Mysqlx.Error {
  severity: ERROR
  code: 3140
  msg: "Invalid JSON text: \"not a JSON text, may need CAST\" at position 0 in value for column \'coll.doc\'."
  sql_state: "22032"
}

Mysqlx.Ok {
  msg: "bye!"
}
ok
set global mysqlx_document_id_unique_prefix = DEFAULT;
DROP SCHEMA IF EXISTS xtest;
