#WL 17024 - Activate triggers on referencing tables during foreign key CASCADE
#FR 2 is covered in foreign_key_variable.test.
#FR 9 is covered in foreign_key_binlog.test.
#FR 10 is covered in rpl_foreign_key_sql_child_trig.test
--echo # FR 1) Triggers must be fired for foreign key CASCADE action in SQL Layer
--echo # Foreign Key Handling
--echo # FR 3) Parent table containing multiple foreign keys:
--echo # Create parent table and two child tables referencing with foreign key constraints.
--echo # create triggers on each table which insert into logtable(tablename and operation
--echo # type) and verify them.
SET ENABLE_CASCADE_TRIGGERS = ON;
CREATE TABLE logtable (action VARCHAR(128), oldval INT, newval INT);
CREATE TABLE parent(id1 INT PRIMARY KEY, id2 INT);
CREATE TABLE child(idd1 INT PRIMARY KEY, idd2 INT,
  FOREIGN KEY (idd2) REFERENCES parent(id1) ON UPDATE CASCADE ON DELETE CASCADE);
CREATE TABLE childn(idd1 INT PRIMARY KEY, idd2 INT,
  FOREIGN KEY (idd2) REFERENCES parent(id1) ON UPDATE SET NULL ON DELETE SET NULL);
INSERT INTO parent VALUES (10, 10), (20, 20), (30, 30);
INSERT INTO child VALUES (10, 10), (20, 20), (30, 30);
INSERT INTO childn VALUES (10, 10), (20, 20), (30, 30);

delimiter //;
CREATE TRIGGER trg1 BEFORE DELETE ON child
FOR EACH ROW BEGIN
    INSERT INTO logtable VALUES("child-BEFORE-DELETE", OLD.idd2, 0);
END//

CREATE TRIGGER trg2 AFTER DELETE ON child
FOR EACH ROW BEGIN
    INSERT INTO logtable VALUES("child-AFTER-DELETE", OLD.idd2, 0);
END//

CREATE TRIGGER trg3 BEFORE UPDATE ON child
FOR EACH ROW BEGIN
    INSERT INTO logtable VALUES("child-BEFORE-UPDATE", OLD.idd2, NEW.idd2);
END//

CREATE TRIGGER trg4 AFTER UPDATE ON child
FOR EACH ROW BEGIN
    INSERT INTO logtable VALUES("child-AFTER-UPDATE", OLD.idd2, NEW.idd2);
END//

CREATE TRIGGER trg11 BEFORE DELETE ON childn
FOR EACH ROW BEGIN
    INSERT INTO logtable VALUES("childn-BEFORE-DELETE", OLD.idd2, 0);
END//

CREATE TRIGGER trg21 AFTER DELETE ON childn
FOR EACH ROW BEGIN
    INSERT INTO logtable VALUES("childn-AFTER-DELETE", OLD.idd2, 0);
END//

CREATE TRIGGER trg31 BEFORE UPDATE ON childn
FOR EACH ROW BEGIN
    INSERT INTO logtable VALUES("childn-BEFORE-UPDATE", OLD.idd2, NEW.idd2);
END//

CREATE TRIGGER trg41 AFTER UPDATE ON childn
FOR EACH ROW BEGIN
    INSERT INTO logtable VALUES("childn-AFTER-UPDATE", OLD.idd2, NEW.idd2);
END//
delimiter ;//

--echo # FR 1.1: ON DELETE CASCADE:
--echo # During DELETE operation on parent table, DELETE triggers on child
--echo # tables must fire for each deleted row.
--echo # FR 1.3: ON DELETE SET NULL:
--echo # During DELETE operation on parent table, UPDATE triggers on childn
--echo # tables must fire for each deleted row.
DELETE FROM parent WHERE id1=20;
SELECT * FROM logtable;
DELETE FROM logtable;

--echo # FR 1.2: ON UPDATE CASCADE:
--echo # During UPDATE operation on parent table, UPDATE triggers on child
--echo # tables must fire for each deleted row.
--echo # FR 1.4: ON UPDATE SET NULL:
--echo # During UPDATE operation on parent table, UPDATE triggers on childn
--echo # tables must fire for each deleted row.
UPDATE parent SET id1 = 40 where id1 = 10;
SELECT * FROM logtable;

SELECT * FROM parent;
SELECT * FROM child order by idd1;
SELECT * FROM childn;
DROP TABLE child, childn, parent, logtable;

--echo # FR 4: Child table containing multiple triggers:
--echo # FR 4.1: If child table contains multiple triggers, all triggers on
--echo # child table must be fired.
--echo # FR 4.2: Triggers must be executed in the defined execution order.

CREATE TABLE logtable (id INT PRIMARY KEY AUTO_INCREMENT, tbl_name VARCHAR(16));
CREATE TABLE t1 (f1 INT PRIMARY KEY);
CREATE TABLE t2 (f1 INT UNIQUE REFERENCES t1(f1) ON DELETE CASCADE);
INSERT INTO t1 VALUES (1), (2), (3), (4), (5);
INSERT INTO t2 VALUES (1), (2), (3), (4), (5);
CREATE TRIGGER t2_trig1 BEFORE DELETE ON t2
    FOR EACH ROW INSERT INTO logtable(tbl_name)
    VALUES ('t2_trig1_before');
CREATE TRIGGER t2_trig2 BEFORE DELETE ON t2
    FOR EACH ROW INSERT INTO logtable(tbl_name)
    VALUES ('t2_trig2-before');
CREATE TRIGGER t2_trig3 AFTER DELETE ON t2
    FOR EACH ROW INSERT INTO logtable(tbl_name)
    VALUES ('t2_trig3_after');
CREATE TRIGGER t2_trig4 AFTER DELETE ON t2
    FOR EACH ROW INSERT INTO logtable(tbl_name)
    VALUES ('t2_trig4-after');

DELETE FROM t1 WHERE f1 = 1;
SELECT * FROM logtable;

DROP TABLE logtable, t1, t2;

--echo # FR 5: Trigger execution order in case of multi level foreign keys:
--echo # Create parent table and three level of child tables.
--echo # create triggers on each table and insert operation and trigger type
--echo # into log table and verify them.
--echo # FR 5.1: BEFORE triggers must be fired top down order
--echo # FR 5.2: AFTER triggers must be fired bottom up order
--echo # FR 6: In trigger definition refer OLD and NEW values
--echo # FR 6.1: In UPDATE trigger definition refer OLD and NEW values
--echo # FR 6.2: In DELETE trigger definition refer OLD values

CREATE TABLE logtable (action VARCHAR(128), oldval INT, newval INT);
CREATE TABLE parent(id1 INT PRIMARY KEY, id2 INT);
CREATE TABLE child(idd1 INT PRIMARY KEY, idd2 INT, UNIQUE(idd2),
  FOREIGN KEY (idd2) REFERENCES parent(id1) ON UPDATE CASCADE ON DELETE CASCADE);
CREATE TABLE gchild(idd1 INT PRIMARY KEY, idd2 INT, UNIQUE(idd2),
  FOREIGN KEY (idd2) REFERENCES child(idd2) ON UPDATE CASCADE ON DELETE CASCADE);
CREATE TABLE ggchild(idd1 INT PRIMARY KEY, idd2 INT, UNIQUE(idd2),
  FOREIGN KEY (idd2) REFERENCES gchild(idd2) ON UPDATE CASCADE ON DELETE CASCADE);
INSERT INTO parent VALUES (10, 10), (20, 20), (30, 30);
INSERT INTO child VALUES (10, 10), (20, 20), (30, 30);
INSERT INTO gchild VALUES (10, 10), (20, 20), (30, 30);
INSERT INTO ggchild VALUES (10, 10), (20, 20), (30, 30);
delimiter //;
CREATE TRIGGER trg1 BEFORE DELETE ON child
FOR EACH ROW BEGIN
    INSERT INTO logtable VALUES("child-BEFORE-DELETE", OLD.idd2, 0);
