#======================================================================
#
# Trigger Tests
# test cases for TRIGGER privilege on db, table and column level
#======================================================================

--disable_abort_on_error

###########################################
################ Section 3.5.3 ############
# Check for the db level of Triggers      #
###########################################

# General setup to be used in all testcases
let $message= #########      Testcase for table level:   ########;
--source include/show_msg.inc

        --disable_warnings
	drop database if exists priv_db;
	--enable_warnings
	create database priv_db;
	use priv_db;
	eval create table t1 (f1 char(20)) engine= $engine_type;

	create User test_yesprivs@localhost;
	set password for test_yesprivs@localhost = 'PWD';
	revoke ALL PRIVILEGES, GRANT OPTION FROM test_yesprivs@localhost;

        create User test_noprivs@localhost;
        set password for test_noprivs@localhost = 'PWD';
        revoke ALL PRIVILEGES, GRANT OPTION FROM test_noprivs@localhost;

	connect (yes_privs,localhost,test_yesprivs,PWD,test,$MASTER_MYPORT,$MASTER_MYSOCK);

	connect (no_privs,localhost,test_noprivs,PWD,test,$MASTER_MYPORT,$MASTER_MYSOCK);

################ Section 3.5.3 ############
# Check for the table level of Triggers   #
###########################################

# user has no trigger privilege->create trigger fail

let $message= no trigger privilege on table level for create:;
--source include/show_msg.inc
	connection default;
        select current_user;
--replace_column 6 #
	show triggers;
	grant  select, insert, update on priv_db.t1 to test_yesprivs@localhost;
	show grants for test_yesprivs@localhost;
	grant select, update, insert on priv_db.t1 to test_noprivs@localhost;
	show grants for test_noprivs@localhost;

	connection yes_privs;
        select current_user;
	use priv_db;
	show tables;
	--error ER_TABLEACCESS_DENIED_ERROR
	create trigger trg1_1 before INSERT on t1 for each row
		set new.f1 = 'trig 1_1-no';

# no trigger execution, as trigger does'nt exist

	connection no_privs;
	select current_user;
	use priv_db;
	insert into t1 (f1) values ('insert1-yes');
	select f1 from t1 order by f1;
	
	connection default;
        select current_user;
--replace_column 6 #
	show triggers;
	show tables;
	insert into t1 (f1) values ('insert2-yes');
	select f1 from t1 order by f1;
        grant TRIGGER on priv_db.t1 to test_yesprivs@localhost;
        show grants for test_yesprivs@localhost;

# user got trigger privilege->create trigger successful

let $message= trigger privilege on table level for create:;
--source include/show_msg.inc
	connection yes_privs;
        select current_user;
--replace_column 6 #
	show triggers;
	create trigger trg1_2 before INSERT  on t1 for each row
		set new.f1 = 'trig 1_2-yes';

# insert now executes the trigger

	connection no_privs;
	select current_user;
	insert into t1 (f1) values ('insert3-no');
	select f1 from t1 order by f1;

	connection default;
        select current_user;
        insert into t1 (f1) values ('insert4-no');
        select f1 from t1 order by f1;
        revoke TRIGGER on priv_db.t1 from test_yesprivs@localhost;
        show grants for test_yesprivs@localhost;

# revoke triggerprivilege->drop trigger fail

let $message= no trigger privilege on table level for drop:;
--source include/show_msg.inc
	connection yes_privs;
        select current_user;

	--error ER_TABLEACCESS_DENIED_ERROR
	drop trigger trg1_2;

# no trigger priv at activation time->insert fails

let $message= no trigger privilege at activation time:;
--source include/show_msg.inc
	connection no_privs;
	select current_user;
	--error ER_TABLEACCESS_DENIED_ERROR
	insert into t1 (f1) values ('insert5-no');
	select f1 from t1 order by f1;

	connection default;
        select current_user;
        grant TRIGGER on priv_db.t1 to test_yesprivs@localhost;

# trigger privilege at activation time->insert with trigger successful

let $message= trigger privilege at activation time:;
--source include/show_msg.inc
        connection no_privs;
        select current_user;
        insert into t1 (f1) values ('insert6-no');
        select f1 from t1 order by f1;

# trigger privilege->drop trigger successful
let $message= trigger privilege on table level for drop:;
--source include/show_msg.inc
	connection yes_privs;
        select current_user;
        show grants for test_yesprivs@localhost;
        drop trigger trg1_2;

# inserts without trigger

	connection no_privs;
	select current_user;
	insert into t1 (f1) values ('insert7-yes');
	select f1 from t1 order by f1;

	connection default;
        select current_user;
	insert into t1 (f1) values ('insert8-yes');
	select f1 from t1 order by f1;

# trigger privilege must be keep when mixinf tables with and without
# trigger privilege

let $message= switch to table without having trigger priv for it:;
--source include/show_msg.inc
	eval create table t2 (f1 char(20)) engine= $engine_type;
# Adding the minimal priv to be able to set to the db
        grant SELECT, INSERT, UPDATE on priv_db.t2 to test_yesprivs@localhost;
        show grants for test_yesprivs@localhost;

        grant SELECT, INSERT, UPDATE on priv_db.t2 to test_noprivs@localhost;
        show grants for test_noprivs@localhost;

let $message= use table with trigger privilege and without...:;
--source include/show_msg.inc
        connection yes_privs;
        select current_user;
	--error ER_TABLEACCESS_DENIED_ERROR
        create trigger trg2_1 before INSERT  on t2 for each row
                set new.f1 = 'trig 2_1-no';
        create trigger trg1_3 before INSERT  on t1 for each row
                set new.f1 = 'trig 1_3-yes';
        --error ER_TABLEACCESS_DENIED_ERROR
        create trigger trg2_2 before UPDATE  on t2 for each row
                set new.f1 = 'trig 2_2-no';
        create trigger trg1_4 before UPDATE  on t1 for each row
                set new.f1 = 'trig 1_4-yes';
--replace_column 6 #
	show triggers;
        connection no_privs;
        select current_user;
        insert into t2 (f1) values ('insert9-yes');
        select f1 from t2 order by f1;
        insert into t1 (f1) values ('insert10-no');
        select f1 from t1 order by f1;
	disconnect no_privs;

        connection yes_privs;
        select current_user;
	--error ER_TRG_DOES_NOT_EXIST
        drop trigger trg2_1;
        drop trigger trg1_3;
        --error ER_TRG_DOES_NOT_EXIST
        drop trigger trg2_2;
        drop trigger trg1_4;


# Cleanup table level
	--disable_warnings
        disconnect yes_privs;

	connection default;
        select current_user;
	--enable_warnings


# general Cleanup
	--disable_warnings
	drop database if exists priv_db;
	drop user test_yesprivs@localhost;
	drop user test_noprivs@localhost;
	--enable_warnings

