#======================================================================
#
# Trigger Tests
# (test case numbering refer to requirement document TP v1.1)
#======================================================================
# WL#4084: enable disabled parts, 2007-11-15 hhunger

USE test;
--source suite/funcs_1/include/tb3.inc



# General setup for Trigger tests
let $message= Testcase: 3.5:;
--source include/show_msg.inc

--disable_abort_on_error

	create User test_general@localhost;
        alter user test_general@localhost identified by 'PWD';
	revoke ALL PRIVILEGES, GRANT OPTION FROM test_general@localhost;

	create User test_super@localhost;
        alter user test_super@localhost identified by 'PWD';
	grant ALL on *.* to test_super@localhost with grant OPTION;
	connect (con2_general,localhost,test_general,PWD,test,$MASTER_MYPORT,$MASTER_MYSOCK);
	connect (con2_super,localhost,test_super,PWD,test,$MASTER_MYPORT,$MASTER_MYSOCK);
	connection default;

#################################
####### Section 3.5.8 ###########
# Checks on Triggered Actions   #
#################################

#Section 3.5.8.1
# Testcase: Ensure that the triggered action of every trigger always executes
#           correctly and the results in all expected changes made to the database
let $message= Testcase 3.5.8.1: (implied in previous tests);
--source include/show_msg.inc

#Section 3.5.8.2
# Testcase: Ensure that the triggered actions of every trigger never results
#           in an unexpected change made to the database.
let $message= Testcase 3.5.8.2: (implied in previous tests);
--source include/show_msg.inc


#Section 3.5.8.3 / 3.5.8.4
#Test case: Ensure that the triggered action can any valid SQL statement / set
#           of valid SQL statements, provided the statements are written within
#           a BEGIN/END compound statement construct
# OBN - At this point the tests focuses on the the INSERT/UPDATE/DELETE SQL statements
#       as there are the most likely to be used in triggers
let $message= Testcase 3.5.8.3/4:;
--source include/show_msg.inc

# creating test tables to perform the trigger SQL on
	connection con2_super;
	create database db_test;
        create user test_general;
	grant SELECT, INSERT, UPDATE, DELETE on db_test.* to test_general;
	grant LOCK TABLES on db_test.* to test_general;
	Use db_test;
        --replace_result $engine_type <engine_to_be_used>
	eval create table t1_i (
		i120 char ascii not null DEFAULT b'101',
		i136 smallint zerofill not null DEFAULT 999,
		i144 int zerofill not null DEFAULT 99999,
		i163 decimal (63,30)) engine=$engine_type;
        --replace_result $engine_type <engine_to_be_used>
	eval create table t1_u (
		u120 char ascii not null DEFAULT b'101',
		u136 smallint zerofill not null DEFAULT 999,
		u144 int zerofill not null DEFAULT 99999,
		u163 decimal (63,30)) engine=$engine_type;
        --replace_result $engine_type <engine_to_be_used>
	eval create table t1_d (
		d120 char ascii not null DEFAULT b'101',
		d136 smallint zerofill not null DEFAULT 999,
		d144 int zerofill not null DEFAULT 99999,
		d163 decimal (63,30)) engine=$engine_type;
	Insert into t1_u values ('a',111,99999,999.99);
	Insert into t1_u values ('b',222,99999,999.99);
	Insert into t1_u values ('c',333,99999,999.99);
	Insert into t1_u values ('d',222,99999,999.99);
	Insert into t1_u values ('e',222,99999,999.99);
	Insert into t1_u values ('f',333,99999,999.99);
	Insert into t1_d values ('a',111,99999,999.99);
	Insert into t1_d values ('b',222,99999,999.99);
	Insert into t1_d values ('c',333,99999,999.99);
	Insert into t1_d values ('d',444,99999,999.99);
	Insert into t1_d values ('e',222,99999,999.99);
	Insert into t1_d values ('f',222,99999,999.99);

let $message= 3.5.8.4 - multiple SQL;
--source include/show_msg.inc
# Trigger definition - multiple SQL
	use test;
	delimiter //;
	Create trigger trg1 AFTER INSERT on tb3 for each row
	BEGIN
		insert into db_test.t1_i
			values (new.f120, new.f136, new.f144, new.f163);
		update db_test.t1_u
			set u144=new.f144, u163=new.f163
			where u136=new.f136;
		delete from db_test.t1_d where d136= new.f136;
		select sum(db_test.t1_u.u163) into @test_var from db_test.t1_u
			where u136= new.f136;
	END//
	delimiter ;//