END//

CREATE TRIGGER trg2 AFTER DELETE ON child
FOR EACH ROW BEGIN
    INSERT INTO logtable VALUES("child-AFTER-DELETE", OLD.idd2, 0);
END//

CREATE TRIGGER trg3 BEFORE UPDATE ON child
FOR EACH ROW BEGIN
    INSERT INTO logtable VALUES("child-BEFORE-UPDATE", OLD.idd2, NEW.idd2);
END//

CREATE TRIGGER trg4 AFTER UPDATE ON child
FOR EACH ROW BEGIN
    INSERT INTO logtable VALUES("child-AFTER-UPDATE", OLD.idd2, NEW.idd2);
END//

CREATE TRIGGER trg5 BEFORE DELETE ON gchild
FOR EACH ROW BEGIN
    INSERT INTO logtable VALUES("gchild-BEFORE-DELETE", OLD.idd2, 0);
END//

CREATE TRIGGER trg6 AFTER DELETE ON gchild
FOR EACH ROW BEGIN
    INSERT INTO logtable VALUES("gchild-AFTER-DELETE", OLD.idd2, 0);
END//

CREATE TRIGGER trg7 BEFORE UPDATE ON gchild
FOR EACH ROW BEGIN
    INSERT INTO logtable VALUES("gchild-BEFORE-UPDATE", OLD.idd2, NEW.idd2);
END//

CREATE TRIGGER trg8 AFTER UPDATE ON gchild
FOR EACH ROW BEGIN
    INSERT INTO logtable VALUES("gchild-AFTER-UPDATE", OLD.idd2, NEW.idd2);
END//

CREATE TRIGGER trg9 BEFORE DELETE ON ggchild
FOR EACH ROW BEGIN
    INSERT INTO logtable VALUES("ggchild-BEFORE-DELETE", OLD.idd2, 0);
END//

CREATE TRIGGER trg10 AFTER DELETE ON ggchild
FOR EACH ROW BEGIN
    INSERT INTO logtable VALUES("ggchild-AFTER-DELETE", OLD.idd2, 0);
END//

CREATE TRIGGER trg11 BEFORE UPDATE ON ggchild
FOR EACH ROW BEGIN
    INSERT INTO logtable VALUES("ggchild-BEFORE-UPDATE", OLD.idd2, NEW.idd2);
END//

CREATE TRIGGER trg12 AFTER UPDATE ON ggchild
FOR EACH ROW BEGIN
    INSERT INTO logtable VALUES("ggchild-AFTER-UPDATE", OLD.idd2, NEW.idd2);
END//

CREATE TRIGGER trg13 BEFORE DELETE ON parent
FOR EACH ROW BEGIN
    INSERT INTO logtable VALUES("parent-BEFORE-DELETE", OLD.id1, 0);
END//

CREATE TRIGGER trg14 AFTER DELETE ON parent
FOR EACH ROW BEGIN
    INSERT INTO logtable VALUES("parent-AFTER-DELETE", OLD.id1, 0);
END//

CREATE TRIGGER trg15 BEFORE UPDATE ON parent
FOR EACH ROW BEGIN
    INSERT INTO logtable VALUES("parent-BEFORE-UPDATE", OLD.id1, NEW.id1);
END//

CREATE TRIGGER trg16 AFTER UPDATE ON parent
FOR EACH ROW BEGIN
    INSERT INTO logtable VALUES("parent-AFTER-UPDATE", OLD.id1, NEW.id1);
END//

delimiter ;//

--echo Inspecting trigger firing sequence for DELETE CASCADE
DELETE FROM parent WHERE id1=20;
SELECT * FROM logtable;
SELECT * FROM parent;
SELECT * FROM child order by idd1;
SELECT * FROM gchild order by idd1;
SELECT * FROM ggchild order by idd1;

DELETE FROM logtable;
--echo Inspecting trigger firing sequence for UPDATE CASCADE
UPDATE parent SET id1 = 40 where id1 = 10;
SELECT * FROM logtable;
SELECT * FROM parent;
SELECT * FROM child order by idd1;
SELECT * FROM gchild order by idd1;
SELECT * FROM ggchild order by idd1;

DELETE FROM parent;
DELETE FROM logtable;
--echo # FR 7: Error handling must roll back all operations.
--echo Create utable to generate duplicate key error in trigger definition
CREATE TABLE utable(f1 INT PRIMARY KEY);
INSERT INTO utable VALUES(1);
--echo Reinsert records into tables
INSERT INTO parent VALUES (10, 10), (20, 20), (30, 30);
INSERT INTO child VALUES (10, 10), (20, 20), (30, 30);
INSERT INTO gchild VALUES (10, 10), (20, 20), (30, 30);
INSERT INTO ggchild VALUES (10, 10), (20, 20), (30, 30);

delimiter //;
CREATE TRIGGER trg21 BEFORE DELETE ON gchild
FOR EACH ROW BEGIN
    INSERT INTO logtable VALUES("gchild-BEFORE-DELETE", OLD.idd2, 0);
    INSERT INTO utable VALUES(1);
END//

CREATE TRIGGER trg22 AFTER UPDATE ON ggchild
FOR EACH ROW BEGIN
    INSERT INTO logtable VALUES("ggchild-BEFORE-UPDATE", OLD.idd2, 0);
    SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Forced Failure on ggchild';
END//

delimiter ;//

--echo # FR 7.1: If trigger execution fails or signals error, all operations
--echo # must be rolled back.
--echo # Inspecting trigger firing with failure in between for UPDATE CASCADE
--error ER_SIGNAL_EXCEPTION
UPDATE parent SET id1 = 40 where id1 = 10;
SELECT * FROM logtable;
SELECT * FROM child;
SELECT * FROM ggchild;

--echo # FR 7.2: Error in Multi level cascade should be rolled back.
--echo # Inspecting trigger firing with failure in between for DELETE CASCADE
--error ER_DUP_ENTRY
DELETE FROM parent WHERE id1=20;
SELECT * FROM logtable;
SELECT * FROM child;
SELECT * FROM ggchild;

DROP TRIGGER trg21;
delimiter //;
CREATE TRIGGER trg21 BEFORE DELETE ON gchild
FOR EACH ROW BEGIN
    DECLARE CONTINUE HANDLER FOR 1062
      INSERT INTO logtable VALUES("gchild-BEFORE-DELETE-DUPLICATE", 0, 0);
      INSERT INTO logtable VALUES("gchild-BEFORE-DELETE", OLD.idd2, 0);
      INSERT INTO utable VALUES(1);
END//

delimiter ;//
--echo #DUPLICATE KEY is handled so below delete should succeed
DELETE FROM parent WHERE id1=20;
SELECT * FROM logtable;
SELECT * FROM child;
SELECT * FROM ggchild;

DROP TABLE child, gchild, ggchild, parent, logtable, utable;

--echo # FR 8: Recursion and loop prevention
--echo # FR 8.1: For self-referencing foreign keys, triggers must fire on rows
--echo # in the same table affected by the cascade.
CREATE TABLE self (pk INT PRIMARY KEY, fk1 INT, FOREIGN KEY (fk1) REFERENCES self (pk) ON DELETE CASCADE);
CREATE TABLE logtable (id INT PRIMARY KEY AUTO_INCREMENT, action VARCHAR(128), oldval INT, newval INT);
INSERT INTO self VALUES (1, NULL), (2, 1), (3, 2);
delimiter //;
CREATE TRIGGER trg1 BEFORE DELETE ON self
FOR EACH ROW BEGIN
    INSERT INTO logtable(action, oldval) VALUES("self-BEFORE-DELETE", OLD.pk);
END//

CREATE TRIGGER trg2 AFTER DELETE ON self
FOR EACH ROW BEGIN
    INSERT INTO logtable(action, oldval) VALUES("self-AFTER-DELETE", OLD.pk);
