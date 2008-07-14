#################################################################
# This file inclde tests that address the foreign key cases of
# the following requirements since they are specific to innodb.
# Other test cases for these requirements are included in the
# triggers_master.test file.
#################################################################

--disable_abort_on_error

# OBN - The following tests are disabled until triggers are supported with forign
#       keys in innodb (foreign keys tests dispabled - bug 11472)
#################################################################################
#Section x.x.x.3
# Test case: Similar to 3.5.10.5 but with ten tables to see if multiple triggers
#            can be executed at once
let $message= Testcase x.x.x.3:;
--source include/show_msg.inc

	--disable_warnings
	DROP TABLE IF EXISTS t1, t2;
	--enable_warnings

	eval CREATE TABLE t0 (col1 char(50)) ENGINE=$engine_type;
	eval CREATE TABLE t1 (id INT NOT NULL, col1 char(50),
		PRIMARY KEY (id)) ENGINE=$engine_type;
	eval CREATE TABLE t2 (id INT PRIMARY KEY, f_id INT, INDEX par_ind
		(f_id), col1 char(50), FOREIGN KEY (f_id) REFERENCES t1(id)
		ON DELETE SET NULL) ENGINE=$engine_type;
	eval CREATE TABLE t3 (id INT PRIMARY KEY, f_id INT, INDEX par_ind
		(f_id), col1 char(50), FOREIGN KEY (f_id) REFERENCES t1(id)
		ON DELETE SET NULL ON UPDATE CASCADE) ENGINE=$engine_type;
	eval CREATE TABLE t4 (id INT PRIMARY KEY, f_id INT, INDEX par_ind
		(f_id), col1 char(50), FOREIGN KEY (f_id) REFERENCES t1(id)
		ON DELETE SET NULL ON UPDATE CASCADE) ENGINE=$engine_type;
	eval CREATE TABLE t5 (id INT PRIMARY KEY, f_id INT, INDEX par_ind
		(f_id), col1 char(50), FOREIGN KEY (f_id) REFERENCES t1(id)
		ON DELETE SET NULL ON UPDATE CASCADE) ENGINE=$engine_type;
	eval CREATE TABLE t6 (id INT PRIMARY KEY, f_id INT, INDEX par_ind
		(f_id), col1 char(50), FOREIGN KEY (f_id) REFERENCES t1(id)
		ON DELETE SET NULL ON UPDATE CASCADE) ENGINE=$engine_type;
	eval CREATE TABLE t7 (id INT PRIMARY KEY, f_id INT, INDEX par_ind
		(f_id), col1 char(50), FOREIGN KEY (f_id) REFERENCES t1(id)
		ON DELETE SET NULL ON UPDATE CASCADE) ENGINE=$engine_type;
	eval CREATE TABLE t8 (id INT PRIMARY KEY, f_id INT, INDEX par_ind
		(f_id), col1 char(50), FOREIGN KEY (f_id) REFERENCES t1(id)
		ON DELETE SET NULL ON UPDATE CASCADE) ENGINE=$engine_type;
	eval CREATE TABLE t9 (id INT PRIMARY KEY, f_id INT, INDEX par_ind
		(f_id), col1 char(50), FOREIGN KEY (f_id) REFERENCES t1(id)
		ON DELETE SET NULL ON UPDATE CASCADE) ENGINE=$engine_type;
	eval CREATE TABLE t10(id INT PRIMARY KEY, f_id INT, INDEX par_ind
		(f_id), col1 char(50), FOREIGN KEY (f_id) REFERENCES t1(id)
		ON DELETE SET NULL ON UPDATE CASCADE) ENGINE=$engine_type;
	eval CREATE TABLE t11(id INT PRIMARY KEY, f_id INT, INDEX par_ind
		(f_id), col1 char(50), FOREIGN KEY (f_id) REFERENCES t1(id)
		ON DELETE SET NULL ON UPDATE CASCADE) ENGINE=$engine_type;

	create trigger tr1 after update on t2 for each row
		insert into t0 values ('tr_t2');
	create trigger tr2 after update on t3 for each row
		insert into t0 values ('tr_t3');
	create trigger tr3 after update on t4 for each row
		insert into t0 values ('tr_t4');
	create trigger tr3 after update on t5 for each row
		insert into t0 values ('tr_t5');
	create trigger tr4 after update on t6 for each row
		insert into t0 values ('tr_t6');
	create trigger tr5 after update on t7 for each row
		insert into t0 values ('tr_t7');
	create trigger tr5 after update on t8 for each row
		insert into t0 values ('tr_t8');
	create trigger tr6 after update on t9 for each row
		insert into t0 values ('tr_t9');
	create trigger tr7 after update on t10 for each row
		insert into t0 values ('tr_t10');
	create trigger tr8 after update on t11 for each row
		insert into t0 values ('tr_t11');

	insert into t1 values (1,'Department A');
	insert into t1 values (2,'Department B');
	insert into t1 values (3,'Department C');

	insert into t2 values (1,2,'Employee');
	insert into t3 values (1,2,'Employee');
	insert into t4 values (1,2,'Employee');
	insert into t5 values (1,2,'Employee');
	insert into t6 values (1,2,'Employee');
	insert into t7 values (1,2,'Employee');
	insert into t8 values (1,2,'Employee');
	insert into t9 values (1,2,'Employee');
	insert into t10 values (1,2,'Employee');
	insert into t11 values (1,2,'Employee');

	select * from t1;
	select * from t2;
	select * from t3;
	select * from t4;
	select * from t5;
	select * from t6;
	select * from t7;
	select * from t8;
	select * from t9;
	select * from t10;
	select * from t11;

	delete from t1 where id=2;
	select * from t1;
	select * from t2;
	select * from t3;
	select * from t4;
	select * from t5;
	select * from t6;
	select * from t7;
	select * from t8;
	select * from t9;
	select * from t10;
	select * from t11;

	select * from t0;