# Test trigger execution - multiple SQL
	connection con2_general;
	Use test;
	set @test_var=0;
	Insert into tb3 (f120, f122, f136, f144, f163)
		values ('1', 'Test 3.5.8.4', 222, 23456, 1.05);
	Select f120, f122, f136, f144, f163 from tb3 where f122= 'Test 3.5.8.4';
	--sorted_result
	select * from db_test.t1_i;
	--sorted_result
	select * from db_test.t1_u;
	--sorted_result
	select * from db_test.t1_d;
	select @test_var;


let $message= 3.5.8.4 - single SQL - insert;
--source include/show_msg.inc
# Trigger definition - single SQL Insert
	connection con2_super;
        delimiter //;
	Create trigger trg2 BEFORE UPDATE on tb3 for each row
        BEGIN
		insert into db_test.t1_i
			values (new.f120, new.f136, new.f144, new.f163);
        END//
        delimiter ;//

# Trigger exeution - single SQL Insert
	connection con2_general;
        Select f120, f122, f136, f144, f163 from tb3 where f122 like 'Test 3.5.8.4%';
        select * from db_test.t1_i order by i120;
	update tb3 set f120='I', f122='Test 3.5.8.4-Single Insert'
		 where f122='Test 3.5.8.4';
	Select f120, f122, f136, f144, f163 from tb3 where f122 like 'Test 3.5.8.4%';
	select * from db_test.t1_i order by i120;


let $message= 3.5.8.4 - single SQL - update;
--source include/show_msg.inc
# Trigger definition - single SQL update
	connection con2_super;
	drop trigger trg2;
	Create trigger trg3 BEFORE UPDATE on tb3 for each row
		update db_test.t1_u
			set u120=new.f120
			where u136=new.f136;

# Trigger exeution - single SQL  - update;
	connection con2_general;
	update tb3 set f120='U', f122='Test 3.5.8.4-Single Update'
		 where f122='Test 3.5.8.4-Single Insert';
	Select f120, f122, f136, f144, f163 from tb3 where f122 like 'Test 3.5.8.4%';
	select * from db_test.t1_u order by u120;


let $message= 3.5.8.3/4 - single SQL - delete;
--source include/show_msg.inc
# Trigger definition - single SQL delete
	connection con2_super;
	drop trigger trg3;
	Create trigger trg4 AFTER UPDATE on tb3 for each row
		delete from db_test.t1_d where d136= new.f136;

# Trigger exeution - single SQL delete
	connection con2_general;
#lock tables tb3 write, db_test.t1_i write, db_test.t1_u write, db_test.t1_d write;
	update tb3 set f120='D', f136=444,
		f122='Test 3.5.8.4-Single Delete'
		 where f122='Test 3.5.8.4-Single Update';
#unlock tables;
	Select f120, f122, f136, f144, f163 from tb3 where f122 like 'Test 3.5.8.4%';
	select * from db_test.t1_d order by d120;


let $message= 3.5.8.3/4 - single SQL - select;
--source include/show_msg.inc
# Trigger definition - single SQL select
	connection con2_super;
	drop trigger trg4;
	Create trigger trg5 AFTER UPDATE on tb3 for each row
		select sum(db_test.t1_u.u163) into @test_var from db_test.t1_u
			where u136= new.f136;

# Trigger exeution - single SQL select
	connection con2_general;
	set @test_var=0;
	update tb3 set f120='S', f136=111,
		f122='Test 3.5.8.4-Single Select'
		 where f122='Test 3.5.8.4-Single Delete';
	Select f120, f122, f136, f144, f163 from tb3 where f122 like 'Test 3.5.8.4%';
	select @test_var;

#Cleanup
	connection default;
	--disable_warnings
	drop trigger trg1;
	drop trigger trg5;
	drop database if exists db_test;
	delete from tb3 where f122 like 'Test 3.5.8.4%';
	revoke ALL PRIVILEGES, GRANT OPTION FROM 'test_general'@'localhost';
	--enable_warnings