END//
delimiter ;//
DELETE FROM self WHERE pk = 1;
SELECT * FROM self;
SELECT * FROM logtable;
DROP TABLE self, logtable;

--echo # FR 8.1.1: For self-referencing foreign keys, triggers must fire on rows
--echo # in the same table affected by the cascade for ON DELETE SET NULL.
--echo # UPDATE trigger should be fired.
CREATE TABLE self (pk INT PRIMARY KEY, fk1 INT, FOREIGN KEY (fk1) REFERENCES self (pk) ON DELETE SET NULL);
CREATE TABLE logtable (id INT PRIMARY KEY AUTO_INCREMENT, action VARCHAR(128), oldval INT, newval INT);
INSERT INTO self VALUES (1, NULL), (2, 1), (3, 2);
delimiter //;
CREATE TRIGGER trg1 BEFORE DELETE ON self
FOR EACH ROW BEGIN
    INSERT INTO logtable(action, oldval) VALUES("self-BEFORE-DELETE", OLD.pk);
END//

CREATE TRIGGER trg11 BEFORE UPDATE ON self
FOR EACH ROW BEGIN
    INSERT INTO logtable(action, oldval, newval) VALUES("self-BEFORE-UPDATE", OLD.fk1, NEW.fk1);
END//

CREATE TRIGGER trg22 AFTER UPDATE ON self
FOR EACH ROW BEGIN
    INSERT INTO logtable(action, oldval, newval) VALUES("self-AFTER-UPDATE", OLD.fk1, NEW.fk1);
END//

CREATE TRIGGER trg2 AFTER DELETE ON self
FOR EACH ROW BEGIN
    INSERT INTO logtable(action, oldval) VALUES("self-AFTER-DELETE", OLD.pk);
END//
delimiter ;//
DELETE FROM self WHERE pk = 1;
SELECT * FROM self;
SELECT * FROM logtable;
DROP TABLE self, logtable;

--echo # FR 8.1.2: For self-referencing foreign keys, triggers must fire on rows
--echo # in the same table affected by the cascade for ON DELETE CASCADE.
--echo # DELETE trigger should be fired.
CREATE TABLE self (pk INT PRIMARY KEY, fk1 INT, FOREIGN KEY (fk1) REFERENCES self (pk) ON DELETE CASCADE);
CREATE TABLE logtable (id INT PRIMARY KEY AUTO_INCREMENT, action VARCHAR(128), oldval INT, newval INT);
INSERT INTO self VALUES (1, NULL), (2, 1), (3, 2);
delimiter //;
CREATE TRIGGER trg1 BEFORE DELETE ON self
FOR EACH ROW BEGIN
    INSERT INTO logtable(action, oldval) VALUES("self-BEFORE-DELETE", OLD.pk);
END//

CREATE TRIGGER trg11 BEFORE UPDATE ON self
FOR EACH ROW BEGIN
    INSERT INTO logtable(action, oldval, newval) VALUES("self-BEFORE-UPDATE", OLD.fk1, NEW.fk1);
END//

CREATE TRIGGER trg22 AFTER UPDATE ON self
FOR EACH ROW BEGIN
    INSERT INTO logtable(action, oldval, newval) VALUES("self-AFTER-UPDATE", OLD.fk1, NEW.fk1);
END//

CREATE TRIGGER trg2 AFTER DELETE ON self
FOR EACH ROW BEGIN
    INSERT INTO logtable(action, oldval) VALUES("self-AFTER-DELETE", OLD.pk);
END//
delimiter ;//
DELETE FROM self WHERE pk = 1;
SELECT * FROM self;
SELECT * FROM logtable;
DROP TABLE self, logtable;

--echo # FR 8.2: Circular foreign key must succeed and triggers should be fired
--echo # only once for tables involved in circular foreign key.
SET FOREIGN_KEY_CHECKS=0;
CREATE TABLE t1(cid INT PRIMARY KEY,  eid INT, FOREIGN KEY (eid) REFERENCES t2(eid) ON DELETE CASCADE ON UPDATE CASCADE);
CREATE TABLE t2(eid INT PRIMARY KEY, cid INT, FOREIGN KEY (cid) REFERENCES t1(cid) ON DELETE CASCADE ON UPDATE CASCADE);
CREATE TABLE logtable (id INT PRIMARY KEY AUTO_INCREMENT, action VARCHAR(128), oldval INT, newval INT);
INSERT INTO t1 VALUES (1, 10), (2, 20);
INSERT INTO t2 VALUES (10, 1), (20, 2);
delimiter //;
CREATE TRIGGER trg1 BEFORE DELETE ON t1
FOR EACH ROW BEGIN
    INSERT INTO logtable VALUES(NULL, "t1-BEFORE-DELETE", OLD.cid, 0);
END//

CREATE TRIGGER trg2 AFTER DELETE ON t1
FOR EACH ROW BEGIN
    INSERT INTO logtable VALUES(NULL, "t1-AFTER-DELETE", OLD.cid, 0);
END//

CREATE TRIGGER trg3 BEFORE DELETE ON t2
FOR EACH ROW BEGIN
    INSERT INTO logtable VALUES(NULL, "t2-BEFORE-DELETE", OLD.cid, 0);
END//

CREATE TRIGGER trg4 AFTER DELETE ON t2
FOR EACH ROW BEGIN
    INSERT INTO logtable VALUES(NULL, "t2-AFTER-DELETE", OLD.cid, 0);
END//

CREATE TRIGGER trg5 BEFORE UPDATE ON t1
FOR EACH ROW BEGIN
    INSERT INTO logtable VALUES(NULL, "t1-BEFORE-UPDATE", OLD.cid, NEW.cid);
END//

CREATE TRIGGER trg6 BEFORE UPDATE ON t2
FOR EACH ROW BEGIN
    INSERT INTO logtable VALUES(NULL, "t2-BEFORE-UPDATE", OLD.cid, NEW.cid);
END//

CREATE TRIGGER trg7 AFTER UPDATE ON t1
FOR EACH ROW BEGIN
    INSERT INTO logtable VALUES(NULL, "t1-AFTER-UPDATE", OLD.cid, NEW.cid);
END//

CREATE TRIGGER trg8 AFTER UPDATE ON t2
FOR EACH ROW BEGIN
    INSERT INTO logtable VALUES(NULL, "t2-AFTER-UPDATE", OLD.cid, NEW.cid);
END//

delimiter ;//

SET FOREIGN_KEY_CHECKS=1;
DELETE FROM logtable;
DELETE FROM t1 WHERE cid=1;
SELECT * FROM t1;
SELECT * FROM t2;
SELECT * FROM logtable;
DELETE FROM logtable;
UPDATE t1 SET cid=3 WHERE cid=2;
SELECT * FROM t1;
SELECT * FROM t2;
SELECT * FROM logtable;
DROP TABLE t1, t2, logtable;

--echo # FR 8.3:  Cascade-induced trigger execution must not allow infinite loops.
--echo # FR 8.3.1: Self referencing ON DELETE CASCADE with INSERT to same table
--echo # in trigger definition should give error
CREATE TABLE emp (id INT PRIMARY KEY, mgr_id INT, name VARCHAR(20),
log VARCHAR(100), FOREIGN KEY (mgr_id) REFERENCES emp(id) ON DELETE CASCADE);

delimiter //;
CREATE TRIGGER emp_delete AFTER DELETE ON emp
FOR EACH ROW
BEGIN
INSERT INTO emp (id, name, mgr_id, log) VALUES (-(OLD.id), "deleted", NULL, CONCAT("Deleted by trigger: ", OLD.id));
END//
delimiter ;//

INSERT INTO emp VALUES (1, NULL, 'CEO', NULL);
INSERT INTO emp VALUES (2, 1, 'Direct1', NULL);

--error ER_CANT_UPDATE_USED_TABLE_IN_SF_OR_TRG
DELETE FROM emp WHERE id = 1;
SELECT * FROM emp;
DROP TABLE emp;

