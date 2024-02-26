CREATE TABLE t1 (id INT NOT NULL PRIMARY KEY) Engine=InnoDB;
INSERT INTO t1 (id) VALUES (1),(2),(3),(4),(5);
BEGIN;
SELECT * FROM t1 WHERE id = 1 FOR UPDATE;
id
1
BEGIN;
SELECT * FROM t1 WHERE id = 2 FOR UPDATE;
id
2
SET DEBUG_SYNC = 'lock_wait_will_wait SIGNAL con2_will_wait';
SELECT * FROM t1 WHERE id = 1 FOR UPDATE; ROLLBACK;
BEGIN;
SET DEBUG_SYNC = 'now WAIT_FOR con2_will_wait';
SELECT * FROM t1 WHERE id = 3 FOR UPDATE;
id
3
SET DEBUG_SYNC = 'lock_wait_will_wait SIGNAL con3_will_wait';
SELECT * FROM t1 WHERE id = 2 FOR UPDATE; ROLLBACK;
BEGIN;
SET DEBUG_SYNC = 'now WAIT_FOR con3_will_wait';
SELECT * FROM t1 WHERE id = 4 FOR UPDATE;
id
4
SET DEBUG_SYNC = 'lock_wait_will_wait SIGNAL con4_will_wait';
SELECT * FROM t1 WHERE id = 3 FOR UPDATE; ROLLBACK;
SET DEBUG_SYNC = 'now WAIT_FOR con4_will_wait';
SELECT * FROM t1 WHERE id = 4 FOR UPDATE;
ERROR 40001: Deadlock found when trying to get lock; try restarting transaction
id
1
id
2
id
3
SHOW ENGINE INNODB STATUS;
Type	Name	Status
InnoDB		
------------------------
%DATE% %THREAD_ID%
*** (1) TRANSACTION:
TRANSACTION %NUMBER%, ACTIVE %NUMBER% sec starting index read
mysql tables in use 1, locked 1
LOCK WAIT 3 lock struct(s), heap size %NUMBER%, 2 row lock(s)
MySQL thread id %NUMBER%, OS thread handle %NUMBER%, query id %NUMBER% %ADDRESS% %USER% statistics
SELECT * FROM t1 WHERE id = 1 FOR UPDATE

*** (1) HOLDS THE LOCK(S):
RECORD LOCKS space id %NUMBER% page no 4 n bits 72 index PRIMARY of table `test`.`t1` trx id %NUMBER% lock_mode X locks rec but not gap
Record lock, heap no 3 PHYSICAL RECORD: n_fields 3; compact format; info bits 0
 0: len 4; hex %HEXADECIMAL%; asc %ASC%;;
 1: len 6; hex %HEXADECIMAL%; asc %ASC%;;
 2: len 7; hex %HEXADECIMAL%; asc %ASC%;;


*** (1) WAITING FOR THIS LOCK TO BE GRANTED:
RECORD LOCKS space id %NUMBER% page no 4 n bits 72 index PRIMARY of table `test`.`t1` trx id %NUMBER% lock_mode X locks rec but not gap waiting
Record lock, heap no 2 PHYSICAL RECORD: n_fields 3; compact format; info bits 0
 0: len 4; hex %HEXADECIMAL%; asc %ASC%;;
 1: len 6; hex %HEXADECIMAL%; asc %ASC%;;
 2: len 7; hex %HEXADECIMAL%; asc %ASC%;;


*** (2) TRANSACTION:
TRANSACTION %NUMBER%, ACTIVE %NUMBER% sec starting index read
mysql tables in use 1, locked 1
LOCK WAIT 3 lock struct(s), heap size %NUMBER%, 2 row lock(s)
MySQL thread id %NUMBER%, OS thread handle %NUMBER%, query id %NUMBER% %ADDRESS% %USER% statistics
SELECT * FROM t1 WHERE id = 4 FOR UPDATE

*** (2) HOLDS THE LOCK(S):
RECORD LOCKS space id %NUMBER% page no 4 n bits 72 index PRIMARY of table `test`.`t1` trx id %NUMBER% lock_mode X locks rec but not gap
Record lock, heap no 2 PHYSICAL RECORD: n_fields 3; compact format; info bits 0
 0: len 4; hex %HEXADECIMAL%; asc %ASC%;;
 1: len 6; hex %HEXADECIMAL%; asc %ASC%;;
 2: len 7; hex %HEXADECIMAL%; asc %ASC%;;


