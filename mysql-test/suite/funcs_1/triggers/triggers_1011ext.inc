#======================================================================
#
# Trigger Tests
# (test case numbering refer to requirement document TP v1.1)
#======================================================================
# WL#4084: enable disabled parts, 2007-11-15, hhunger

--disable_abort_on_error

##############################################
################ Section 3.5.10 #################
# Check on Trigger Activation
##############################################
#Section 3.5.10.1
# Test case: Ensure that every trigger that should be activated by
#            every possible type of implicit insertion into its subject
#            table (INSERT into a view based on the subject table) is
#            indeed activated correctly
#Section 3.5.10.2
# Test case: Ensure that every trigger that should be activated by every
#            possible type of implicit insertion into its subject table
#            (UPDATE into a view based on the subject table) is indeed
#            activated correctly
#Section 3.5.10.3
# Test case: Ensure that every trigger that should be activated by every
#            possible type of implicit insertion into its subject table
#            (DELETE from a view based on the subject table) is indeed
#            activated correctly
let $message= Testcase 3.5.10.1/2/3:;
--source include/show_msg.inc

	Create view vw11 as select * from tb3
		 where f122 like 'Test 3.5.10.1/2/3%';
	Create trigger trg1a before insert on tb3
		for each row set new.f163=111.11;
	Create trigger trg1b after insert on tb3
		for each row set @test_var='After Insert';
	Create trigger trg1c before update on tb3
		for each row set new.f121='Y', new.f122='Test 3.5.10.1/2/3-Update';
	Create trigger trg1d after update on tb3
		for each row set @test_var='After Update';
	Create trigger trg1e before delete on tb3
		for each row set @test_var=5;
	Create trigger trg1f after delete on tb3
		for each row set @test_var= 2* @test_var+7;

#Section 3.5.10.1
	Insert into vw11 (f122, f151) values ('Test 3.5.10.1/2/3', 1);
	Insert into vw11 (f122, f151) values ('Test 3.5.10.1/2/3', 2);
	Insert into vw11 (f122, f151) values ('Not in View', 3);
	select f121, f122, f151, f163
		from tb3 where f122 like 'Test 3.5.10.1/2/3%' order by f151;
	select f121, f122, f151, f163 from vw11;
	select f121, f122, f151, f163
		from tb3 where f122 like 'Not in View';

#Section 3.5.10.2
	Update vw11 set f163=1;
	select f121, f122, f151, f163 from tb3
		where f122 like 'Test 3.5.10.1/2/3%' order by f151;
	select f121, f122, f151, f163 from vw11;

#Section 3.5.10.3
	set @test_var=0;
	Select @test_var as 'before delete';
	delete from vw11 where f151=1;
	select f121, f122, f151, f163 from tb3
		where f122 like 'Test 3.5.10.1/2/3%' order by f151;
	select f121, f122, f151, f163 from vw11;
	Select @test_var as 'after delete';

#Cleanup
	--disable_warnings
	drop view vw11;
	drop trigger trg1a;
	drop trigger trg1b;
	drop trigger trg1c;
	drop trigger trg1d;
	drop trigger trg1e;
	drop trigger trg1f;
	delete from tb3 where f122 like 'Test 3.5.10.1/2/3%';
	--enable_warnings


#Section 3.5.10.4
# Test case: Ensure that every trigger that should be activated by every
#            possible type of implicit insertion into its subject table
#            (LOAD into the subject table) is indeed activated correctly
let $message= Testcase 3.5.10.4:;
--source include/show_msg.inc

	eval create table tb_load (f1 int, f2 char(25),f3 int) engine=$engine_type;
	Create trigger trg4 before insert on tb_load
		for each row set new.f3=-(new.f1 div 5), @counter= @counter+1;

	set @counter= 0;
	select @counter as 'Rows Loaded Before';
	--replace_result $MYSQL_TEST_DIR MYSQL_TEST_DIR
	eval load data infile '$MYSQL_TEST_DIR/suite/funcs_1/data/t9.txt' into table tb_load;

	select @counter as 'Rows Loaded After';
	Select * from tb_load order by f1 limit 10;

#Cleanup
	--disable_warnings
	drop trigger trg4;
	drop table tb_load;
	--enable_warnings


#Section 3.5.10.5
# Testcase: Ensure that every trigger that should be activated by every possible
#           type of implicit update of its subject table (e.g.a FOREIGN KEY SET
#           DEFAULT action or an UPDATE of a view based on the subject table) is
#           indeed activated correctly
let $message= Testcase 3.5.10.5: (implemented in trig_frkey.test);
--source include/show_msg.inc