--echo # FR 8.3.2: Parent trigger definition containing child table with CASCADE
--echo # should pass with cascade firing child triggers
CREATE TABLE logtable (action VARCHAR(128), oldval INT, newval INT);
CREATE TABLE parent(f1 INT PRIMARY KEY);
CREATE TABLE child(f1 INT PRIMARY KEY, f2 INT,
  FOREIGN KEY (f2) REFERENCES parent(f1) ON UPDATE CASCADE ON DELETE CASCADE);
INSERT INTO parent VALUES (10), (20), (30);
INSERT INTO child VALUES (10, 10);
INSERT INTO child VALUES (20, 20);
INSERT INTO child VALUES (30, 30);

delimiter //;
CREATE TRIGGER trg1 AFTER DELETE ON parent
FOR EACH ROW BEGIN
    INSERT INTO logtable VALUES("parent-AFTER-DELETE", OLD.f1, 0);
    DELETE FROM child where f2=OLD.f1;
END//

CREATE TRIGGER trg2 AFTER UPDATE ON parent
FOR EACH ROW BEGIN
    INSERT INTO logtable VALUES("parent-AFTER-UPDATE", OLD.f1, NEW.f1);
    UPDATE child SET f2=NEW.f1 where f2=OLD.f1;
END//

CREATE TRIGGER trg3 AFTER UPDATE ON child
FOR EACH ROW BEGIN
    INSERT INTO logtable VALUES("child-AFTER-UPDATE", OLD.f2, NEW.f2);
END//

CREATE TRIGGER trg4 AFTER DELETE ON child
FOR EACH ROW BEGIN
    INSERT INTO logtable VALUES("child-AFTER-DELETE", OLD.f2, 0);
END//
delimiter ;//

--echo invoking delete trigger with child table containing FK cascade
--error ER_CANT_UPDATE_USED_TABLE_IN_SF_OR_TRG
DELETE FROM parent where f1 = 10;
--error ER_CANT_UPDATE_USED_TABLE_IN_SF_OR_TRG
UPDATE parent SET f1 = 40 where f1 = 20;
SELECT * FROM parent;
SELECT * FROM child;
SELECT * FROM logtable;
DROP TABLE logtable, parent, child;

--echo # FR 8.3.3: Child trigger definition containing parent table with CASCADE
--echo # should not lead to infinite loop and report an error
CREATE TABLE logtable (action VARCHAR(128), oldval INT, newval INT);
CREATE TABLE parent(f1 INT PRIMARY KEY);
CREATE TABLE child(f1 INT PRIMARY KEY, f2 INT,
  FOREIGN KEY (f2) REFERENCES parent(f1) ON UPDATE CASCADE ON DELETE CASCADE);
INSERT INTO parent VALUES (10), (20), (30);
INSERT INTO child VALUES (10, 10), (20, 20), (30, 30);

delimiter //;
CREATE TRIGGER trg1 AFTER DELETE ON parent
FOR EACH ROW BEGIN
    INSERT INTO logtable VALUES("parent-AFTER-DELETE", OLD.f1, 0);
END//

CREATE TRIGGER trg2 AFTER UPDATE ON parent
FOR EACH ROW BEGIN
    INSERT INTO logtable VALUES("parent-AFTER-UPDATE", OLD.f1, NEW.f1);
END//

CREATE TRIGGER trg3 AFTER UPDATE ON child
FOR EACH ROW BEGIN
    INSERT INTO logtable VALUES("child-AFTER-UPDATE", OLD.f2, NEW.f2);
    UPDATE parent SET f1=NEW.f2 where f1=OLD.f2;
END//

CREATE TRIGGER trg4 AFTER DELETE ON child
FOR EACH ROW BEGIN
    INSERT INTO logtable VALUES("child-AFTER-DELETE", OLD.f2, 0);
    DELETE FROM parent where f1=OLD.f2;
END//
delimiter ;//

--echo invoking delete trigger with table containing fk cascade
--error ER_CANT_UPDATE_USED_TABLE_IN_SF_OR_TRG
DELETE FROM parent where f1 = 10;

--error ER_CANT_UPDATE_USED_TABLE_IN_SF_OR_TRG
UPDATE parent SET f1 = 40 where f1 = 20;

#drop trigger. calling update should succeed.
DROP TRIGGER trg3;
UPDATE parent SET f1 = f1 + 1;

#recreate trigger on child table with INSERT on parent table
#and verify it fails
delimiter //;
CREATE TRIGGER trg5 BEFORE UPDATE ON child
FOR EACH ROW BEGIN
    INSERT INTO logtable VALUES("child-BEFORE-UPDATE", OLD.f2, NEW.f2);
    INSERT INTO parent VALUES(100);
END//
delimiter ;//

--error ER_CANT_UPDATE_USED_TABLE_IN_SF_OR_TRG
UPDATE parent SET f1 = f1 + 1;

--error ER_CANT_UPDATE_USED_TABLE_IN_FK_CASCADE
DELETE FROM child where f2 = 11;

SELECT * FROM parent;
SELECT * FROM child;
SELECT * FROM logtable;
DROP TABLE logtable, parent, child;


--echo # FR 8.4: If cascades exceed the maximum depth (i.e. 15), then error
--echo # ER_FK_DEPTH_EXCEEDED must be reported and the statements executed
--echo # must be rolled back.
CALL mtr.add_suppression(
    "Cannot delete/update rows with cascading foreign key constraints that exceed max depth of 15. "
    "Please drop excessive foreign constraints and try again"
);

CREATE TABLE logtable (id INT PRIMARY KEY AUTO_INCREMENT, tbl_name VARCHAR(16), operation VARCHAR(16));
CREATE TABLE t1 (f1 INT PRIMARY KEY);
CREATE TABLE t2 (f1 INT UNIQUE REFERENCES t1(f1) ON DELETE CASCADE);
CREATE TABLE t3 (f1 INT UNIQUE REFERENCES t2(f1) ON DELETE CASCADE);
CREATE TABLE t4 (f1 INT UNIQUE REFERENCES t3(f1) ON DELETE CASCADE);
CREATE TABLE t5 (f1 INT UNIQUE REFERENCES t4(f1) ON DELETE CASCADE);
CREATE TABLE t6 (f1 INT UNIQUE REFERENCES t5(f1) ON DELETE CASCADE);
CREATE TABLE t7 (f1 INT UNIQUE REFERENCES t6(f1) ON DELETE CASCADE);
CREATE TABLE t8 (f1 INT UNIQUE REFERENCES t7(f1) ON DELETE CASCADE);
CREATE TABLE t9 (f1 INT UNIQUE REFERENCES t8(f1) ON DELETE CASCADE);
CREATE TABLE t10 (f1 INT UNIQUE REFERENCES t9(f1) ON DELETE CASCADE);
CREATE TABLE t11 (f1 INT UNIQUE REFERENCES t10(f1) ON DELETE CASCADE);
CREATE TABLE t12 (f1 INT UNIQUE REFERENCES t11(f1) ON DELETE CASCADE);
CREATE TABLE t13 (f1 INT UNIQUE REFERENCES t12(f1) ON DELETE CASCADE);
CREATE TABLE t14 (f1 INT UNIQUE REFERENCES t13(f1) ON DELETE CASCADE);
CREATE TABLE t15 (f1 INT UNIQUE REFERENCES t14(f1) ON DELETE CASCADE);
CREATE TABLE t16 (f1 INT UNIQUE REFERENCES t15(f1) ON DELETE CASCADE);
CREATE TABLE t17 (f1 INT UNIQUE REFERENCES t16(f1) ON DELETE CASCADE);
INSERT INTO t1 VALUES (1), (2);
INSERT INTO t2 VALUES (1), (2);
INSERT INTO t3 VALUES (1), (2);
INSERT INTO t4 VALUES (1), (2);
INSERT INTO t5 VALUES (1), (2);
INSERT INTO t6 VALUES (1), (2);
INSERT INTO t7 VALUES (1), (2);
INSERT INTO t8 VALUES (1), (2);
INSERT INTO t9 VALUES (1), (2);
INSERT INTO t10 VALUES (1), (2);
INSERT INTO t11 VALUES (1), (2);
INSERT INTO t12 VALUES (1), (2);
INSERT INTO t13 VALUES (1), (2);
INSERT INTO t14 VALUES (1), (2);
INSERT INTO t15 VALUES (1), (2);
INSERT INTO t16 VALUES (1), (2);
INSERT INTO t17 VALUES (1), (2);

