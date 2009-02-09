#======================================================================
#
# Trigger Tests
# (test case numbering refer to requirement document TP v1.1)
#======================================================================

USE test;
--source suite/funcs_1/include/tb3.inc

--replace_result $MYSQLTEST_VARDIR <MYSQLTEST_VARDIR>
eval
load data infile '$MYSQLTEST_VARDIR/std_data/funcs_1/memory_tb3.txt'
into table tb3;

#################################
####### Section 3.5.9 ###########
#   Checks on old and new rows  #
#################################

#Section 3.5.9.1
#Test case: Ensure that every trigger executes its triggered action on each row
#           that meets the conditions stated in the trigger definition.
#Section 3.5.9.2
#Testcase: Ensure that a trigger never executes its triggered action on any row
#          that doesn't meet the conditions stated in the trigger definition.
let $message= Testcase 3.5.9.1/2:;
--source include/show_msg.inc

	Create trigger trg1 BEFORE UPDATE on tb3 for each row
		set new.f142 = 94087, @counter=@counter+1;
--disable_query_log
	select count(*) as TotalRows from tb3;
	select count(*) as Affected from tb3 where f130<100;
	select count(*) as NotAffected from tb3 where f130>=100;
	select count(*) as NewValuew from tb3 where f142=94087;
--enable_query_log
	set @counter=0;
	Update tb3 Set f142='1' where f130<100;
	select count(*) as ExpectedChanged, @counter as TrigCounter
		from tb3 where f142=94087;
	select count(*) as ExpectedNotChange from tb3
		where f130<100 and f142<>94087;
	select count(*) as NonExpectedChanged from tb3
		where f130>=130 and f142=94087;

#Cleanup
	--disable_warnings
	drop trigger trg1;
	--enable_warnings


#Section 3.5.9.3
#Test case: Ensure that a reference to OLD.<column name> always correctly refers
#           to the values of the specified column of the subject table before a
#           data row is updated or deleted.
let $message= Testcase 3.5.9.3:;
--source include/show_msg.inc
	
	Create trigger trg2_a before update on tb3 for each row
		set @tr_var_b4_118=old.f118, @tr_var_b4_121=old.f121,
		    @tr_var_b4_122=old.f122, @tr_var_b4_136=old.f136,
		    @tr_var_b4_163=old.f163;

	Create trigger trg2_b after update on tb3 for each row
		set @tr_var_af_118=old.f118, @tr_var_af_121=old.f121,
		    @tr_var_af_122=old.f122, @tr_var_af_136=old.f136,
		    @tr_var_af_163=old.f163;

	Create trigger trg2_c before delete on tb3 for each row
		set @tr_var_b4_118=old.f118, @tr_var_b4_121=old.f121,
		    @tr_var_b4_122=old.f122, @tr_var_b4_136=old.f136,
		    @tr_var_b4_163=old.f163;

	Create trigger trg2_d after delete on tb3 for each row
		set @tr_var_af_118=old.f118, @tr_var_af_121=old.f121,
		    @tr_var_af_122=old.f122, @tr_var_af_136=old.f136,
		    @tr_var_af_163=old.f163;


--disable_query_log
	set @tr_var_b4_118=0, @tr_var_b4_121=0, @tr_var_b4_122=0,
		@tr_var_b4_136=0, @tr_var_b4_163=0;
	set @tr_var_af_118=0, @tr_var_af_121=0, @tr_var_af_122=0,
		@tr_var_af_136=0, @tr_var_af_163=0;
	select  @tr_var_b4_118, @tr_var_b4_121, @tr_var_b4_122,
		@tr_var_b4_136, @tr_var_b4_163;
	select  @tr_var_af_118, @tr_var_af_121, @tr_var_af_122,
		@tr_var_af_136, @tr_var_af_163;
--enable_query_log

	Insert into tb3 (f122, f136, f163)
		values ('Test 3.5.9.3', 7, 123.17);
	Update tb3 Set f136=8 where f122='Test 3.5.9.3';

	select f118, f121, f122, f136, f163 from tb3 where f122='Test 3.5.9.3' order by f136;
	select  @tr_var_b4_118, @tr_var_b4_121, @tr_var_b4_122,
		@tr_var_b4_136, @tr_var_b4_163;
	select  @tr_var_af_118, @tr_var_af_121, @tr_var_af_122,
		@tr_var_af_136, @tr_var_af_163;

--disable_query_log
	set @tr_var_b4_118=0, @tr_var_b4_121=0, @tr_var_b4_122=0,
		@tr_var_b4_136=0, @tr_var_b4_163=0;
	set @tr_var_af_118=0, @tr_var_af_121=0, @tr_var_af_122=0,
		@tr_var_af_136=0, @tr_var_af_163=0;
	select  @tr_var_b4_118, @tr_var_b4_121, @tr_var_b4_122,
		@tr_var_b4_136, @tr_var_b4_163;
	select  @tr_var_af_118, @tr_var_af_121, @tr_var_af_122,
		@tr_var_af_136, @tr_var_af_163;