#Section 3.5.10.6
# Testcase: Ensure that every trigger that should be activated by every possible
#           type of implicit deletion from its subject table (e.g.a FOREIGN KEY
#           CASCADE action or a DELETE from a view based on the subject table) is
#           indeed activated correctly
let $message= Testcase 3.5.10.6: (implemented in trig_frkey.test);
--source include/show_msg.inc

#Section 3.5.10.extra
# Testcase: Ensure that every trigger that should be activated by every possible
#           type of implicit deletion from its subject table (e.g. an action performed
#           on the subject table from a stored procedure is indeed activated correctly
let $message= Testcase 3.5.10.extra:;
--source include/show_msg.inc

	eval create table t1_sp (var136 tinyint, var151 decimal) engine=$engine_type;

	create trigger trg before insert on t1_sp
		for each row set @counter=@counter+1;
		# declare continue handler for sqlstate '01000' set done = 1;

	delimiter //;
	create procedure trig_sp()
	begin
		declare done int default 0;
		declare var151 decimal;
		declare var136 tinyint;
		declare cur1 cursor for select f136, f151 from tb3;
		declare continue handler for sqlstate '01000' set done = 1;
		open cur1;
		fetch cur1 into var136, var151;
		wl_loop: WHILE NOT done DO
			insert into t1_sp values (var136, var151);
			fetch cur1 into var136, var151;
		END WHILE wl_loop;
		close cur1;
	end//
	delimiter ;//

	set @counter=0;
	select @counter;
	--error 1329
	call trig_sp();
	select @counter;
	select count(*) from tb3;
	select count(*) from t1_sp;

#Cleanup
	--disable_warnings
	drop procedure trig_sp;
	drop trigger trg;
	drop table t1_sp;
	--enable_warnings

##################################
########## Section 3.5.11 ########
# Check on Trigger Performance   #
##################################
#Section 3.5.11.1
# Testcase: Ensure that a set of complicated, interlocking triggers that are activated
#           by multiple trigger events on no fewer than 50 different tables with at least
#           500,000 rows each, all work correctly, return the correct results, and have
#           the correct effects on the database. It is expected that the Services Provider
#           will use its own skills and experience in database testing to devise tables and
#           triggers that fulfill this requirement.
let $message= Testcase 3.5.11.1 (implemented in trig_perf.test);
--source include/show_msg.inc


##########################################
# Other Scenasrios (not in requirements) #
##########################################
# Testcase: y.y.y.2:
# Checking for triggers starting triggers (no direct requirement)
let $message= Testcase y.y.y.2: Check for triggers starting triggers;
--source include/show_msg.inc

	use test;
	--disable_warnings
	drop table if exists t1;
	drop table if exists t2_1;
	drop table if exists t2_2;
	drop table if exists t2_3;
	drop table if exists t2_4;
	drop table if exists t3;
	--enable_warnings

	eval create table t1 (f1 integer) engine=$engine_type;
	eval create table t2_1 (f1 integer) engine=$engine_type;
	eval create table t2_2 (f1 integer) engine=$engine_type;
	eval create table t2_3 (f1 integer) engine=$engine_type;
	eval create table t2_4 (f1 integer) engine=$engine_type;
	eval create table t3 (f1 integer) engine=$engine_type;

	insert into t1 values (1);
	delimiter //;
	create trigger tr1 after insert on t1 for each row
	BEGIN
		insert into t2_1 (f1) values (new.f1+1);
		insert into t2_2 (f1) values (new.f1+1);
		insert into t2_3 (f1) values (new.f1+1);
		insert into t2_4 (f1) values (new.f1+1);
	END//
	delimiter ;//

	create trigger tr2_1 after insert on t2_1 for each row
		insert into t3 (f1) values (new.f1+10);
	create trigger tr2_2 after insert on t2_2 for each row
		insert into t3 (f1) values (new.f1+100);
	create trigger tr2_3 after insert on t2_3 for each row
		insert into t3 (f1) values (new.f1+1000);
	create trigger tr2_4 after insert on t2_4 for each row
		insert into t3 (f1) values (new.f1+10000);

#lock tables t1 write, t2_1 write, t2_2 write, t2_3 write, t2_4 write, t3 write;
	insert into t1 values (1);
#unlock tables;
	select * from t3 order by f1;

#Cleanup
	--disable_warnings
	drop trigger tr1;
	drop trigger tr2_1;
	drop trigger tr2_2;
	drop trigger tr2_3;
	drop trigger tr2_4;
	drop table t1, t2_1, t2_2, t2_3, t2_4, t3;
	--enable_warnings