CREATE TRIGGER t1_ad AFTER DELETE ON t1
    FOR EACH ROW INSERT INTO logtable(tbl_name,operation)
    VALUES ('t1','DELETE');
CREATE TRIGGER t2_ad AFTER DELETE ON t2
    FOR EACH ROW INSERT INTO logtable(tbl_name,operation)
    VALUES ('t2','DELETE');
CREATE TRIGGER t5_ad AFTER DELETE ON t5
    FOR EACH ROW INSERT INTO logtable(tbl_name,operation)
    VALUES ('t5','DELETE');
CREATE TRIGGER t10_ad AFTER DELETE ON t10
    FOR EACH ROW INSERT INTO logtable(tbl_name,operation)
    VALUES ('t10','DELETE');
CREATE TRIGGER t15_ad AFTER DELETE ON t15
    FOR EACH ROW INSERT INTO logtable(tbl_name,operation)
    VALUES ('t15','DELETE');

--error ER_FK_DEPTH_EXCEEDED
DELETE FROM t1 WHERE f1=1;

--echo #All changes in trigger should be reverted back. No rows
SELECT * FROM logtable ORDER BY id;

--echo #reduce depth by deleting records in t16 and t17(cascade)
DELETE FROM t16;

--echo #try again, it should work now
DELETE FROM t1 WHERE f1=1;
SELECT * FROM t1 ORDER BY f1;
SELECT * FROM logtable ORDER BY id;

DELETE FROM logtable;

--echo # FR 8.5: If number of tables that can participate collectively in all
--echo # cascade chains exceeds max limit (i.e. 30) during a single statement
--echo # execution, then an error must be reported.
--echo # FR 8.6:  Multi-level, trigger-initiated cascades must function
--echo # correctly.  When a child table trigger fires during a FK cascade,
--echo # it may initiate further FK cascades to other related tables. It must
--echo # ensure referential integrity and proper execution order for all
--echo # triggered and cascading actions.
#initiate another cascade for another table
CREATE TABLE t111 (f1 INT PRIMARY KEY);
CREATE TABLE t112 (f1 INT UNIQUE REFERENCES t111(f1) ON DELETE CASCADE);
CREATE TABLE t113 (f1 INT UNIQUE REFERENCES t112(f1) ON DELETE CASCADE);
CREATE TABLE t114 (f1 INT UNIQUE REFERENCES t113(f1) ON DELETE CASCADE);
CREATE TABLE t115 (f1 INT UNIQUE REFERENCES t114(f1) ON DELETE CASCADE);
CREATE TABLE t116 (f1 INT UNIQUE REFERENCES t115(f1) ON DELETE CASCADE);
CREATE TABLE t117 (f1 INT UNIQUE REFERENCES t116(f1) ON DELETE CASCADE);
CREATE TABLE t118 (f1 INT UNIQUE REFERENCES t117(f1) ON DELETE CASCADE);
CREATE TABLE t119 (f1 INT UNIQUE REFERENCES t118(f1) ON DELETE CASCADE);
CREATE TABLE t120 (f1 INT UNIQUE REFERENCES t119(f1) ON DELETE CASCADE);
CREATE TABLE t121 (f1 INT UNIQUE REFERENCES t120(f1) ON DELETE CASCADE);
CREATE TABLE t122 (f1 INT UNIQUE REFERENCES t121(f1) ON DELETE CASCADE);
CREATE TABLE t123 (f1 INT UNIQUE REFERENCES t122(f1) ON DELETE CASCADE);
CREATE TABLE t124 (f1 INT UNIQUE REFERENCES t123(f1) ON DELETE CASCADE);
CREATE TABLE t125 (f1 INT UNIQUE REFERENCES t124(f1) ON DELETE CASCADE);
INSERT INTO t111 VALUES (1), (2);
INSERT INTO t112 VALUES (1), (2);
INSERT INTO t113 VALUES (1), (2);
INSERT INTO t114 VALUES (1), (2);
INSERT INTO t115 VALUES (1), (2);
INSERT INTO t116 VALUES (1), (2);
INSERT INTO t117 VALUES (1), (2);
INSERT INTO t118 VALUES (1), (2);
INSERT INTO t119 VALUES (1), (2);
INSERT INTO t120 VALUES (1), (2);
INSERT INTO t121 VALUES (1), (2);
INSERT INTO t122 VALUES (1), (2);
INSERT INTO t123 VALUES (1), (2);
INSERT INTO t124 VALUES (1), (2);
INSERT INTO t125 VALUES (1), (2);
DROP TRIGGER t15_ad;
CREATE TRIGGER t15_ad_cascade AFTER DELETE ON t15
    FOR EACH ROW DELETE FROM t111 WHERE f1 = OLD.f1;

CREATE TRIGGER t125_ad AFTER DELETE ON t125
    FOR EACH ROW DELETE FROM t211 WHERE f1 = OLD.f1;

CREATE TABLE t211 (f1 INT PRIMARY KEY);
CREATE TABLE t212 (f1 INT UNIQUE REFERENCES t211(f1) ON DELETE CASCADE);
CREATE TABLE t213 (f1 INT UNIQUE REFERENCES t212(f1) ON DELETE CASCADE);
INSERT INTO t211 VALUES (1), (2);
INSERT INTO t212 VALUES (1), (2);
INSERT INTO t213 VALUES (1), (2);
--echo #try again, it should not work now
--error ER_FK_MAX_TABLES_IN_CASCADE_CHAIN_EXCEEDED
DELETE FROM t1 WHERE f1=2;
SELECT * FROM t1 ORDER BY f1;
SELECT * FROM logtable ORDER BY id;
DROP TABLE logtable;
DROP TABLE t17, t16, t15, t14, t13, t12, t11, t10, t9,  t8,  t7,  t6,  t5,  t4,  t3,  t2, t1;
DROP TABLE t125, t124, t123, t122, t121, t120, t119,  t118,  t117,  t116,  t115,  t114,  t113,  t112, t111;
DROP TABLE t213,  t212, t211;

--echo # FR 8.6.1: Child trigger definition containing another table with CASCADE
--echo # should succeed
CREATE TABLE logtable (id INT PRIMARY KEY AUTO_INCREMENT, action VARCHAR(128), val INT);
CREATE TABLE parent(f1 INT PRIMARY KEY);
CREATE TABLE parent1(f1 INT PRIMARY KEY);
CREATE TABLE child(f1 INT PRIMARY KEY, f2 INT,
  FOREIGN KEY (f2) REFERENCES parent(f1) ON DELETE CASCADE);
CREATE TABLE child1(f1 INT PRIMARY KEY, f2 INT,
  FOREIGN KEY (f2) REFERENCES parent1(f1) ON DELETE CASCADE);
INSERT INTO parent VALUES (10), (20), (30);
INSERT INTO parent1 VALUES (10), (20), (30);
INSERT INTO child VALUES (10, 10), (20, 20), (30, 30);
INSERT INTO child1 VALUES (10, 10), (20, 20), (30, 30);

delimiter //;
CREATE TRIGGER trg1 AFTER DELETE ON parent
FOR EACH ROW BEGIN
    INSERT INTO logtable(action, val) VALUES("parent-AFTER-DELETE", OLD.f1);
END//

CREATE TRIGGER trg2 AFTER DELETE ON parent1
FOR EACH ROW BEGIN
    INSERT INTO logtable(action, val) VALUES("parent1-AFTER-DELETE", OLD.f1);
