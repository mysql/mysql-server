--------------
CREATE FUNCTION metaphon RETURNS STRING SONAME "udf_example.so"
--------------

Query OK, 0 rows affected

--------------
CREATE FUNCTION myfunc_double RETURNS REAL SONAME "udf_example.so"
--------------

Query OK, 0 rows affected

--------------
CREATE FUNCTION myfunc_int RETURNS INTEGER SONAME "udf_example.so"
--------------

Query OK, 0 rows affected

--------------
CREATE FUNCTION lookup RETURNS STRING SONAME "udf_example.so"
--------------

Query OK, 0 rows affected

--------------
CREATE FUNCTION reverse_lookup RETURNS STRING SONAME "udf_example.so"
--------------

Query OK, 0 rows affected

--------------
CREATE AGGREGATE FUNCTION avgcost RETURNS REAL SONAME "udf_example.so"
--------------

Query OK, 0 rows affected

--------------
select metaphon("hello")
--------------

metaphon("hello")
HL
1 row in set

--------------
select myfunc_double("hello","world")
--------------

myfunc_double("hello","world")
108.40
1 row in set

--------------
select myfunc_int(1,2,3),myfunc_int("1","11","111")
--------------

myfunc_int(1,2,3)	myfunc_int("1","11","111")
6	6
1 row in set

--------------
select lookup("localhost")
--------------

lookup("localhost")
127.0.0.1
1 row in set

--------------
select reverse_lookup("127.0.0.1")
--------------

reverse_lookup("127.0.0.1")
localhost
1 row in set

--------------
create temporary table t1 (a int,b double)
--------------

Query OK, 0 rows affected

--------------
insert into t1 values (1,5),(1,4),(2,8),(3,9),(4,11)
--------------

Query OK, 5 rows affected
Records: 0  Duplicates: 5  Warnings: 0

--------------
select avgcost(a,b) from t1
--------------

avgcost(a,b)
8.7273
1 row in set

--------------
select avgcost(a,b) from t1 group by a
--------------

avgcost(a,b)
4.5000
8.0000
9.0000
11.0000
4 rows in set

--------------
drop table t1
--------------

Query OK, 0 rows affected

--------------
DROP FUNCTION metaphon
--------------

Query OK, 0 rows affected

--------------
DROP FUNCTION myfunc_double
--------------

Query OK, 0 rows affected

--------------
DROP FUNCTION myfunc_int
--------------

Query OK, 0 rows affected

--------------
DROP FUNCTION lookup
--------------

Query OK, 0 rows affected

--------------
DROP FUNCTION reverse_lookup
--------------

Query OK, 0 rows affected

--------------
DROP FUNCTION avgcost
--------------

Query OK, 0 rows affected

Bye