--enable_query_log

	delete from tb3 where f122='Test 3.5.9.3';

	select f118, f121, f122, f136, f163 from tb3 where f122='Test 3.5.9.3' order by f136;
	select  @tr_var_b4_118, @tr_var_b4_121, @tr_var_b4_122,
		@tr_var_b4_136, @tr_var_b4_163;
	select  @tr_var_af_118, @tr_var_af_121, @tr_var_af_122,
		@tr_var_af_136, @tr_var_af_163;
#Cleanup
	--disable_warnings
	drop trigger trg2_a;
	drop trigger trg2_b;
	drop trigger trg2_c;
	drop trigger trg2_d;
	--enable_warnings

#Section 3.5.9.4
#Test case: Ensure that a reference to NEW.<column name> always correctly refers
#           to the values of the specified column of the subject table after an
#           existing data row has been updated or a new data row has been inserted.
let $message= Testcase 3.5.9.4:;
--source include/show_msg.inc
	
	Create trigger trg3_a before insert on tb3 for each row
		set @tr_var_b4_118=new.f118, @tr_var_b4_121=new.f121,
		    @tr_var_b4_122=new.f122, @tr_var_b4_136=new.f136,
		    @tr_var_b4_151=new.f151, @tr_var_b4_163=new.f163;

	Create trigger trg3_b after insert on tb3 for each row
		set @tr_var_af_118=new.f118, @tr_var_af_121=new.f121,
		    @tr_var_af_122=new.f122, @tr_var_af_136=new.f136,
		    @tr_var_af_151=new.f151, @tr_var_af_163=new.f163;

	Create trigger trg3_c before update on tb3 for each row
		set @tr_var_b4_118=new.f118, @tr_var_b4_121=new.f121,
		    @tr_var_b4_122=new.f122, @tr_var_b4_136=new.f136,
		    @tr_var_b4_151=new.f151, @tr_var_b4_163=new.f163;

	Create trigger trg3_d after update on tb3 for each row
		set @tr_var_af_118=new.f118, @tr_var_af_121=new.f121,
		    @tr_var_af_122=new.f122, @tr_var_af_136=new.f136,
		    @tr_var_af_151=new.f151, @tr_var_af_163=new.f163;

--disable_query_log
	set @tr_var_b4_118=0, @tr_var_b4_121=0, @tr_var_b4_122=0,
		@tr_var_b4_136=0, @tr_var_b4_151=0, @tr_var_b4_163=0;
	set @tr_var_af_118=0, @tr_var_af_121=0, @tr_var_af_122=0,
		@tr_var_af_136=0, @tr_var_af_151=0, @tr_var_af_163=0;
	select  @tr_var_b4_118, @tr_var_b4_121, @tr_var_b4_122,
		@tr_var_b4_136, @tr_var_b4_151, @tr_var_b4_163;
	select  @tr_var_af_118, @tr_var_af_121, @tr_var_af_122,
		@tr_var_af_136, @tr_var_af_151, @tr_var_af_163;
--enable_query_log

	Insert into tb3 (f122, f136, f151, f163)
		values ('Test 3.5.9.4', 7, DEFAULT, 995.24);

	select f118, f121, f122, f136, f151, f163 from tb3
		where f122 like 'Test 3.5.9.4%' order by f163;
	select  @tr_var_b4_118, @tr_var_b4_121, @tr_var_b4_122,
		@tr_var_b4_136, @tr_var_b4_151, @tr_var_b4_163;
	select  @tr_var_af_118, @tr_var_af_121, @tr_var_af_122,
		@tr_var_af_136, @tr_var_af_151, @tr_var_af_163;

--disable_query_log
	set @tr_var_b4_118=0, @tr_var_b4_121=0, @tr_var_b4_122=0,
		@tr_var_b4_136=0, @tr_var_b4_151=0, @tr_var_b4_163=0;
	set @tr_var_af_118=0, @tr_var_af_121=0, @tr_var_af_122=0,
		@tr_var_af_136=0, @tr_var_af_151=0, @tr_var_af_163=0;
	select  @tr_var_b4_118, @tr_var_b4_121, @tr_var_b4_122,
		@tr_var_b4_136, @tr_var_b4_151, @tr_var_b4_163;
	select  @tr_var_af_118, @tr_var_af_121, @tr_var_af_122,
		@tr_var_af_136, @tr_var_af_151, @tr_var_af_163;
--enable_query_log

	Update tb3 Set f122='Test 3.5.9.4-trig', f136=NULL, f151=DEFAULT, f163=NULL
		 where f122='Test 3.5.9.4';

        Update tb3 Set f122='Test 3.5.9.4-trig', f136=0, f151=DEFAULT, f163=NULL
                 where f122='Test 3.5.9.4';

	select f118, f121, f122, f136, f151, f163 from tb3
		where f122 like 'Test 3.5.9.4-trig' order by f163;
	select  @tr_var_b4_118, @tr_var_b4_121, @tr_var_b4_122,
		@tr_var_b4_136, @tr_var_b4_151, @tr_var_b4_163;
	select  @tr_var_af_118, @tr_var_af_121, @tr_var_af_122,
		@tr_var_af_136, @tr_var_af_151, @tr_var_af_163;