END//

#initiates casade on parent1 from child trigger
CREATE TRIGGER trg3 AFTER DELETE ON child
FOR EACH ROW BEGIN
    INSERT INTO logtable(action, val) VALUES("child-AFTER-DELETE", OLD.f2);
    DELETE FROM parent1 where f1=OLD.f2;
END//

CREATE TRIGGER trg4 AFTER DELETE ON child1
FOR EACH ROW BEGIN
    INSERT INTO logtable(action, val) VALUES("child1-AFTER-DELETE", OLD.f2);
END//
delimiter ;//

--echo invoking delete trigger with table containing fk cascade
DELETE FROM parent where f1 = 10;

SELECT * FROM parent;
SELECT * FROM child;
--echo #All triggers should be fired in the correct order
SELECT * FROM logtable order by id;
DROP TABLE logtable, parent, child, parent1, child1;

--echo # FR 11: SQL Statement Interactions
--echo # FR 11.2: Trigger firing for REPLACE ON FK column
--echo # REPLACE on the parent that leads to a DELETE followed by
--echo # INSERT statement must fire DELETE triggers on affected parent rows
--echo # and cascade DELETE triggers on children
CREATE TABLE logtable (action VARCHAR(128), oldval INT, newval INT);
CREATE TABLE parent(f1 INT PRIMARY KEY, f2 INT, UNIQUE(f2));
CREATE TABLE child(f1 INT PRIMARY KEY, f2 INT, FOREIGN KEY (f2) REFERENCES parent(f2) ON UPDATE CASCADE ON DELETE CASCADE);
delimiter //;
CREATE TRIGGER trg1 BEFORE INSERT ON parent
FOR EACH ROW BEGIN
    INSERT INTO logtable VALUES("parent-BEFORE-INSERT", 0, NEW.f2);
END//

CREATE TRIGGER trg2 BEFORE UPDATE ON parent
FOR EACH ROW BEGIN
    INSERT INTO logtable VALUES("parent-BEFORE-UPDATE", OLD.f2, NEW.f2);
END//

CREATE TRIGGER trg3 BEFORE DELETE ON parent
FOR EACH ROW BEGIN
    INSERT INTO logtable VALUES("parent-BEFORE-DELETE", OLD.f2, 0);
END//

CREATE TRIGGER trg4 BEFORE INSERT ON child
FOR EACH ROW BEGIN
    INSERT INTO logtable VALUES("child-BEFORE-INSERT", 0, NEW.f2);
END//

CREATE TRIGGER trg5 BEFORE UPDATE ON child
FOR EACH ROW BEGIN
    INSERT INTO logtable VALUES("child-BEFORE-UPDATE", OLD.f2, NEW.f2);
END//

CREATE TRIGGER trg6 BEFORE DELETE ON child
FOR EACH ROW BEGIN
    INSERT INTO logtable VALUES("child-BEFORE-DELETE", OLD.f2, 0);
END//
delimiter ;//

REPLACE INTO parent VALUES (1, 1), (2,2);
REPLACE INTO child VALUES (1, 1), (2,2);
REPLACE INTO parent VALUES (1, 5);
SELECT * FROM logtable;
SELECT * FROM parent order by f1;
SELECT * FROM child;
DELETE FROM logtable;

--echo # FR 11.1: INSERT...ON DUPLICATE KEY UPDATE on the parent that leads to
--echo # UPDATE CASCADE must fire UPDATE triggers on affected child rows.
INSERT INTO parent VALUES (2, 20) ON DUPLICATE KEY UPDATE f2=50;
SELECT * FROM logtable;
SELECT * FROM parent order by f1;
SELECT * FROM child;
DROP TABLE parent, child, logtable;

--echo # FR 11.3: Foreign key checks on tables used by trigger bodies must follow
--echo # standard foreign_key_checks semantics.
--echo # CASCADE should not work and fire triggers with foreign_key_checks=off
--echo # TODO:include FK check in trigger
CREATE TABLE parent(id INT PRIMARY KEY);
CREATE TABLE child(id INT PRIMARY KEY REFERENCES parent(id) ON DELETE CASCADE);
CREATE TABLE logtable (action VARCHAR(128), oldval INT, newval INT);
INSERT INTO parent VALUES (100);
INSERT INTO child VALUES (100);
delimiter //;
CREATE TRIGGER trg1 AFTER DELETE ON parent
FOR EACH ROW BEGIN
    INSERT INTO logtable VALUES("parent-AFTER-DELETE", OLD.id, 0);
END//

CREATE TRIGGER trg2 AFTER DELETE ON child
FOR EACH ROW BEGIN
    INSERT INTO logtable VALUES("child-AFTER-DELETE", OLD.id, 0);
END//

delimiter ;//

SET FOREIGN_KEY_CHECKS=0;
DELETE FROM parent WHERE id=100;
SET FOREIGN_KEY_CHECKS=1;

SELECT * FROM parent;
SELECT * FROM child;
SELECT * FROM logtable;
DROP TABLE parent, child, logtable;

--echo # FR 11.4: LOCK TABLE must work with foreign key cascade trigger firing.
--echo # Triggers should fire with LOCK TABLE
CREATE TABLE parent(id INT PRIMARY KEY);
CREATE TABLE child(id INT PRIMARY KEY REFERENCES parent(id) ON DELETE CASCADE);
CREATE TABLE logtable (action VARCHAR(128), oldval INT, newval INT);
INSERT INTO parent VALUES (100);
INSERT INTO child VALUES (100);

delimiter //;
CREATE TRIGGER trg1 AFTER DELETE ON parent
FOR EACH ROW BEGIN
    INSERT INTO logtable VALUES("parent-AFTER-DELETE", OLD.id, 0);
END//

CREATE TRIGGER trg2 AFTER DELETE ON child
FOR EACH ROW BEGIN
    INSERT INTO logtable VALUES("child-AFTER-DELETE", OLD.id, 0);
END//

delimiter ;//

LOCK TABLES parent WRITE;
DELETE FROM parent WHERE id=100;
UNLOCK TABLES;

SELECT * FROM parent;
SELECT * FROM child;
SELECT * FROM logtable;
DROP TABLE parent, child, logtable;

--echo # FR 11.5: Child table triggers calling FUNCTION referring to parent table
--echo # must return error ER_CANT_UPDATE_USED_TABLE_IN_SF_OR_TRG.
CREATE TABLE parent (id INT PRIMARY KEY AUTO_INCREMENT, name VARCHAR(20));
CREATE TABLE child (id INT PRIMARY KEY AUTO_INCREMENT, parent_id INT,
data VARCHAR(20), FOREIGN KEY (parent_id) REFERENCES parent(id) ON DELETE CASCADE);

DELIMITER |;
CREATE FUNCTION get_parent_count() RETURNS INT DETERMINISTIC READS SQL DATA
BEGIN
  DECLARE cnt INT;
  SELECT COUNT(*) INTO cnt FROM parent;
  RETURN cnt;
END|

CREATE FUNCTION insert_parent() RETURNS INT DETERMINISTIC MODIFIES SQL DATA
BEGIN
  INSERT INTO parent(name) VALUES('IN FUNC');
  RETURN 0;
END|

CREATE TRIGGER child_after_delete AFTER DELETE ON child
FOR EACH ROW
BEGIN
  SET @parent_count = get_parent_count();
  DO insert_parent();
END|
DELIMITER ;|

INSERT INTO parent(name) VALUES ('A'), ('B');
INSERT INTO child(parent_id, data) VALUES (1, 'foo');
SELECT get_parent_count();
--error ER_CANT_UPDATE_USED_TABLE_IN_SF_OR_TRG
DELETE FROM parent where id = 1;
SELECT @parent_count;
SELECT * FROM parent;
DROP TRIGGER child_after_delete;
DROP FUNCTION get_parent_count;
DROP FUNCTION insert_parent;
DROP TABLE child;
DROP TABLE parent;

