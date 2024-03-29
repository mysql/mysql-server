#======================================================================
#
# Trigger Tests
# (test case numbering refer to requirement document TP v1.1)
#======================================================================

USE test;
--source suite/funcs_1/include/tb3.inc

# OBM - ToDo
############
# 1. Performace
###############################################

--disable_abort_on_error

#####################################################
################# Section 3.5.1 #####################
# Syntax checks for CREATE TRIGGER and DROP TRIGGER #
#####################################################

#Section 3.5.1.1
# Testcase: Ensure that all clauses that should be supported are supported.
let $message= Testcase: 3.5.1.1:;
--source include/show_msg.inc
# OBN - This test case tests basic trigger definition and execution
#       of INSERT/UPDATE/DELETE actions and BEFORE/AFTER timings.
#       As such it covers the equirements in sections 3.5.6.1, 3.5.6.2,
#       3.5.6.4, 3.5.6.5, 3.5.7.1, 3.5.7.2, 3.5.7.3, 3.5.7.17 below.
#     - Note currently as a result of limitations with locking tables in
#       triggers, a specifc lockingof the tables is done.
#       Once fixed, the locking and alias referances should be removed

use test;
# Trigger Definition
	Create trigger trg1_1 BEFORE INSERT
		on tb3 for each row set @test_before = 2, new.f142 = @test_before;
	Create trigger trg1_2 AFTER INSERT
		on tb3 for each row set @test_after = 6;
	Create trigger trg1_4 BEFORE UPDATE
		on tb3 for each row set @test_before = 27,
					new.f142 = @test_before,
					new.f122 = 'Before Update Trigger';
	Create trigger trg1_3 AFTER UPDATE
		on tb3 for each row set @test_after = '15';
	Create trigger trg1_5 BEFORE DELETE on tb3 for each row
		select count(*) into @test_before from tb3 as tr_tb3
			where f121 = 'Test 3.5.1.1';
	Create trigger trg1_6 AFTER DELETE on tb3 for each row
		select count(*) into @test_after from tb3 as tr_tb3
			where f121 = 'Test 3.5.1.1';
# Trigger Execution Insert (before and after)
	set @test_before = 1;
	set @test_after = 5;
        select @test_before, @test_after;
	Insert into tb3 (f121, f122, f142, f144, f134)
		values ('Test 3.5.1.1', 'First Row', @test_before, @test_after, 1);
        --sorted_result
	select f121, f122, f142, f144, f134 from tb3 where f121 = 'Test 3.5.1.1';
        select @test_before, @test_after;

# Trigger Execution Update (before and after)
	set @test_before = 18;
	set @test_after = 8;
        select @test_before, @test_after;
	Update tb3 set  tb3.f122 = 'Update',
			tb3.f142 = @test_before,
			tb3.f144 = @test_after
		where tb3.f121 = 'Test 3.5.1.1';
        --sorted_result
	select f121, f122, f142, f144, f134 from tb3 where f121 = 'Test 3.5.1.1';
        select @test_before, @test_after;

# Trigger Execution Delete (before and after)
	Insert into tb3 (f121, f122, f142, f144, f134)
		values ('Test 3.5.1.1', 'Second Row', 5, 6, 2);
	set @test_before = 0;
	set @test_after = 0;
        --sorted_result
	select f121, f122, f142, f144, f134 from tb3 where f121 = 'Test 3.5.1.1';
        select @test_before, @test_after;
	Delete from tb3 where f121 = 'Test 3.5.1.1' and f134 = 2;
        --sorted_result
	select f121, f122, f142, f144, f134 from tb3 where f121 = 'Test 3.5.1.1';
        select @test_before, @test_after;

