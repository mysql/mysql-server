--------------
drop table if exists auto_incr_test,auto_incr_test2
--------------

Query OK, 0 rows affected

--------------
create table auto_incr_test (id int not null auto_increment, name char(40), timestamp timestamp, primary key (id))
--------------

Query OK, 0 rows affected

--------------
insert into auto_incr_test (name) values ("first record")
--------------

Query OK, 1 row affected

--------------
insert into auto_incr_test values (last_insert_id()+1,"second record",null)
--------------

Query OK, 1 row affected

--------------
insert into auto_incr_test (id,name) values (10,"tenth record")
--------------

Query OK, 1 row affected

--------------
insert into auto_incr_test values (0,"eleventh record",null)
--------------

Query OK, 1 row affected

--------------
insert into auto_incr_test values (last_insert_id()+1,"12","1997-01-01")
--------------

Query OK, 1 row affected

--------------
insert into auto_incr_test values (12,"this will not work",NULL)
--------------

ERROR 1062 at line 15: Duplicate entry '12' for key 1
--------------
replace into auto_incr_test values (12,"twelfth record",NULL)
--------------

Query OK, 2 rows affected

--------------
select * from auto_incr_test
--------------

id	name	timestamp
1	first record	19980817042654
2	second record	19980817042655
10	tenth record	19980817042655
11	eleventh record	19980817042655
12	twelfth record	19980817042655
5 rows in set

--------------
create table auto_incr_test2 (id int not null auto_increment, name char(40), primary key (id))
--------------

Query OK, 0 rows affected

--------------
insert into auto_incr_test2 select NULL,name from auto_incr_test
--------------

Query OK, 5 rows affected
Records: 5  Duplicates: 0  Warnings: 0

--------------
insert into auto_incr_test2 select id,name from auto_incr_test
--------------

Query OK, 3 rows affected
Records: 5  Duplicates: 2  Warnings: 0

--------------
replace into auto_incr_test2 select id,name from auto_incr_test
--------------

Query OK, 5 rows affected
Records: 5  Duplicates: 5  Warnings: 0

--------------
select * from auto_incr_test2
--------------

id	name
1	first record
2	second record
3	tenth record
4	eleventh record
5	twelfth record
10	tenth record
11	eleventh record
12	twelfth record
8 rows in set

--------------
drop table auto_incr_test,auto_incr_test2
--------------

Query OK, 0 rows affected

Bye