#Section 3.5.8.5 (IF)
# Test case: Ensure that the stored procedure-specific flow control statement like IF
#            works correctly when it is a part of the triggered action portion of a
#            trigger definition.
let $message= Testcase 3.5.8.5 (IF):;
--source include/show_msg.inc

	delimiter //;
	create trigger trg2 before insert on tb3 for each row
	BEGIN
		IF new.f120='1' then
			set @test_var='one', new.f120='2';
		ELSEIF new.f120='2' then
			set @test_var='two', new.f120='3';
		ELSEIF new.f120='3' then
			set @test_var='three', new.f120='4';
		END IF;

		IF (new.f120='4') and (new.f136=10) then
			set @test_var2='2nd if', new.f120='d';
		ELSE
			set @test_var2='2nd else', new.f120='D';
		END IF;
	END//
	delimiter ;//

	set @test_var='Empty', @test_var2=0;
	Insert into tb3 (f120, f122, f136) values ('1', 'Test 3.5.8.5-if', 101);
	select f120, f122, f136, @test_var, @test_var2
		from tb3 where f122 = 'Test 3.5.8.5-if' order by f136;
	Insert into tb3 (f120, f122, f136) values ('2', 'Test 3.5.8.5-if', 102);
	select f120, f122, f136, @test_var, @test_var2
		from tb3 where f122 = 'Test 3.5.8.5-if' order by f136;
	Insert into tb3 (f120, f122, f136) values ('3', 'Test 3.5.8.5-if', 10);
	select f120, f122, f136, @test_var, @test_var2
		from tb3 where f122 = 'Test 3.5.8.5-if' order by f136;
	Insert into tb3 (f120, f122, f136) values ('3', 'Test 3.5.8.5-if', 103);
	select f120, f122, f136, @test_var, @test_var2
		from tb3 where f122 = 'Test 3.5.8.5-if' order by f136;

	delimiter //;
	--error ER_PARSE_ERROR
	create trigger trg3 before update on tb3 for each row
	BEGIN
		ELSEIF new.f120='2' then
		END IF;
	END//
	--error 0, ER_TRG_DOES_NOT_EXIST
	drop trigger trg3//

	--error ER_PARSE_ERROR
	create trigger trg4 before update on tb3 for each row
	BEGIN
		IF (new.f120='4') and (new.f136=10) then
			set @test_var2='2nd if', new.f120='d';
		ELSE
			set @test_var2='2nd else', new.f120='D';
	END//
	delimiter ;//
	--error 0, ER_TRG_DOES_NOT_EXIST
	drop trigger trg4;

#Cleanup
	--disable_warnings
	drop trigger trg2;
	delete from tb3 where f121='Test 3.5.8.5-if';	
	--enable_warnings