--echo #FK UPDATE SET NULL along with child table BEFORE UPDATE trigger containing SET
CREATE TABLE author (author_id INT, first_name VARCHAR(50) NOT NULL,
last_name VARCHAR(50) NOT NULL, PRIMARY KEY(author_id)) ;
INSERT INTO author(author_id, first_name, last_name) VALUES('1', 'Jules', 'Verne');
INSERT INTO author(author_id, first_name, last_name) VALUES('3', 'Sidney', 'Sheldon');

CREATE TABLE book (book_id INT PRIMARY KEY AUTO_INCREMENT, title VARCHAR(200) NOT NULL, pub_year INT,
author_id INT, FOREIGN KEY (author_id) REFERENCES author(author_id) ON DELETE SET NULL  ON UPDATE SET NULL);
INSERT INTO book VALUES('1', 'Master of the Game', 1982, 3);

CREATE TRIGGER trig1 BEFORE UPDATE ON book FOR EACH ROW SET NEW.author_id = 4;
--error ER_FK_CASCADE_TRIGGER_UPDATING_FK_COLUMNS_NOT_SUPPORTED
UPDATE author SET author_id=2 WHERE author_id=3;
SELECT * FROM author;
SELECT * FROM book;
DROP TRIGGER trig1;
--echo #updating non foreign key column using SET should succeed
CREATE TRIGGER trig1 BEFORE UPDATE ON book FOR EACH ROW SET NEW.pub_year = 1970;
UPDATE author SET author_id=2 WHERE author_id=3;
SELECT * FROM author;
SELECT * FROM book;
DROP TABLE book, author;

--echo #FK UPDATE CASCADE along with child table BEFORE UPDATE trigger containing SET
CREATE TABLE author (author_id INT, first_name VARCHAR(50) NOT NULL,
last_name VARCHAR(50) NOT NULL, PRIMARY KEY(author_id)) ;
INSERT INTO author(author_id, first_name, last_name) VALUES('1', 'Jules', 'Verne');
INSERT INTO author(author_id, first_name, last_name) VALUES('3', 'Sidney', 'Sheldon');

CREATE TABLE book (book_id INT PRIMARY KEY AUTO_INCREMENT, title VARCHAR(200) NOT NULL, pub_year INT,
author_id INT, FOREIGN KEY (author_id) REFERENCES author(author_id) ON UPDATE CASCADE);
INSERT INTO book VALUES('1', 'Master of the Game', 1982, 3);

CREATE TRIGGER trig1 BEFORE UPDATE ON book FOR EACH ROW SET NEW.author_id = 4;
--error ER_FK_CASCADE_TRIGGER_UPDATING_FK_COLUMNS_NOT_SUPPORTED
UPDATE author SET author_id=2 WHERE author_id=3;
SELECT * FROM author;
SELECT * FROM book;
DROP TRIGGER trig1;
CREATE TRIGGER trig1 BEFORE UPDATE ON book FOR EACH ROW SET NEW.pub_year = 1970;
--echo #updating non foreign key column using SET should succeed
UPDATE author SET author_id=2 WHERE author_id=3;
SELECT * FROM author;
SELECT * FROM book;
DROP TABLE book, author;

--echo #FK CASCADE along with child table BEFORE UPDATE trigger containing UPDATE on same table
CREATE TABLE author (author_id INT, first_name VARCHAR(50) NOT NULL,
last_name VARCHAR(50) NOT NULL, PRIMARY KEY(author_id)) ;
INSERT INTO author(author_id, first_name, last_name) VALUES('1', 'Jules', 'Verne');
INSERT INTO author(author_id, first_name, last_name) VALUES('3', 'Sidney', 'Sheldon');

CREATE TABLE book (book_id INT PRIMARY KEY AUTO_INCREMENT, title VARCHAR(200) NOT NULL, pub_year INT,
author_id INT, FOREIGN KEY (author_id) REFERENCES author(author_id) ON DELETE SET NULL);
INSERT INTO book VALUES('1', 'Master of the Game', 1982, 3);

CREATE TRIGGER trig1 BEFORE UPDATE ON book FOR EACH ROW UPDATE book SET title = 'No Author' WHERE author_id = OLD.author_id;
#DELETE FROM author WHERE author_id=3;
SELECT * FROM author;
SELECT * FROM book;
DROP TABLE book, author;

--echo # FK parent check: BEFORE trigger on child sets fk column to non existing value => should fail
CREATE TABLE parent (id INT PRIMARY KEY);
CREATE TABLE pparent (id INT PRIMARY KEY);
CREATE TABLE child (id INT PRIMARY KEY, parent_id INT, pparent_id INT,
CONSTRAINT fk_1 FOREIGN KEY (parent_id) REFERENCES parent(id) ON UPDATE CASCADE,
CONSTRAINT fk_2 FOREIGN KEY (pparent_id) REFERENCES pparent(id) ON UPDATE CASCADE);

DELIMITER |;
CREATE TRIGGER bt_child BEFORE UPDATE ON child
FOR EACH ROW
BEGIN
    -- If parent_id changes, modify to non exising value
    IF NEW.parent_id != OLD.parent_id THEN
        SET NEW.pparent_id = 10;
    END IF;
END|
DELIMITER ;|

INSERT INTO parent VALUES (1);
INSERT INTO pparent VALUES (1);
INSERT INTO child VALUES (100, 1, 1);
--error ER_NO_REFERENCED_ROW_2
UPDATE parent SET id = 5 WHERE id = 1;

DROP TABLE parent, pparent, child;

--echo # FK child check: BEFORE trigger on child changes fk column with existing child value => should fail
CREATE TABLE parent (id INT PRIMARY KEY);
CREATE TABLE child (id INT PRIMARY KEY, parent_id INT,
CONSTRAINT fk_1 FOREIGN KEY (parent_id) REFERENCES parent(id) ON UPDATE CASCADE);
CREATE TABLE gchild (id INT PRIMARY KEY, pparent_id INT,
CONSTRAINT fk_2 FOREIGN KEY (pparent_id) REFERENCES child(id));

DELIMITER |;
CREATE TRIGGER bt_child BEFORE UPDATE ON child
FOR EACH ROW
BEGIN
    -- If parent_id changes, modify existing value
    IF NEW.parent_id != OLD.parent_id THEN
        SET NEW.id = 20;
    END IF;
END|
DELIMITER ;|

INSERT INTO parent VALUES (1);
INSERT INTO child VALUES (10, 1);
INSERT INTO gchild VALUES (100, 10);
--error ER_ROW_IS_REFERENCED_2
UPDATE parent SET id = 5 WHERE id = 1;

DROP TABLE parent, child, gchild;

--echo # FK child check: BEFORE trigger on child changes field using CURRENT_TIMESTAMP
CREATE TABLE parent (id INT PRIMARY KEY);
CREATE TABLE child (id INT PRIMARY KEY, parent_id INT, dt DATE,
CONSTRAINT fk_1 FOREIGN KEY (parent_id) REFERENCES parent(id) ON UPDATE CASCADE);

DELIMITER |;
CREATE TRIGGER bt_child BEFORE UPDATE ON child
FOR EACH ROW
BEGIN
    -- If parent_id changes, modify existing value
    IF NEW.parent_id != OLD.parent_id THEN
        SET NEW.dt = CURRENT_TIMESTAMP;
    END IF;
END|
DELIMITER ;|

INSERT INTO parent VALUES (1);
INSERT INTO child VALUES (10, 1, '2000-01-01');

UPDATE parent SET id = 5 WHERE id = 1;
SELECT (dt <> DATE '2000-01-01') AS dt_changed FROM child;

DROP TABLE parent, child;