# Cleanup
	drop trigger tr1;
	drop trigger tr2;
	drop trigger tr3;
	drop trigger tr4;
	drop trigger tr5;
	drop trigger tr6;
	drop trigger tr7;
	drop trigger tr8;
	drop trigger tr9;
	drop trigger tr10;
	drop table t2,t3,t4,t5,t6,t7,t8,t9,t10,t11,t1,t0;




#Section 3.5.10.5
# Test case: Ensure that every trigger that should be activated by every possible
#            type of implicit update of its subject table (e.g. a FOREIGN KEY SET
#            DEFAULT action or an UPDATE of a view based on the subject table)
#            is indeed activated correctly.
let $message= Testcase 3.5.10.5 (foreign keys):;
--source include/show_msg.inc


	--disable_warnings
	DROP TABLE IF EXISTS t1, t2;
	--enable_warnings

	eval CREATE TABLE t1 (id INT NOT NULL, col1 char(50),
		PRIMARY KEY (id)) ENGINE=$engine_type;
	eval CREATE TABLE t2 (id INT PRIMARY KEY, f_id INT,
		INDEX par_ind (f_id), col1 char(50),
		FOREIGN KEY (f_id) REFERENCES t1(id)
		ON DELETE SET NULL ON UPDATE CASCADE) ENGINE=$engine_type;
	create trigger tr_t2 after update on t2
		for each row set @counter=@counter+1;

	insert into t1 values (1,'Department A');
	insert into t1 values (2,'Department B');
	insert into t1 values (3,'Department C');
	insert into t2 values (1,2,'Emp 1');
	insert into t2 values (2,2,'Emp 2');
	insert into t2 values (3,2,'Emp 3');
	insert into t2 values (4,2,'Emp 4');
	insert into t2 values (5,2,'Emp 5');
	insert into t2 values (6,3,'Emp 6');
	set @counter=0;

	select * from t1;
	select * from t2;
	select @counter;

	update t1 set id=4 where id=3;
	select * from t1;
	select * from t2;
	select @counter;

	delete from t1 where id=2;
	select * from t1;
	select * from t2;
	select @counter;

# This is to verify that the trigger works when updated directly
	update t2 set col1='Emp 5a' where id=5;
	select * from t2;
	select @counter;

# Cleanup
	drop trigger tr_t2;
	drop table t2, t1;


#Section 3.5.10.6
# Test case: Ensure that every trigger that should be activated by every possible
#            type of implicit deletion from its subject table (e.g. a FOREIGN KEY
#            CASCADE action or a DELETE from a view based on the subject table)
#            is indeed activated correctly.
let $message= Testcase 3.5.10.6 (foreign keys):;
--source include/show_msg.inc

	--disable_warnings
	DROP TABLE IF EXISTS t1, t2;
	--enable_warnings
	
	eval CREATE TABLE t1 (id INT NOT NULL, col1 char(50),
		PRIMARY KEY (id)) ENGINE=$engine_type;
	eval CREATE TABLE t2 (id INT PRIMARY KEY, f_id INT,
		INDEX par_ind (f_id), col1 char(50),
		FOREIGN KEY (f_id) REFERENCES t1(id)
		ON DELETE CASCADE) ENGINE=$engine_type;

	create trigger tr_t2 before delete on t2
		for each row set @counter=@counter+1;

	insert into t1 values (1,'Department A');
	insert into t1 values (2,'Department B');
	insert into t1 values (3,'Department C');
	insert into t2 values (1,2,'Emp 1');
	insert into t2 values (2,2,'Emp 2');
	insert into t2 values (3,2,'Emp 3');
	insert into t2 values (4,2,'Emp 4');
	insert into t2 values (5,2,'Emp 5');
	insert into t2 values (6,3,'Emp 6');
	set @counter=0;

	select * from t1;
	select * from t2;
	select @counter;

	delete from t1 where id=2;

	select * from t1;
	select * from t2;
	select @counter;

# This is to verify that the trigger works when deleted directly
	delete from t2 where id=6;
	select * from t2;
	select @counter;

# Cleanup
	drop trigger tr_t2;
	drop table t2, t1;

