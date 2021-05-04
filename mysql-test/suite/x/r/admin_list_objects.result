CREATE SCHEMA xtest DEFAULT CHARSET 'utf8mb4';
CREATE TABLE xtest.mytable (a INT);
CREATE TABLE xtest.mytable2 (a INT, doc JSON);
CREATE VIEW xtest.myview AS SELECT 1;

command ok

0 rows affected

0 rows affected

0 rows affected
name	type
books	COLLECTION
fakecoll	VIEW
fakexview	VIEW
mytable	TABLE
mytable2	TABLE
myview	VIEW
xdoc	COLLECTION_VIEW
command ok
Got expected error: Unknown database 'invalid' (code 1049)
name	type
mytable	TABLE
mytable2	TABLE
command ok
name	type
command ok
Got expected error: No database selected (code 1046)

0 rows affected
name	type
books	COLLECTION
fakecoll	VIEW
fakexview	VIEW
mytable	TABLE
mytable2	TABLE
myview	VIEW
xdoc	COLLECTION_VIEW
command ok
name	type
fakecoll	VIEW
fakexview	VIEW
command ok

0 rows affected
Records: 0  Duplicates: 0  Warnings: 0
Field	Type	Null	Key	Default	Extra
doc	json	YES		null	
_id	varbinary(32)	NO	PRI	null	STORED GENERATED
_json_schema	json	YES		null	VIRTUAL GENERATED
nothing	int	YES		null	
0 rows affected
name	type
books	TABLE
command ok
name	type
books	TABLE
fakecoll	VIEW
fakexview	VIEW
mytable	TABLE
mytable2	TABLE
myview	VIEW
xdoc	COLLECTION_VIEW
command ok

0 rows affected
name	type
fakecoll	VIEW
fakexview	VIEW
mytable	TABLE
mytable2	TABLE
myview	VIEW
xdoc	COLLECTION_VIEW
command ok
name	type
processlist	VIEW
command ok

Collection with index
---------------------

command ok

command ok
name	type
fruit	COLLECTION
command ok

Collection with validation schema
---------------------------------

command ok

command ok
name	type
stuff	COLLECTION
command ok

Removing `_id` from collection
------------------------------

0 rows affected
Records: 0  Duplicates: 0  Warnings: 0
name	type
fruit	TABLE
command ok

Removing `_id` and index from collection
----------------------------------------

command ok
name	type
fruit	TABLE
command ok
Mysqlx.Ok {
  msg: "bye!"
}
ok
