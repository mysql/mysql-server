#################################################################
# This file inclde tests that address the foreign key cases of
# the following requirements since they are specific to innodb.
# Other test cases for these requirements are included in the
# triggers_master.test file.
#################################################################

--disable_abort_on_error

#Section x.x.x.1
# Test case: Verifing that a trigger that activates a primary key results in
#            the primary key acting correctly on the foreign key
let $message= Testcase x.x.x.1:;
--source include/show_msg.inc


	--disable_warnings
	DROP TABLE IF EXISTS t0, t1, t2;
	--enable_warnings

	eval CREATE TABLE t0 (col1 char(50)) ENGINE=$engine_type;
	eval CREATE TABLE t1 (id INT NOT NULL, col1 char(50),
		PRIMARY KEY (id)) ENGINE=$engine_type;
	eval CREATE TABLE t2 (id INT PRIMARY KEY, f_id INT,
		INDEX par_ind (f_id), col1 char(50),
		FOREIGN KEY (f_id) REFERENCES t1(id)
		ON DELETE SET NULL) ENGINE=$engine_type;

	insert into t1 values (1,'Department A');
	insert into t1 values (2,'Department B');
	insert into t1 values (3,'Department C');
	insert into t2 values (1,2,'Emp 1');
	insert into t2 values (2,2,'Emp 2');
	insert into t2 values (3,2,'Emp 3');

	create trigger trig after insert on t0 for each row
		delete from t1 where col1=new.col1;

	select * from t2;
lock tables t0 write, t1 write;
	insert into t0 values ('Department B');
unlock tables;
	select * from t2;

# Cleanup
	drop trigger trig;
	drop table t2, t1;


#Section x.x.x.2
# Test case: Checking that triggers can be used as a way to address missing foreign
#            key definition
let $message= Testcase x.x.x.2:;
--source include/show_msg.inc

	--disable_warnings
	DROP TABLE IF EXISTS t1, t2;
	--enable_warnings

	eval CREATE TABLE t1 (id INT NOT NULL, col1 char(50),
		PRIMARY KEY (id)) ENGINE=$engine_type;
	eval CREATE TABLE t2 (id INT PRIMARY KEY, f_id INT,
		INDEX par_ind (f_id), col1 char(50),
		FOREIGN KEY (f_id) REFERENCES t1(id)
		ON UPDATE CASCADE) ENGINE=$engine_type;

	insert into t1 values (1,'Department A');
	insert into t1 values (2,'Department B');
	insert into t1 values (3,'Department C');
	insert into t2 values (1,2,'Emp 1');
	insert into t2 values (2,3,'Emp 2');

	--error ER_NO_REFERENCED_ROW_2
	insert into t2 values (3,4,'Emp 3');

	create trigger tr_t2 before insert on t2 for each row
		insert into t1 values(new.f_id, concat('New Department ', new.f_id));

lock tables t1 write, t2 write;
	insert into t2 values (3,4,'Emp 3');
unlock tables;

	select * from t1;
	select * from t2;

# Cleanup
	drop trigger tr_t2;
	drop table t2, t1, t0;


let $message= Foreign Key tests disabled (bug 11472 - stored in trig_frkey2.test);
--source include/show_msg.inc