--echo # Generated column check in cascade trigger path
CREATE TABLE gen_parent (id INT PRIMARY KEY);
CREATE TABLE gen_child (id INT PRIMARY KEY, pid INT, x INT,
  gcol INT GENERATED ALWAYS AS (x + 1) STORED NOT NULL,
  CONSTRAINT fk_g1 FOREIGN KEY (pid) REFERENCES gen_parent(id) ON UPDATE CASCADE);

INSERT INTO gen_parent VALUES (1);
INSERT INTO gen_child (id, pid, x) VALUES (100, 1, 10);

DELIMITER |;
CREATE TRIGGER gen_gcol BEFORE UPDATE ON gen_child
FOR EACH ROW
BEGIN
  -- On parent id update (cascade), set x = NULL (gcol will recompute as NULL and fail NOT NULL)
  IF NEW.pid != OLD.pid THEN SET NEW.x = NULL; END IF;
END|
DELIMITER ;|

--error ER_BAD_NULL_ERROR
UPDATE gen_parent SET id=2 WHERE id=1;

DROP TRIGGER gen_gcol;
--echo # Set gcol to a valid value in the trigger and allow cascade
DELIMITER |;
CREATE TRIGGER gen_gcol BEFORE UPDATE ON gen_child
FOR EACH ROW
BEGIN
  IF NEW.pid != OLD.pid THEN SET NEW.x = 100; END IF;
END|
DELIMITER ;|
--echo must succeed as generated column is recomputed correctly
UPDATE gen_parent SET id=3 WHERE id=1;
SELECT * FROM gen_child;
DROP TABLE gen_parent, gen_child;

--echo # Null check: BEFORE trigger on child sets non-nullable column to NULL => should fail
CREATE TABLE parent (id INT PRIMARY KEY, val INT);
CREATE TABLE child (id INT PRIMARY KEY, parent_id INT, notnull_col INT NOT NULL,
    CONSTRAINT fk FOREIGN KEY (parent_id) REFERENCES parent(id) ON UPDATE CASCADE);

DELIMITER |;
CREATE TRIGGER bt_child BEFORE UPDATE ON child
FOR EACH ROW
BEGIN
    -- If parent_id changes, nullify notnull_col
    IF NEW.parent_id != OLD.parent_id THEN
        SET NEW.notnull_col = NULL;
    END IF;
END|
DELIMITER ;|

INSERT INTO parent VALUES (1, 10);
INSERT INTO child VALUES (100, 1, 123);

--error ER_BAD_NULL_ERROR
UPDATE parent SET id=2 WHERE id=1;

--echo # Null check: direct DML on child with trigger should still enforce NOT NULL
--error ER_BAD_NULL_ERROR
UPDATE child SET notnull_col = NULL WHERE id=100;

--echo # Check constraint: child trigger sets column to violate CHECK constraint, cascade must fail
ALTER TABLE child MODIFY notnull_col INT NOT NULL CHECK (notnull_col >= 100);
INSERT INTO parent VALUES (3, 20);
INSERT INTO child VALUES (200, 3, 2000);

DELIMITER |;
CREATE TRIGGER bt_child2 BEFORE UPDATE ON child
FOR EACH ROW
BEGIN
    IF NEW.parent_id != OLD.parent_id THEN
        SET NEW.notnull_col = 5;
    END IF;
END|
DELIMITER ;|

--error ER_CHECK_CONSTRAINT_VIOLATED
UPDATE parent SET id=4 WHERE id=3;

--echo # Check constraint: direct DML on child with trigger sets bad value
--error ER_CHECK_CONSTRAINT_VIOLATED
UPDATE child SET notnull_col = 5 WHERE id = 200;

--echo # CHECK constraint satisfied by trigger: direct update (should succeed)
UPDATE child SET notnull_col = 1500 WHERE id=200;
UPDATE parent SET id=5 WHERE id=4;

DROP TABLE parent, child;

--echo # Bug#11745182:Triggers not executed following foreign key updates/deletes
CREATE TABLE t1 (id INT NOT NULL, col1 char(50), PRIMARY KEY (id));
CREATE TABLE t2 (id INT PRIMARY KEY, f_id INT, INDEX par_ind (f_id), col1 char(50),
FOREIGN KEY (f_id) REFERENCES t1(id) ON DELETE SET NULL);
CREATE TRIGGER tr_t2 AFTER UPDATE ON t2 FOR EACH ROW SET @counter=@counter+1;
INSERT INTO t1 values (1,'Department A');
INSERT INTO t1 VALUES (2,'Department B');
INSERT INTO t1 VALUES (3,'Department C');
INSERT INTO t2 VALUES (1,2,'Emp 1');
INSERT INTO t2 VALUES (2,2,'Emp 2');
INSERT INTO t2 VALUES (3,2,'Emp 3');
INSERT INTO t2 VALUES (4,2,'Emp 4');
INSERT INTO t2 VALUES (5,2,'Emp 5');
SET @counter=0;
SELECT * FROM t1;
SELECT * FROM t2;
SELECT @counter;

DELETE FROM t1 where id=2;
SELECT * FROM t1;
SELECT * FROM t2;
SELECT @counter;

DROP TABLE t1, t2;

--echo # Self Referencing FK CASCADE with ON DELETE SET NULL triggering UPDATE trigger
--echo # on same table must fail
CREATE TABLE employees (employee_id INT UNIQUE KEY, employee_name VARCHAR(100),
manager_id INT, FOREIGN KEY (manager_id) REFERENCES employees(employee_id) ON DELETE SET NULL);

INSERT INTO employees values(1, 'Emp1', NULL);
INSERT INTO employees values(2, 'Emp2', 1);
INSERT INTO employees values(3, 'Emp3', 1);

CREATE TRIGGER trig2 BEFORE UPDATE ON employees FOR EACH ROW UPDATE employees SET employee_name=NULL;

--error ER_CANT_UPDATE_USED_TABLE_IN_SF_OR_TRG
DELETE FROM employees WHERE employee_id=1;

DROP TABLE employees;

--echo # Cascade initiated by triggers should not update same table twice
--echo # during statement execution and must fail
CREATE TABLE employees (employee_id INT PRIMARY KEY, name VARCHAR(100));
INSERT INTO employees VALUES(1, 'Deepa');
INSERT INTO employees VALUES(2, 'Kushal');

CREATE TABLE departments (department_id INT PRIMARY KEY, dept_name VARCHAR(100));
INSERT INTO departments VALUES(1, 'Science');
INSERT INTO departments VALUES(2, 'English');

CREATE TABLE projects (project_id INT PRIMARY KEY, project_name VARCHAR(100));
INSERT INTO projects VALUES(1, 'Project1');
INSERT INTO projects VALUES(2, 'Project2');

CREATE TABLE assignments (
    assignment_id INT PRIMARY KEY, employee_id INT, department_id INT,
    project_id INT, assignment_date DATE,
    FOREIGN KEY (employee_id) REFERENCES employees(employee_id) ON UPDATE CASCADE ON DELETE SET NULL,
    FOREIGN KEY (department_id) REFERENCES departments(department_id) ON UPDATE SET DEFAULT ON DELETE CASCADE,
    FOREIGN KEY (project_id) REFERENCES projects(project_id) ON UPDATE SET NULL ON DELETE SET DEFAULT);
INSERT INTO assignments VALUES(1, 1, 1, 1, '2025-01-01');
INSERT INTO assignments VALUES(2, 2, 2, 2, '2025-02-02');

CREATE TABLE triggered(v VARCHAR(25));
CREATE TRIGGER trig1 BEFORE DELETE on assignments FOR EACH ROW INSERT INTO triggered VALUES ('BEFORE-DELETE');
CREATE TRIGGER trig2 BEFORE UPDATE on assignments FOR EACH ROW DELETE FROM departments WHERE department_id=1;

--error ER_CANT_UPDATE_USED_TABLE_IN_FK_CASCADE
UPDATE employees SET employee_id=3 WHERE employee_id=1;
DROP TABLE employees, assignments, projects, departments, triggered;
SET ENABLE_CASCADE_TRIGGERS = DEFAULT;