#Cleanup
	--disable_warnings
	drop trigger trg3_a;
	drop trigger trg3_b;
	drop trigger trg3_c;
	drop trigger trg3_d;
	delete from tb3 where f122='Test 3.5.9.4-trig';
	--enable_warnings


#Section 3.5.9.5
# Test case: Ensure that the definition of an INSERT trigger can include a
#            reference to NEW. <Column name>.
let $message= Testcase 3.5.9.5: (implied in previous tests);
--source include/show_msg.inc
	
#Section 3.5.9.6
# Test case: Ensure that the definition of an INSERT trigger cannot include
#            a reference to OLD. <Column name>.
let $message= Testcase 3.5.9.6:;
--source include/show_msg.inc
	
	--error ER_TRG_NO_SUCH_ROW_IN_TRG
	create trigger trg4a before insert on tb3 for each row
		set @temp1= old.f120;
	--error ER_TRG_CANT_CHANGE_ROW
	create trigger trg4b after insert on tb3 for each row
		set old.f120= 'test';

#Cleanup
	--disable_warnings
	--error 0, ER_TRG_DOES_NOT_EXIST
	drop trigger trg4a;
	--error 0, ER_TRG_DOES_NOT_EXIST
	drop trigger trg4b;
	--enable_warnings


#Section 3.5.9.7
# Test case: Ensure that the definition of an UPDATE trigger can include a
#            reference to NEW. <Column name>.
let $message= Testcase 3.5.9.7: (implied in previous tests);
--source include/show_msg.inc
	
#Section 3.5.9.8
# Test case: Ensure that the definition of an UPDATE trigger cannot include a
#            reference to OLD. <Column name>.
let $message= Testcase 3.5.9.8: (implied in previous tests);
--source include/show_msg.inc
	
#Section 3.5.9.9
# Test case: Ensure that the definition of a DELETE trigger cannot include a
#            reference to NEW.<column name>.
let $message= Testcase 3.5.9.9:;
--source include/show_msg.inc
	
	--error ER_TRG_NO_SUCH_ROW_IN_TRG
	create trigger trg5a before DELETE on tb3 for each row
		set @temp1=new.f122;
	--error ER_TRG_NO_SUCH_ROW_IN_TRG
	create trigger trg5b after DELETE on tb3 for each row
		set new.f122='test';

#Cleanup
	--disable_warnings
	--error 0, ER_TRG_DOES_NOT_EXIST
	drop trigger trg5a;
	--error 0, ER_TRG_DOES_NOT_EXIST
	drop trigger trg5b;
	--enable_warnings


#Section 3.5.9.10
# Test case: Ensure that the definition of a DELETE trigger can include a reference
#            to OLD.<column name>.
let $message= Testcase 3.5.9.10: (implied in previous tests);
--source include/show_msg.inc
	

#Section 3.5.9.11
# Testcase: Ensure that trigger definition that includes a referance to
#           NEW.<colunm name> fails with an appropriate error message,
#           at CREATE TRIGGER time, if the trigger event in not INSERT or UPDATE
let $message= Testcase 3.5.9.11: covered by 3.5.9.9;
--source include/show_msg.inc


#Section 3.5.9.12
# Testcase: Ensure that trigger definition that includes a referance to
#           OLD.<column name> fails with an appropriate error message, at
#           CREATE TRIGGER time, if the trigger event is not DELETE or UPDATE
let $message= Testcase 3.5.9.12: covered by 3.5.9.6;
--source include/show_msg.inc


#Section 3.5.9.13
# Test case: Ensure that all references to OLD. <Column name> are read-only,
#            that is, that they cannot be used to modify a data row.
let $message= Testcase 3.5.9.13:;
--source include/show_msg.inc
	
	--error ER_TRG_CANT_CHANGE_ROW
	create trigger trg6a before UPDATE on tb3 for each row
		set old.f118='C', new.f118='U';
	--error ER_TRG_CANT_CHANGE_ROW
	create trigger trg6b after INSERT on tb3 for each row
		set old.f136=163, new.f118='U';
	--error ER_TRG_CANT_CHANGE_ROW
	create trigger trg6c after UPDATE on tb3 for each row
		set old.f136=NULL;

#Cleanup
	--disable_warnings
	--error 0, ER_TRG_DOES_NOT_EXIST
	drop trigger trg6a;
	--error 0, ER_TRG_DOES_NOT_EXIST
	drop trigger trg6b;
	--error 0, ER_TRG_DOES_NOT_EXIST
	drop trigger trg6c;
	--enable_warnings


#Section 3.5.9.14
# Test case: Ensure that all references to NEW. <Column name> may be used both to
#            read a data row and to modify a data row
let $message= Testcase 3.5.9.14: (implied in previous tests);
--source include/show_msg.inc

DROP TABLE test.tb3;