#Cleanup
	--disable_warnings
	--error 0, ER_TRG_DOES_NOT_EXIST
	drop trigger trg1_1;
	--error 0, ER_TRG_DOES_NOT_EXIST
	drop trigger trg1_2;
	--error 0, ER_TRG_DOES_NOT_EXIST
	drop trigger trg1_3;
	--error 0, ER_TRG_DOES_NOT_EXIST
	drop trigger trg1_4;
	--error 0, ER_TRG_DOES_NOT_EXIST
	drop trigger trg1_5;
	--error 0, ER_TRG_DOES_NOT_EXIST
	drop trigger trg1_6;
	--enable_warnings
	delete from tb3 where f121='Test 3.5.1.1';

#Section 3.5.1.2
# Testcase: Ensure that all clauses that should not be supported are disallowed
#           with an appropriate error message.
let $message= Testcase: 3.5.1.2:;
--source include/show_msg.inc
	
	--error ER_PARSE_ERROR
	Create trigger trg_1 after insert
		on tb3 for each statement set @x= 1;

#Cleanup
	--disable_warnings
	--error 0, ER_TRG_DOES_NOT_EXIST
	drop trigger trg_1;
	--enable_warnings
	

#Section 3.5.1.3
# Testcase: Ensure that all supported clauses are supported only in the correct order.
let $message= Testcase 3.5.1.3:;
--source include/show_msg.inc
	--error ER_PARSE_ERROR
	CREATE TRIGGER trg3_1 on tb3 BEFORE INSERT for each row set new.f120 = 't';

	--error ER_PARSE_ERROR
	CREATE trg3_2 TRIGGER AFTER INSERT on tb3 for each row set new.f120 = 's';
		
	--error ER_PARSE_ERROR
	CREATE TRIGGER trg3_3 Before DELETE on tb3 set @ret1 = 'test' for each row;
	
	--error ER_PARSE_ERROR
	CREATE TRIGGER trg3_4 DELETE AFTER on tb3 set @ret1 = 'test' for each row;

	--error ER_PARSE_ERROR
	CREATE for each row TRIGGER trg3_5 AFTER UPDATE on tb3 set @ret1 = 'test';
	
#Cleanup
# OBN - Although none of the above should have been created we should do a cleanup
#       since if they have been created, not dropping them will affect following
#       tests.
	--disable_warnings
	--error 0, ER_TRG_DOES_NOT_EXIST
	drop trigger trg3_1;
	--error 0, ER_TRG_DOES_NOT_EXIST
	drop trigger trg3_2;
	--error 0, ER_TRG_DOES_NOT_EXIST
	drop trigger trg3_3;
	--error 0, ER_TRG_DOES_NOT_EXIST
	drop trigger trg3_4;
	--error 0, ER_TRG_DOES_NOT_EXIST
	drop trigger trg3_5;
	--enable_warnings


#Section 3.5.1.4
# Testcase: Ensure that an appropriate error message is returned if a clause
#           is out-of-order in an SQL statement.
# OBN - FIXME - Missing 3.5.1.4 need to add

#Section 3.5.1.5
# Testcase: Ensure that all clauses that are defined to be mandatory are indeed
#           required to be mandatory by the MySQL server and tools
let  $message= Testcase: 3.5.1.5:;
--source include/show_msg.inc

	--error ER_PARSE_ERROR
	CREATE TRIGGER trg4_1 AFTER on tb3 for each row set new.f120 = 'e';

	--error ER_PARSE_ERROR
	CREATE TRIGGER trg4_2 INSERT on tb3 for each set row  new.f120 = 'f';

	--error ER_PARSE_ERROR
	CREATE TRIGGER trg4_3 BEFORE INSERT tb3 for each row set new.f120 = 'g';

	--error ER_PARSE_ERROR
	CREATE TRIGGER trg4_4 AFTER UPDATE on tb3 for each set new.f120 = 'g';

	--error ER_PARSE_ERROR
	CREATE trg4_5 AFTER DELETE on tb3 for each set new.f120 = 'g';

	--error ER_PARSE_ERROR
	CREATE TRIGGER trg4_6 BEFORE DELETE for each row set new.f120 = 'g';

