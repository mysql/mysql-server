CREATE SCHEMA xtest;

Update by ARRAY_APPEND
======================

1 rows affected
Rows matched: 1  Changed: 1  Warnings: 0
Got expected error (code 0)

Update by ARRAY_INSERT
======================

1 rows affected
Rows matched: 1  Changed: 1  Warnings: 0
Got expected error (code 0)

Update by ARRAY_INSERT (invalid path)
=====================================
Got expected error: A path expression is not a path to a cell in an array. (code 3165)

Update by ITEM_REPLACE
======================

1 rows affected
Rows matched: 1  Changed: 1  Warnings: 0
Got expected error (code 0)

Update by ITEM_REPLACE non-existing member
==========================================

0 rows affected
Rows matched: 1  Changed: 0  Warnings: 0
Got expected error (code 0)

ITEM_SET to NULL
================

1 rows affected
Rows matched: 1  Changed: 1  Warnings: 0
Got expected error (code 0)

Update by ITEM_SET twice
========================

1 rows affected
Rows matched: 1  Changed: 1  Warnings: 0
Got expected error (code 0)

Update by ITEM_SET non-existing member
======================================

1 rows affected
Rows matched: 1  Changed: 1  Warnings: 0
Got expected error (code 0)

Update a valid document by _id (from field)
===========================================

1 rows affected
Rows matched: 1  Changed: 1  Warnings: 0
Got expected error (code 0)

Update document with illegal _id change
=======================================
Got expected error: Forbidden update operation on '$._id' member (code 5053)

Update non-existing document
============================

0 rows affected
Rows matched: 0  Changed: 0  Warnings: 0
Got expected error (code 0)

Update multiple documents
=========================
doc
{"_id": "1", "name": "banana", "price": 1.2}
{"_id": "2", "name": "apple", "price": 0.25}
{"_id": "3", "name": "tomato", "price": 1.8}
{"_id": "4", "name": "mango", "price": 3.14}
{"_id": "5", "name": "mango", "price": 0.9, "origin": ["mexico", "philippines"]}
command ok

4 rows affected
Rows matched: 4  Changed: 4  Warnings: 0
Got expected error (code 0)
doc
{"_id": "1", "name": "lemon", "price": 1.2}
{"_id": "2", "name": "lemon", "price": 0.25}
{"_id": "3", "name": "lemon", "price": 1.8}
{"_id": "4", "name": "lemon", "price": 3.14}
{"_id": "5", "name": "mango", "price": 0.9, "origin": ["mexico", "philippines"]}
command ok

Update on an invalid schema (bad)
=================================

1 rows affected
Got expected error: Table 'ytest.coll' doesn't exist (code 1146)

0 rows affected

Update on an invalid collection (bad)
=====================================
Got expected error: Table 'xtest.no_coll' doesn't exist (code 1146)

Update member with column (bad)
===============================
Got expected error: Invalid column name to update (code 5052)

Update member with table name (bad)
===================================
Got expected error: Invalid column name to update (code 5052)

Update member with schema name (bad)
====================================
Got expected error: Invalid column name to update (code 5052)

Update by SET (not allowed)
===========================
Got expected error: Invalid type of update operation for document (code 5051)

Update by ITEM_REMOVE
=====================

1 rows affected
Rows matched: 1  Changed: 1  Warnings: 0
Got expected error (code 0)

Multiple chained operations
===========================

1 rows affected
Rows matched: 1  Changed: 1  Warnings: 0
Got expected error (code 0)

Merge with another document
===========================

1 rows affected
Rows matched: 1  Changed: 1  Warnings: 0
Got expected error (code 0)

Merge with another document containing _id
==========================================

1 rows affected
Rows matched: 1  Changed: 1  Warnings: 0
Got expected error (code 0)

Merge with another document containing _id (differ root)
========================================================

1 rows affected
Rows matched: 1  Changed: 1  Warnings: 0
Got expected error (code 0)

Set item and merge with another document
========================================

1 rows affected
Rows matched: 1  Changed: 1  Warnings: 0
Got expected error (code 0)

Merge with another document and set item
========================================

1 rows affected
Rows matched: 1  Changed: 1  Warnings: 0
Got expected error (code 0)

Merge with NULL
===============
Got expected error: Document is missing a required field (code 5115)

Merge with non-document
=======================
Got expected error: Document is missing a required field (code 5115)

Merge with two documents
========================
doc
{"_id": "1", "name": "banana", "price": 1.2}
command ok

1 rows affected
Rows matched: 1  Changed: 1  Warnings: 0
Got expected error (code 0)

Merge with numeric literal
==========================
Got expected error: Invalid data for update operation on document collection table (code 5050)

Replace entire document (by empty source)
=========================================

1 rows affected
Rows matched: 1  Changed: 1  Warnings: 0
Got expected error (code 0)

Replace entire document (by member)
===================================

1 rows affected
Rows matched: 1  Changed: 1  Warnings: 0
Got expected error (code 0)

Replace entire document, no _id (by member)
===========================================

1 rows affected
Rows matched: 1  Changed: 1  Warnings: 0
Got expected error (code 0)

Replace entire document by NULL
===============================
Got expected error: Document is missing a required field (code 5115)

Set entire document (by empty source)
=====================================

1 rows affected
Rows matched: 1  Changed: 1  Warnings: 0
Got expected error (code 0)

Set entire document (by member)
===============================

1 rows affected
Rows matched: 1  Changed: 1  Warnings: 0
Got expected error (code 0)

Set entire document, no _id (by member)
=======================================
doc
{"_id": "1", "name": "banana", "price": 1.2}
command ok

1 rows affected
Rows matched: 1  Changed: 1  Warnings: 0
Got expected error (code 0)

Set entire document to NULL
===========================
Got expected error: Document is missing a required field (code 5115)

Merge literal to member
=======================
Got expected error: Unexpected source for ITEM_MERGE operation (code 5050)

Merge document to sub-document
==============================
Got expected error: Unexpected source for ITEM_MERGE operation (code 5050)

Merge document to member
========================
Got expected error: Unexpected source for ITEM_MERGE operation (code 5050)

Merge document to member of array
=================================
Got expected error: Unexpected source for ITEM_MERGE operation (code 5050)
Mysqlx.Ok {
  msg: "bye!"
}
ok