*** (2) WAITING FOR THIS LOCK TO BE GRANTED:
RECORD LOCKS space id %NUMBER% page no 4 n bits 72 index PRIMARY of table `test`.`t1` trx id %NUMBER% lock_mode X locks rec but not gap waiting
Record lock, heap no 5 PHYSICAL RECORD: n_fields 3; compact format; info bits 0
 0: len 4; hex %HEXADECIMAL%; asc %ASC%;;
 1: len 6; hex %HEXADECIMAL%; asc %ASC%;;
 2: len 7; hex %HEXADECIMAL%; asc %ASC%;;


*** (3) TRANSACTION:
TRANSACTION %NUMBER%, ACTIVE %NUMBER% sec starting index read
mysql tables in use 1, locked 1
LOCK WAIT 3 lock struct(s), heap size %NUMBER%, 2 row lock(s)
MySQL thread id %NUMBER%, OS thread handle %NUMBER%, query id %NUMBER% %ADDRESS% %USER% statistics
SELECT * FROM t1 WHERE id = 3 FOR UPDATE

*** (3) HOLDS THE LOCK(S):
RECORD LOCKS space id %NUMBER% page no 4 n bits 72 index PRIMARY of table `test`.`t1` trx id %NUMBER% lock_mode X locks rec but not gap
Record lock, heap no 5 PHYSICAL RECORD: n_fields 3; compact format; info bits 0
 0: len 4; hex %HEXADECIMAL%; asc %ASC%;;
 1: len 6; hex %HEXADECIMAL%; asc %ASC%;;
 2: len 7; hex %HEXADECIMAL%; asc %ASC%;;


*** (3) WAITING FOR THIS LOCK TO BE GRANTED:
RECORD LOCKS space id %NUMBER% page no 4 n bits 72 index PRIMARY of table `test`.`t1` trx id %NUMBER% lock_mode X locks rec but not gap waiting
Record lock, heap no 4 PHYSICAL RECORD: n_fields 3; compact format; info bits 0
 0: len 4; hex %HEXADECIMAL%; asc %ASC%;;
 1: len 6; hex %HEXADECIMAL%; asc %ASC%;;
 2: len 7; hex %HEXADECIMAL%; asc %ASC%;;


*** (4) TRANSACTION:
TRANSACTION %NUMBER%, ACTIVE %NUMBER% sec starting index read
mysql tables in use 1, locked 1
LOCK WAIT 3 lock struct(s), heap size %NUMBER%, 2 row lock(s)
MySQL thread id %NUMBER%, OS thread handle %NUMBER%, query id %NUMBER% %ADDRESS% %USER% statistics
SELECT * FROM t1 WHERE id = 2 FOR UPDATE

*** (4) HOLDS THE LOCK(S):
RECORD LOCKS space id %NUMBER% page no 4 n bits 72 index PRIMARY of table `test`.`t1` trx id %NUMBER% lock_mode X locks rec but not gap
Record lock, heap no 4 PHYSICAL RECORD: n_fields 3; compact format; info bits 0
 0: len 4; hex %HEXADECIMAL%; asc %ASC%;;
 1: len 6; hex %HEXADECIMAL%; asc %ASC%;;
 2: len 7; hex %HEXADECIMAL%; asc %ASC%;;


*** (4) WAITING FOR THIS LOCK TO BE GRANTED:
RECORD LOCKS space id %NUMBER% page no 4 n bits 72 index PRIMARY of table `test`.`t1` trx id %NUMBER% lock_mode X locks rec but not gap waiting
Record lock, heap no 3 PHYSICAL RECORD: n_fields 3; compact format; info bits 0
 0: len 4; hex %HEXADECIMAL%; asc %ASC%;;
 1: len 6; hex %HEXADECIMAL%; asc %ASC%;;
 2: len 7; hex %HEXADECIMAL%; asc %ASC%;;

*** WE ROLL BACK TRANSACTION (2)
------------