# Testcase: y.y.y.3:
# Checking for circular trigger definitions
let $message= Testcase y.y.y.3: Circular trigger reference;
--source include/show_msg.inc
	use test;
	--disable_warnings
	drop table if exists t1;
	drop table if exists t2;
	drop table if exists t3;
	drop table if exists t4;
	--enable_warnings
	eval create table t1 (f1 integer) engine = $engine_type;
	eval create table t2 (f2 integer) engine = $engine_type;
	eval create table t3 (f3 integer) engine = $engine_type;
	eval create table t4 (f4 integer) engine = $engine_type;

	insert into t1 values (0);
	create trigger tr1 after insert on t1
		for each row insert into t2 (f2) values (new.f1+1);
	create trigger tr2 after insert on t2
		for each row insert into t3 (f3) values (new.f2+1);
	create trigger tr3 after insert on t3
		for each row insert into t4 (f4) values (new.f3+1);
	create trigger tr4 after insert on t4
		for each row insert into t1 (f1) values (new.f4+1);

        # Bug#11896 Partial locking in case of recursive trigger definittions
	--error ER_CANT_UPDATE_USED_TABLE_IN_SF_OR_TRG
	insert into t1 values (1);
	select * from t1 order by f1;
	select * from t2 order by f2;
	select * from t3 order by f3;
	select * from t4 order by f4;

#Cleanup
	--disable_warnings
	drop trigger tr1;
	drop trigger tr2;
	drop trigger tr3;
	drop trigger tr4;
	drop table t1;
	drop table t2;
	drop table t3;
	drop table t4;
	--enable_warnings


#Section y.y.y.4
# Testcase: create recursive trigger/storedprocedures conditions
let $message= Testcase y.y.y.4: Recursive trigger/SP references;
--source include/show_msg.inc

set @sql_mode='traditional';
	eval create table t1_sp (
		count integer,
		var136 tinyint,
		var151 decimal) engine=$engine_type;

	delimiter //;
	create procedure trig_sp()
	begin
		declare done int default 0;
		declare var151 decimal;
		declare var136 tinyint;
		declare cur1 cursor for select f136, f151 from tb3;
		declare continue handler for sqlstate '01000' set done = 1;
		set @counter= @counter+1;
		open cur1;
		fetch cur1 into var136, var151;
		wl_loop: WHILE NOT done DO
			insert into t1_sp values (@counter, var136, var151);
			fetch cur1 into var136, var151;
		END WHILE wl_loop;
		close cur1;
	end//
	delimiter ;//

	create trigger trg before insert on t1_sp
		for each row call trig_sp();

	set @counter=0;
	select @counter;
	--error 1456
	call trig_sp();
	select @counter;
	select count(*) from tb3;
	select count(*) from t1_sp;

	# check recursion will not work here:
	set @@max_sp_recursion_depth= 10;
	set @counter=0;
	select @counter;
	--error 1442
	call trig_sp();
	select @counter;
	select count(*) from tb3;
	select count(*) from t1_sp;

#Cleanup
	--disable_warnings
	drop procedure trig_sp;
	drop trigger trg;
	drop table t1_sp;
	--enable_warnings


# Testcase: y.y.y.5:
# Checking rollback of nested trigger definitions
let $message= Testcase y.y.y.5: Roleback of nested trigger references;
--source include/show_msg.inc

	set @@sql_mode='traditional';
	use test;
	--disable_warnings
	drop table if exists t1;
	drop table if exists t2;
	drop table if exists t3;
	drop table if exists t4;
	--enable_warnings
	eval create table t1 (f1 integer) engine = $engine_type;
	eval create table t2 (f2 integer) engine = $engine_type;
	eval create table t3 (f3 integer) engine = $engine_type;
	eval create table t4 (f4 tinyint) engine = $engine_type;
	show create table t1;
	insert into t1 values (1);
	create trigger tr1 after insert on t1
		for each row insert into t2 (f2) values (new.f1+1);
	create trigger tr2 after insert on t2
		for each row insert into t3 (f3) values (new.f2+1);
	create trigger tr3 after insert on t3
		for each row insert into t4 (f4) values (new.f3+1000);

#lock tables t1 write, t2 write, t3 write, t4 write;

	set autocommit=0;
	start transaction;
	--error 1264
	insert into t1 values (1);
	commit;
	select * from t1 order by f1;
	select * from t2 order by f2;
	select * from t3 order by f3;
#unlock tables;
#Cleanup
	--disable_warnings
	drop trigger tr1;
	drop trigger tr2;
	drop trigger tr3;
	drop table t1;
	drop table t2;
	drop table t3;
	drop table t4;
	--enable_warnings

