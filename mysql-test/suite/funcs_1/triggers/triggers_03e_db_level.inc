#======================================================================
#
# Trigger Tests
# test cases for TRIGGER privilege on db, table and column level
#======================================================================

--disable_abort_on_error

############################################
################ Section 3.5.3 #############
# basic tests for the db level of Triggers #
############################################

# General setup to be used in all testcases
let $message= Testcase for db level:;
--source include/show_msg.inc

        --disable_warnings
	drop database if exists priv_db;
	drop database if exists no_priv_db;
	--enable_warnings
	create database priv_db;
        create database no_priv_db;
	use priv_db;
	eval create table t1 (f1 char(20)) engine= $engine_type;

	create User test_yesprivs@localhost;
	set password for test_yesprivs@localhost = password('PWD');
	revoke ALL PRIVILEGES, GRANT OPTION FROM test_yesprivs@localhost;
	grant select on priv_db.* to test_yesprivs@localhost;
	show grants for test_yesprivs@localhost;

        create User test_noprivs@localhost;
        set password for test_noprivs@localhost = password('PWD');
        revoke ALL PRIVILEGES, GRANT OPTION FROM test_noprivs@localhost;
        grant select,insert on priv_db.* to test_noprivs@localhost;
        show grants for test_noprivs@localhost;

# no trigger privilege->create trigger must fail:
	connect (yes_privs,localhost,test_yesprivs,PWD,test,$MASTER_MYPORT,$MASTER_MYSOCK);
let $message= no trigger privilege on db level for create:;
--source include/show_msg.inc
	use priv_db;
	--error ER_TABLEACCESS_DENIED_ERROR
	create trigger trg1_1 before INSERT on t1 for each row
		set new.f1 = 'trig 1_1-no';

# user with minimum privs on t1->no trigger executed;
	connect (no_privs,localhost,test_noprivs,PWD,test,$MASTER_MYPORT,$MASTER_MYSOCK);
	use priv_db;
	insert into t1 (f1) values ('insert-yes');
	select f1 from t1 order by f1;
	
	connection default;
        select current_user;
        grant TRIGGER on priv_db.* to test_yesprivs@localhost;
        show grants for test_yesprivs@localhost;

# user got trigger privilege->create successful:
let $message= trigger privilege on db level for create:;
--source include/show_msg.inc
	connection yes_privs;
        select current_user;
	use priv_db;
	create trigger trg1_2 before INSERT  on t1 for each row
		set new.f1 = 'trig 1_2-yes';

# user with minimum privs on t1->fail,as trigger definer no update priv:;
        connection no_privs;
	select current_user;
        use priv_db;
        insert into t1 (f1) values ('insert-yes');
        select f1 from t1 order by f1;

	connection default;
        select current_user;
        grant UPDATE on priv_db.* to test_yesprivs@localhost;
# succeed,as trigger definer has update privilege:
# new privilege take effect after 'use db':
        use priv_db;
        insert into t1 (f1) values ('insert-no');
        select f1 from t1 order by f1;

# succeed:
        connection no_privs;
	select current_user;
        use priv_db;
        insert into t1 (f1) values ('insert-yes');
        select f1 from t1 order by f1;

        connection default;
        select current_user;
        revoke TRIGGER on priv_db.* from test_yesprivs@localhost;
        show grants for test_yesprivs@localhost;

# drop must fail, as no trigger privilege:
let $message= no trigger privilege on db level for drop:;
--source include/show_msg.inc
	connection yes_privs;
        select current_user;
	use priv_db;
	--error ER_TABLEACCESS_DENIED_ERROR
	drop trigger trg1_2;

	connection no_privs;
        select current_user;
        use priv_db;
# no trigger privilege at activation time:
let $message= no trigger privilege at activation time:;
--source include/show_msg.inc
        --error ER_TABLEACCESS_DENIED_ERROR
        insert into t1 (f1) values ('insert-yes');
        select f1 from t1 order by f1;

let $message= trigger privilege at activation time:;
--source include/show_msg.inc
	connection default;
	select current_user;
        grant TRIGGER on priv_db.* to test_yesprivs@localhost;

# succeed, as trigger privilege at activation time:
	connection no_privs;
	select current_user;
	use priv_db;
	insert into t1 (f1) values ('insert-no');
	select f1 from t1 order by f1;
# drop must fail, as no 'use db' executed:
let $message= trigger privilege on db level for drop:;
--source include/show_msg.inc
	connection yes_privs;
        select current_user;
        show grants for test_yesprivs@localhost;
        --error ER_TABLEACCESS_DENIED_ERROR
 	drop trigger trg1_2;

# succeed
let $message= takes effect after use priv_db:;
--source include/show_msg.inc
        use priv_db;
        drop trigger trg1_2;

	connection default;
        select current_user;
	use priv_db;
	insert into t1 (f1) values ('insert-yes');
	select f1 from t1 order by f1;

let $message= switch to db without having trigger priv for it:;
--source include/show_msg.inc
        use no_priv_db;
	eval create table t1 (f1 char(20)) engine= $engine_type;
# Adding the minimal priv to be able to set to the db
        grant SELECT,UPDATE on no_priv_db.* to test_yesprivs@localhost;
        show grants for test_yesprivs@localhost;

# trigger privilege is hold over changes between priv and no priv db:
let $message= use db with trigger privilege on db level and without...:;
--source include/show_msg.inc
        connection yes_privs;
        select current_user;
        use no_priv_db;
	--error ER_TABLEACCESS_DENIED_ERROR
        create trigger trg1_3 before INSERT  on t1 for each row
                set new.f1 = 'trig 1_3-no';
        use priv_db;
        create trigger trg1_3 before INSERT  on t1 for each row
                set new.f1 = 'trig 1_3-yes';
        use no_priv_db;
        --error ER_TABLEACCESS_DENIED_ERROR
        create trigger trg1_4 before UPDATE  on t1 for each row
                set new.f1 = 'trig 1_4-no';
        use priv_db;
        create trigger trg1_4 before UPDATE  on t1 for each row
                set new.f1 = 'trig 1_4-yes';

        connection no_privs;
        select current_user;
        use no_priv_db;
        insert into t1 (f1) values ('insert-yes');
        select f1 from t1 order by f1;
        use priv_db;
        insert into t1 (f1) values ('insert-no');
        select f1 from t1 order by f1;
	--disable_warnings
	disconnect no_privs;
	--enable_warnings

        connection yes_privs;
        select current_user;
        use no_priv_db;
	--error ER_TRG_DOES_NOT_EXIST
        drop trigger trg1_3;
        use priv_db;
        drop trigger trg1_3;
        use no_priv_db;
        --error ER_TRG_DOES_NOT_EXIST
        drop trigger trg1_4;
        use priv_db;
        drop trigger trg1_4;


# Cleanup db level
	--disable_warnings
	disconnect yes_privs;

	connection default;
        select current_user;
	drop table priv_db.t1;
	drop table no_priv_db.t1;
	--enable_warnings

# general Cleanup
	--disable_warnings
	drop database if exists priv_db;
        drop database if exists no_priv_db;
	drop user test_yesprivs@localhost;
	drop user test_noprivs@localhost;
	--enable_warnings