CREATE TABLE t2 (id INT NOT NULL PRIMARY KEY) Engine=InnoDB;
INSERT INTO t2 (id) VALUES (1),(2),(3),(4),(5);
BEGIN;
SELECT * FROM t2 FOR UPDATE;
id
1
2
3
4
5
SELECT * FROM t1 WHERE id = 1 FOR UPDATE;
id
1
BEGIN;
SELECT * FROM t1 WHERE id = 2 FOR UPDATE;
id
2
SET DEBUG_SYNC = 'lock_wait_will_wait SIGNAL con2_will_wait';
SELECT * FROM t1 WHERE id = 1 FOR UPDATE; ROLLBACK;
BEGIN;
SET DEBUG_SYNC = 'now WAIT_FOR con2_will_wait';
SELECT * FROM t1 WHERE id = 3 FOR UPDATE;
id
3
SET DEBUG_SYNC = 'lock_wait_will_wait SIGNAL con3_will_wait';
SELECT * FROM t1 WHERE id = 2 FOR UPDATE; ROLLBACK;
BEGIN;
SET DEBUG_SYNC = 'now WAIT_FOR con3_will_wait';
SELECT * FROM t1 WHERE id = 4 FOR UPDATE;
id
4
SET DEBUG_SYNC = 'lock_wait_will_wait SIGNAL con4_will_wait';
SELECT * FROM t1 WHERE id = 3 FOR UPDATE; ROLLBACK;
SET DEBUG_SYNC = 'now WAIT_FOR con4_will_wait';
SELECT * FROM t1 WHERE id = 4 FOR UPDATE;
id
4
ROLLBACK;
ERROR 40001: Deadlock found when trying to get lock; try restarting transaction
id
2
id
3
SHOW ENGINE INNODB STATUS;
Type	Name	Status
InnoDB		
------------------------
%DATE% %THREAD_ID%
*** (1) TRANSACTION:
TRANSACTION %NUMBER%, ACTIVE %NUMBER% sec starting index read
mysql tables in use 1, locked 1
LOCK WAIT 3 lock struct(s), heap size %NUMBER%, 2 row lock(s)
MySQL thread id %NUMBER%, OS thread handle %NUMBER%, query id %NUMBER% %ADDRESS% %USER% statistics
SELECT * FROM t1 WHERE id = 1 FOR UPDATE

*** (1) HOLDS THE LOCK(S):
RECORD LOCKS space id %NUMBER% page no 4 n bits 72 index PRIMARY of table `test`.`t1` trx id %NUMBER% lock_mode X locks rec but not gap
Record lock, heap no 3 PHYSICAL RECORD: n_fields 3; compact format; info bits 0
 0: len 4; hex %HEXADECIMAL%; asc %ASC%;;
 1: len 6; hex %HEXADECIMAL%; asc %ASC%;;
 2: len 7; hex %HEXADECIMAL%; asc %ASC%;;


*** (1) WAITING FOR THIS LOCK TO BE GRANTED:
RECORD LOCKS space id %NUMBER% page no 4 n bits 72 index PRIMARY of table `test`.`t1` trx id %NUMBER% lock_mode X locks rec but not gap waiting
Record lock, heap no 2 PHYSICAL RECORD: n_fields 3; compact format; info bits 0
 0: len 4; hex %HEXADECIMAL%; asc %ASC%;;
 1: len 6; hex %HEXADECIMAL%; asc %ASC%;;
 2: len 7; hex %HEXADECIMAL%; asc %ASC%;;


*** (2) TRANSACTION:
TRANSACTION %NUMBER%, ACTIVE %NUMBER% sec starting index read
mysql tables in use 1, locked 1
LOCK WAIT 5 lock struct(s), heap size %NUMBER%, 8 row lock(s)
MySQL thread id %NUMBER%, OS thread handle %NUMBER%, query id %NUMBER% %ADDRESS% %USER% statistics
SELECT * FROM t1 WHERE id = 4 FOR UPDATE

*** (2) HOLDS THE LOCK(S):
RECORD LOCKS space id %NUMBER% page no 4 n bits 72 index PRIMARY of table `test`.`t1` trx id %NUMBER% lock_mode X locks rec but not gap
Record lock, heap no 2 PHYSICAL RECORD: n_fields 3; compact format; info bits 0
 0: len 4; hex %HEXADECIMAL%; asc %ASC%;;
 1: len 6; hex %HEXADECIMAL%; asc %ASC%;;
 2: len 7; hex %HEXADECIMAL%; asc %ASC%;;


