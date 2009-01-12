#################################################################
# This file include tests that address the foreign key cases of
# the following requirements since they are specific to innodb.
# Other test cases for these requirements are included in the
# triggers_*.inc files.
#################################################################

--disable_abort_on_error

# Section x.x.x.1
# Test case: Verifing that a trigger that activates a primary key results in
#            the primary key acting correctly on the foreign key
let $message= Testcase x.x.x.1:;
--source include/show_msg.inc


--disable_warnings
DROP TABLE IF EXISTS t0, t1, t2;
--enable_warnings

--replace_result $engine_type <engine_to_be_tested>
eval
CREATE TABLE t0 (col1 CHAR(50))
ENGINE = $engine_type;
--replace_result $engine_type <engine_to_be_tested>
eval
CREATE TABLE t1 (id INT NOT NULL, col1 CHAR(50), PRIMARY KEY (id))
ENGINE = $engine_type;
--replace_result $engine_type <engine_to_be_tested>
eval
CREATE TABLE t2 (id INT PRIMARY KEY, f_id INT,
   INDEX par_ind (f_id), col1 CHAR(50),
   FOREIGN KEY (f_id) REFERENCES t1(id) ON DELETE SET NULL)
ENGINE = $engine_type;

INSERT INTO t1 VALUES (1,'Department A');
INSERT INTO t1 VALUES (2,'Department B');
INSERT INTO t1 VALUES (3,'Department C');
INSERT INTO t2 VALUES (1,2,'Emp 1');
INSERT INTO t2 VALUES (2,2,'Emp 2');
INSERT INTO t2 VALUES (3,2,'Emp 3');

CREATE TRIGGER trig AFTER INSERT ON t0 FOR EACH ROW
DELETE FROM t1 WHERE col1 = new.col1;

--sorted_result
SELECT * FROM t2;
LOCK TABLES t0 WRITE, t1 WRITE;
INSERT INTO t0 VALUES ('Department B');
UNLOCK TABLES;
--sorted_result
SELECT * FROM t2;

# Cleanup
DROP TRIGGER trig;
DROP TABLE t2, t1;


#Section x.x.x.2
# Test case: Checking that triggers can be used as a way to address missing foreign
#            key definition
let $message= Testcase x.x.x.2:;
--source include/show_msg.inc

--disable_warnings
DROP TABLE IF EXISTS t1, t2;
--enable_warnings

--replace_result $engine_type <engine_to_be_tested>
eval
CREATE TABLE t1 (id INT NOT NULL, col1 CHAR(50), PRIMARY KEY (id))
ENGINE = $engine_type;
--replace_result $engine_type <engine_to_be_tested>
eval
CREATE TABLE t2 (id INT PRIMARY KEY, f_id INT,
   INDEX par_ind (f_id), col1 CHAR(50),
   FOREIGN KEY (f_id) REFERENCES t1(id) ON UPDATE CASCADE)
ENGINE = $engine_type;

INSERT INTO t1 VALUES (1,'Department A');
INSERT INTO t1 VALUES (2,'Department B');
INSERT INTO t1 VALUES (3,'Department C');
INSERT INTO t2 VALUES (1,2,'Emp 1');
INSERT INTO t2 VALUES (2,3,'Emp 2');

--error ER_NO_REFERENCED_ROW_2
insert into t2 VALUES (3,4,'Emp 3');

CREATE TRIGGER tr_t2 BEFORE INSERT ON t2 FOR EACH ROW
INSERT INTO t1 VALUES(new.f_id, CONCAT('New Department ', new.f_id));

LOCK TABLES t1 WRITE, t2 WRITE;
INSERT INTO t2 VALUES (3,4,'Emp 3');
UNLOCK TABLES;

--sorted_result
SELECT * FROM t1;
--sorted_result
SELECT * FROM t2;

# Cleanup
DROP TRIGGER tr_t2;
DROP TABLE t2, t1, t0;


--echo
--echo Foreign Key tests disabled (bug 11472 - stored in trig_frkey2.test)
--echo -------------------------------------------------------------------