#Cleanup
# OBN - Although none of the above should have been created we should do a cleanup
#       since if they have been created, not dropping them will affect following
#       tests.
	--disable_warnings
	--error 0, ER_TRG_DOES_NOT_EXIST
	drop trigger trg4_1;
	--error 0, ER_TRG_DOES_NOT_EXIST
	drop trigger trg4_2;
	--error 0, ER_TRG_DOES_NOT_EXIST
	drop trigger trg4_3;
	--error 0, ER_TRG_DOES_NOT_EXIST
	drop trigger trg4_4;
	--error 0, ER_TRG_DOES_NOT_EXIST
	drop trigger trg4_5;
	--error 0, ER_TRG_DOES_NOT_EXIST
	drop trigger trg4_6;
	--enable_warnings

#Section 3.5.1.6
# Testcase: Ensure that any clauses that are defined to be optional are indeed
#           trated as optional by MySQL server and tools
let $message= Testcase 3.5.1.6: - Need to fix;
--source include/show_msg.inc
# OBN - FIXME - Missing 3.5.1.6 need to add

#Section 3.5.1.7
# Testcase: Ensure that all valid, fully-qualified, and non-qualified,
#           trigger names are accepted, at creation time.
let $message= Testcase 3.5.1.7: - need to fix;
--source include/show_msg.inc

	drop table if exists t1;
        --replace_result $engine_type <engine_to_be_used>
	eval create table t1 (f1 int, f2 char(25),f3 int) engine = $engine_type;
	CREATE TRIGGER trg5_1 BEFORE INSERT on test.t1
		for each row set new.f3 = '14';
# In 5.0 names to long (more than 64 chars) were trimed without an error
# In 5.1 an error is returned. So adding a call with the expected error
# and one with a shorter name to validate proper execution
	--error ER_TOO_LONG_IDENT
	CREATE TRIGGER trg_abcdefghijklmnopqrstuvwxyz1234567890ABCDEFGHIJKLMNOPQRSTUVWXYZ
		BEFORE UPDATE on test.t1 for each row set new.f3 = '42';
	CREATE TRIGGER trg_abcdefghijklmnopqrstuvwxyz1234567890ABCDEFGHIJKLMNOPQRSTUVWX
		BEFORE UPDATE on test.t1 for each row set new.f3 = '42';

	insert into t1 (f2) values ('insert 3.5.1.7');
	select * from t1;
	update t1 set f2='update 3.5.1.7';
	select * from t1;
	select trigger_name from information_schema.triggers where trigger_schema != 'sys' order by trigger_name;

#Cleanup
	--disable_warnings
	--error 0, ER_TRG_DOES_NOT_EXIST
	drop trigger trg5_1;
	# In 5.1 the long name should generate an error that is to long
	--error ER_TOO_LONG_IDENT
	drop trigger trg_abcdefghijklmnopqrstuvwxyz1234567890ABCDEFGHIJKLMNOPQRSTUVWXYZ;
	drop trigger trg_abcdefghijklmnopqrstuvwxyz1234567890ABCDEFGHIJKLMNOPQRSTUVWX;
	drop table t1;