#Section 3.5.8.5 (CASE)
# Test case: Ensure that the stored procedure-specific flow control statement
#            like CASE works correctly when it is a part of the triggered action
#            portion of a trigger definition.
let $message= Testcase 3.5.8.5-case:;
--source include/show_msg.inc
	SET sql_mode = 'NO_ENGINE_SUBSTITUTION';
	delimiter //;
	create trigger trg3 before insert on tb3 for each row
	BEGIN
		SET new.f120=char(ascii(new.f120)-32);
		CASE
			when new.f136<100 then set new.f136=new.f136+120;
			when new.f136<10 then set new.f144=777;
			when new.f136>100 then set new.f120=new.f136-1;
		END case;
		CASE
			when new.f136=200 then set @test_var=CONCAT(new.f120, '=');
			ELSE set @test_var=concat(new.f120, '*');
		END case;
		CASE new.f144
			when 1 then set @test_var=concat(@test_var, 'one');
			when 2 then set @test_var=concat(@test_var, 'two');
			when 3 then set @test_var=concat(@test_var, 'three');
			when 4 then set @test_var=concat(@test_var, 'four');
			when 5 then set @test_var=concat(@test_var, 'five');
			when 6 then set @test_var=concat(@test_var, 'six');
			when 7 then set @test_var=concat(@test_var, 'seven');
			when 8 then set @test_var=concat(@test_var, 'eight');
			when 9 then set @test_var=concat(@test_var, 'nine');
			when 10 then set @test_var=concat(@test_var, 'ten');
			when 11 then set @test_var=concat(@test_var, 'eleven');
			when 12 then set @test_var=concat(@test_var, 'twelve');
			when 13 then set @test_var=concat(@test_var, 'thirteen');
			when 14 then set @test_var=concat(@test_var, 'fourteen');
			when 15 then set @test_var=concat(@test_var, 'fifteen');
			ELSE set @test_var=CONCAT(new.f120, '*', new.f144);
		END case;
	END//
	delimiter ;//

	set @test_var='Empty';
	Insert into tb3 (f120, f122, f136, f144)
		values ('a', 'Test 3.5.8.5-case', 5, 7);
	select f120, f122, f136, f144, @test_var
		from tb3 where f122 = 'Test 3.5.8.5-case' order by f120,f136;
	Insert into tb3 (f120, f122, f136, f144)
		values ('b', 'Test 3.5.8.5-case', 71,16);
	select f120, f122, f136, f144, @test_var
		from tb3 where f122 = 'Test 3.5.8.5-case' order by f120,f136;
	Insert into tb3 (f120, f122, f136, f144)
		values ('c', 'Test 3.5.8.5-case', 80,1);
	select f120, f122, f136, f144, @test_var
		from tb3 where f122 = 'Test 3.5.8.5-case' order by f120,f136;
	Insert into tb3 (f120, f122, f136)
		values ('d', 'Test 3.5.8.5-case', 152);
	select f120, f122, f136, f144, @test_var
		from tb3 where f122 = 'Test 3.5.8.5-case' order by f120,f136;
	Insert into tb3 (f120, f122, f136, f144)
		values ('e', 'Test 3.5.8.5-case', 200, 8);
	select f120, f122, f136, f144, @test_var
		from tb3 where f122 = 'Test 3.5.8.5-case' order by f120,f136;
	--error 0, ER_SP_CASE_NOT_FOUND
	Insert into tb3 (f120, f122, f136, f144)
		values ('f', 'Test 3.5.8.5-case', 100, 8);
	select f120, f122, f136, f144, @test_var
		from tb3 where f122 = 'Test 3.5.8.5-case' order by f120,f136;

	delimiter //;
	--error ER_PARSE_ERROR
	create trigger trg3a before update on tb3 for each row
	BEGIN
		CASE
			when new.f136<100 then set new.f120='p';
	END//
	delimiter ;//

	--error 0, ER_TRG_DOES_NOT_EXIST
	drop trigger trg3a;

#Cleanup
	--disable_warnings
	drop trigger trg3;
	delete from tb3 where f121='Test 3.5.8.5-case';	
	--enable_warnings
        SET sql_mode = default;
#Section 3.5.8.5 (LOOP)
# Test case: Ensure that the stored procedure-specific flow control
#            statement like LOOP / LEAVE work correctly when they are
#            part of the triggered action portion of a trigger definition.
let $message= Testcase 3.5.8.5-loop/leave:;
--source include/show_msg.inc
		
	delimiter //;
	Create trigger trg4 after insert on tb3 for each row
	BEGIN
		set @counter=0, @flag='Initial';
		Label1: loop
			if new.f136<new.f144 then
				set @counter='Nothing to loop';
				leave Label1;
			else
				set @counter=@counter+1;
				if new.f136=new.f144+@counter then
					set @counter=concat(@counter, ' loops');
					leave Label1;
				end if;
			end if;
			iterate label1;
			set @flag='Final';
		END loop Label1;
	END//
	delimiter ;//
	Insert into tb3 (f122, f136, f144)
		values ('Test 3.5.8.5-loop', 2, 8);
		select @counter, @flag;
	Insert into tb3 (f122, f136, f144)
		values ('Test 3.5.8.5-loop', 11, 8);
		select @counter, @flag;


	delimiter //;

	--error ER_PARSE_ERROR
	Create trigger trg4_2 after update on tb3 for each row
	BEGIN
		Label1: loop
			set @counter=@counter+1;
		END;
	END//
	delimiter ;//
	--error 0, ER_TRG_DOES_NOT_EXIST
	drop trigger trg4_2;

#Cleanup
	--disable_warnings
	drop trigger trg4;
	delete from tb3 where f122='Test 3.5.8.5-loop';
	--enable_warnings

