CREATE SCHEMA xtest DEFAULT CHARSET 'utf8mb4';

Create_collection
=================

command ok

Insert documents into collection 'data'
=======================================

1 rows affected
Got expected error (code 0)

1 rows affected
Got expected error (code 0)

1 rows affected
Got expected error (code 0)

1 rows affected
Got expected error (code 0)

Create index on collection 'data'
=================================

command ok
## 1. Verify usage array index
## 1.1 with "cont_in (JSON_CONTAINS)
doc
{"_id": "003", "integer": [11, 12]}
command ok
Verify ["SELECT NO_INDEX_USED OR NO_GOOD_INDEX_USED as Result FROM performance_schema.events_statements_history WHERE THREAD_ID = XX ORDER BY event_id DESC LIMIT 1" returns in "Result" column, following value 0]
## 1. Verify usage array index
## 1.2 with "overlaps" (JSON_OVERLAPS)
doc
{"_id": "004", "integer": [13, 14, 15, 16]}
command ok
Verify ["SELECT NO_INDEX_USED OR NO_GOOD_INDEX_USED as Result FROM performance_schema.events_statements_history WHERE THREAD_ID = XX ORDER BY event_id DESC LIMIT 1" returns in "Result" column, following value 0]
Mysqlx.Ok {
  msg: "bye!"
}
ok