*** (2) WAITING FOR THIS LOCK TO BE GRANTED:
RECORD LOCKS space id %NUMBER% page no 4 n bits 72 index PRIMARY of table `test`.`t1` trx id %NUMBER% lock_mode X locks rec but not gap waiting
Record lock, heap no 5 PHYSICAL RECORD: n_fields 3; compact format; info bits 0
 0: len 4; hex %HEXADECIMAL%; asc %ASC%;;
 1: len 6; hex %HEXADECIMAL%; asc %ASC%;;
 2: len 7; hex %HEXADECIMAL%; asc %ASC%;;


*** (3) TRANSACTION:
TRANSACTION %NUMBER%, ACTIVE %NUMBER% sec starting index read
mysql tables in use 1, locked 1
LOCK WAIT 3 lock struct(s), heap size %NUMBER%, 2 row lock(s)
MySQL thread id %NUMBER%, OS thread handle %NUMBER%, query id %NUMBER% %ADDRESS% %USER% statistics
SELECT * FROM t1 WHERE id = 3 FOR UPDATE

*** (3) HOLDS THE LOCK(S):
RECORD LOCKS space id %NUMBER% page no 4 n bits 72 index PRIMARY of table `test`.`t1` trx id %NUMBER% lock_mode X locks rec but not gap
Record lock, heap no 5 PHYSICAL RECORD: n_fields 3; compact format; info bits 0
 0: len 4; hex %HEXADECIMAL%; asc %ASC%;;
 1: len 6; hex %HEXADECIMAL%; asc %ASC%;;
 2: len 7; hex %HEXADECIMAL%; asc %ASC%;;


*** (3) WAITING FOR THIS LOCK TO BE GRANTED:
RECORD LOCKS space id %NUMBER% page no 4 n bits 72 index PRIMARY of table `test`.`t1` trx id %NUMBER% lock_mode X locks rec but not gap waiting
Record lock, heap no 4 PHYSICAL RECORD: n_fields 3; compact format; info bits 0
 0: len 4; hex %HEXADECIMAL%; asc %ASC%;;
 1: len 6; hex %HEXADECIMAL%; asc %ASC%;;
 2: len 7; hex %HEXADECIMAL%; asc %ASC%;;


*** (4) TRANSACTION:
TRANSACTION %NUMBER%, ACTIVE %NUMBER% sec starting index read
mysql tables in use 1, locked 1
LOCK WAIT 3 lock struct(s), heap size %NUMBER%, 2 row lock(s)
MySQL thread id %NUMBER%, OS thread handle %NUMBER%, query id %NUMBER% %ADDRESS% %USER% statistics
SELECT * FROM t1 WHERE id = 2 FOR UPDATE

*** (4) HOLDS THE LOCK(S):
RECORD LOCKS space id %NUMBER% page no 4 n bits 72 index PRIMARY of table `test`.`t1` trx id %NUMBER% lock_mode X locks rec but not gap
Record lock, heap no 4 PHYSICAL RECORD: n_fields 3; compact format; info bits 0
 0: len 4; hex %HEXADECIMAL%; asc %ASC%;;
 1: len 6; hex %HEXADECIMAL%; asc %ASC%;;
 2: len 7; hex %HEXADECIMAL%; asc %ASC%;;


*** (4) WAITING FOR THIS LOCK TO BE GRANTED:
RECORD LOCKS space id %NUMBER% page no 4 n bits 72 index PRIMARY of table `test`.`t1` trx id %NUMBER% lock_mode X locks rec but not gap waiting
Record lock, heap no 3 PHYSICAL RECORD: n_fields 3; compact format; info bits 0
 0: len 4; hex %HEXADECIMAL%; asc %ASC%;;
 1: len 6; hex %HEXADECIMAL%; asc %ASC%;;
 2: len 7; hex %HEXADECIMAL%; asc %ASC%;;

*** WE ROLL BACK TRANSACTION (1)
------------

DROP TABLE t2;
DROP TABLE t1;