#Section 3.5.1.8
# Testcase: Ensure that any invalid trigger name is never accepted, and that an
#            appropriate error message is returned when the name is rejected.
let $message= Testcase 3.5.1.8:;
--source include/show_msg.inc

	--error ER_PARSE_ERROR
	CREATE TRIGGER trg12* before insert on tb3 for each row set new.f120 = 't';

	--error ER_PARSE_ERROR
	CREATE TRIGGER trigger before insert on tb3 for each row set new.f120 = 't';

	--error ER_PARSE_ERROR
	CREATE TRIGGER 100 before insert on tb3 for each row set new.f120 = 't';

	--error ER_PARSE_ERROR
	CREATE TRIGGER @@view before insert on tb3 for each row set new.f120 = 't';

	--error ER_PARSE_ERROR
	CREATE TRIGGER @name before insert on tb3 for each row set new.f120 = 't';

	--error ER_TRG_IN_WRONG_SCHEMA
	CREATE TRIGGER tb3.trg6_1 BEFORE INSERT on test.tb3
		for each row set new.f120 ='X';
	
	--disable_warnings
        drop database if exists trig_db;
	--enable_warnings
	create database trig_db;
	use trig_db;
        --replace_result $engine_type <engine_to_be_used>
	eval create table t1 (f1 integer) engine = $engine_type;

	# Can't create a trigger in a different database
	use test;
	--error ER_NO_SUCH_TABLE
	CREATE TRIGGER trig_db.trg6_2 AFTER INSERT on tb3
		for each row set @ret_trg6_2 = 5;

	# Can't create a trigger refrencing a table in a different db
	use trig_db;
	--error ER_TRG_IN_WRONG_SCHEMA
	CREATE TRIGGER trg6_3 AFTER INSERT on test.tb3
		for each row set @ret_trg6_3 = 18;

	use test;

#Cleanup
	--disable_warnings
	drop database trig_db;
# OBN - Although none of the above should have been created we should do a cleanup
#       since if they have been created, not dropping them will affect following
#       tests.
	--error 0, ER_TRG_DOES_NOT_EXIST
	drop trigger trg6_1;
	--error 0, ER_TRG_DOES_NOT_EXIST
	drop trigger trg6_3;
	--enable_warnings

#Section 3.5.1.9
#Testcase:  Ensure that a reference to a non-existent trigger is rejected with
#           an appropriate error message.
let $message= Testcase 3.5.1.9:(cannot be inplemented at this point);
--source include/show_msg.inc


#Section 3.5.1.10
#Testcase: Ensure that it is not possible to create two triggers with the same name on
#          the same table
let $message= Testcase 3.5.1.10:;
--source include/show_msg.inc

	CREATE TRIGGER trg7_1 BEFORE UPDATE on tb3 for each row set new.f120 ='X';

	--error ER_TRG_ALREADY_EXISTS
	CREATE TRIGGER trg7_1 AFTER INSERT on tb3 for each row set @x ='Y';

#Cleanup
	--disable_warnings
	--error 0, ER_TRG_DOES_NOT_EXIST
	drop trigger trg7_1;
	--enable_warnings


#Section 3.5.1.?
# Testcase: Ensure that it is not possible to create two or more triggers with
#           the same name, provided each is associated with a different table.
let $message= Testcase 3.5.1.?:;
--source include/show_msg.inc

	--disable_warnings
	drop table if exists t1;
	drop table if exists t2;
	--enable_warnings
        --replace_result $engine_type <engine_to_be_used>
	eval create table t1 (f1 char(50), f2 integer) engine = $engine_type;
        --replace_result $engine_type <engine_to_be_used>
	eval create table t2 (f1 char(50), f2 integer) engine = $engine_type;

	create trigger trig before insert on t1
		for each row set new.f1 ='trig t1';

	--error ER_TRG_ALREADY_EXISTS
	create trigger trig before update on t2
		for each row set new.f1 ='trig t2';

	insert into t1 value ('insert to t1',1);
	select * from t1;
	update t1 set f1='update to t1';
	select * from t1;
	insert into t2 value ('insert to t2',2);
	update t2 set f1='update to t1';
	select * from t2;

#Cleanup
	--disable_warnings
	drop table t1;
	drop table t2;
	--error 0, ER_TRG_DOES_NOT_EXIST
	drop trigger trig;
	--enable_warnings