#Section 3.5.8.5 (REPEAT ITERATE)
#Testcase: Ensure that the stored procedure-specific flow control statements
#          like REPEAT work correctly when they are part of the triggered action
#          portion of a trigger definition.
let $message= Testcase 3.5.8.5-repeat:;
--source include/show_msg.inc
			
	delimiter //;
	Create trigger trg6 after insert on tb3 for each row
	BEGIN
		rp_label: REPEAT
			SET @counter1 = @counter1 + 1;
			IF (@counter1 MOD 2 = 0) THEN ITERATE rp_label; 	
			END IF;
			SET @counter2 = @counter2 + 1;
		UNTIL @counter1> new.f136 END REPEAT rp_label;
	END//
	delimiter ;//

	set @counter1= 0, @counter2= 0;
	Insert into tb3 (f122, f136)
		values ('Test 3.5.8.5-repeat', 13);
	select @counter1, @counter2;

			
	delimiter //;
	--error ER_PARSE_ERROR
	Create trigger trg6_2 after update on tb3 for each row
	BEGIN
		REPEAT
			SET @counter2 = @counter2 + 1;
	END//
	delimiter ;//

#Cleanup			
	--disable_warnings
	drop trigger trg6;
	delete from tb3 where f122='Test 3.5.8.5-repeat';
	--enable_warnings


#Section 3.5.8.5 (WHILE)
# Test case: Ensure that the stored procedure-specific flow control
#            statements WHILE, work correctly when they are part of
#            the triggered action portion of a trigger definition.
let $message= Testcase 3.5.8.5-while:;
--source include/show_msg.inc

	delimiter //;
	Create trigger trg7 after insert on tb3 for each row
		wl_label: WHILE @counter1 < new.f136 DO
			SET @counter1 = @counter1 + 1;
			IF (@counter1 MOD 2 = 0) THEN ITERATE wl_label; 	
			END IF;
			SET @counter2 = @counter2 + 1;
		END WHILE wl_label//
	delimiter ;//

	set @counter1= 0, @counter2= 0;
	Insert into tb3 (f122, f136)
		values ('Test 3.5.8.5-while', 7);
	select @counter1, @counter2;
	delimiter //;
	--error ER_PARSE_ERROR
	Create trigger trg7_2 after update on tb3 for each row
	BEGIN
		WHILE @counter1 < new.f136
			SET @counter1 = @counter1 + 1;
	END//
	delimiter ;//

#Cleanup
	--disable_warnings
	delete from tb3 where f122='Test 3.5.8.5-while';
	drop trigger trg7;
	--enable_warnings

#Section 3.5.8.6
# Test case: Ensure that a trigger definition that includes a CALL to a stored
#            procedure fails, at CREATE TRIGGER time, with an appropriate error
#            message. Not more valid requirement.
let $message= Testcase 3.5.8.6: (requirement void);
--source include/show_msg.inc
	delimiter //;
	CREATE PROCEDURE sp_01 () BEGIN set @v1=1; END//

	CREATE TRIGGER trg8_1 BEFORE UPDATE ON tb3 FOR EACH ROW
	BEGIN
		CALL sp_01 ();
	END//
	delimiter ;//
	Insert into tb3 (f120, f122, f136) values ('6', 'Test 3.5.8.6-insert', 101);
	update tb3 set f120='S', f136=111,
	       f122='Test 3.5.8.6-tr8_1'
	       where f122='Test 3.5.8.6-insert';
	select f120, f122
	       from tb3 where f122  like 'Test 3.5.8.6%' order by f120;
	DROP TRIGGER trg8_1;
	DROP PROCEDURE sp_01;


#Section 3.5.8.7
# Test case: Ensure that a trigger definition that includes a
#            transaction-delimiting statement (e.g. COMMIT,
#            ROLLBACK, START TRANSACTION) fails, at CREATE TRIGGER
#            time, with an appropriate error message.
let $message= Testcase 3.5.8.7;
--source include/show_msg.inc
	
	delimiter //;
	--error ER_COMMIT_NOT_ALLOWED_IN_SF_OR_TRG
	Create trigger trg9_1 before update on tb3 for each row
	BEGIN
		Start transaction;
			Set new.f120='U';
		Commit;
	END//

	--error ER_COMMIT_NOT_ALLOWED_IN_SF_OR_TRG
	Create trigger trg9_2 before delete on tb3 for each row
	BEGIN
		Start transaction;
			Set @var2=old.f120;
		Rollback;
	END//
	delimiter ;//


# Cleanup section 3.5
	connection default;
	drop user test_general@localhost;
	drop user test_general;
	drop user test_super@localhost;

DROP TABLE test.tb3;