#Section 3.5.1.11
# Testcase: Ensure that it is possible to create two or more triggers with
#           the same name, provided each resides in a different database
let $message= Testcase 3.5.1.11:;
--source include/show_msg.inc

	--disable_warnings
        drop database if exists trig_db1;
        drop database if exists trig_db2;
        drop database if exists trig_db3;
	--enable_warnings
	create database trig_db1;
	create database trig_db2;
	create database trig_db3;
	use trig_db1;
        --replace_result $engine_type <engine_to_be_used>
	eval create table t1 (f1 char(50), f2 integer) engine = $engine_type;
	create trigger trig before insert on t1
		for each row set new.f1 ='trig1', @test_var1='trig1';
	use trig_db2;
        --replace_result $engine_type <engine_to_be_used>
	eval create table t2 (f1 char(50), f2 integer) engine = $engine_type;
	create trigger trig before insert on t2
		for each row set new.f1 ='trig2', @test_var2='trig2';
	use trig_db3;
        --replace_result $engine_type <engine_to_be_used>
	eval create table t1 (f1 char(50), f2 integer) engine = $engine_type;
	create trigger trig before insert on t1
		for each row set new.f1 ='trig3', @test_var3='trig3';

	set @test_var1= '', @test_var2= '', @test_var3= '';
	use trig_db1;
	insert into t1 (f1,f2) values ('insert to db1 t1',1);
	insert into trig_db1.t1 (f1,f2) values ('insert to db1 t1 from db1',2);
	insert into trig_db2.t2 (f1,f2) values ('insert to db2 t2 from db1',3);
	insert into trig_db3.t1 (f1,f2) values ('insert to db3 t1 from db1',4);
	select @test_var1, @test_var2, @test_var3;
	select * from t1 order by f2;
	select * from trig_db2.t2;
	select * from trig_db3.t1;
	select * from t1 order by f2;
	use test;

#Cleanup
	--disable_warnings
	drop database trig_db1;
	drop database trig_db2;
	drop database trig_db3;
	--enable_warnings

###########################################
################ Section 3.5.2 ############
# Check for the global nature of Triggers #
###########################################

#Section 3.5.2.1
# Test case: Ensure that if a trigger created without a qualifying database
#            name belongs to the database in use at creation time.
#Section 3.5.2.2
# Test case: Ensure that if a trigger created with a qualifying database name
#            belongs to the database specified.
#Section 3.5.2.3
# Test case: Ensure that if a trigger created with a qualifying database name
#            does not belong to the database in use at creation time unless
#            the qualifying database name identifies the database that is
#            also in use at creation time.
let $message= Testcase 3.5.2.1/2/3:;
--source include/show_msg.inc


	--disable_warnings
        drop database if exists trig_db1;
        drop database if exists trig_db2;
	--enable_warnings
	create database trig_db1;
	create database trig_db2;
	use trig_db1;
        --replace_result $engine_type <engine_to_be_used>
	eval create table t1 (f1 char(50), f2 integer) engine = $engine_type;
        --replace_result $engine_type <engine_to_be_used>
	eval create table trig_db2.t1 (f1 char(50), f2 integer) engine = $engine_type;
	create trigger trig1_b before insert on t1
		for each row set @test_var1='trig1_b';
	create trigger trig_db1.trig1_a after insert on t1
		for each row set @test_var2='trig1_a';
	create trigger trig_db2.trig2 before insert on trig_db2.t1
		for each row set @test_var3='trig2';
	select trigger_schema, trigger_name, event_object_table
        from information_schema.triggers
        where trigger_schema like 'trig_db%'
        order by trigger_name;

	set @test_var1= '', @test_var2= '', @test_var3= '';
	insert into t1 (f1,f2) values ('insert to db1 t1 from db1',352);
	insert into trig_db2.t1 (f1,f2) values ('insert to db2 t1 from db1',352);
	select @test_var1, @test_var2, @test_var3;

#Cleanup
	--disable_warnings
	drop database trig_db1;
	drop database trig_db2;
        --enable_warnings
DROP TABLE test.tb3;
