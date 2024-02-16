#======================================================================
#
# Trigger Tests
# test cases for TRIGGER privilege on db, table and column level
#======================================================================

--disable_abort_on_error

#########################################################
################ Section 3.5.3 ##########################
# Check for mix of user and db level of Triggers      #
#########################################################

# General setup to be used in all testcases
let $message= #### Testcase for mix of user(global) and db level: ####;
--source include/show_msg.inc

        --disable_warnings
	drop database if exists priv_db;
	drop database if exists no_priv_db;
	--enable_warnings
	create database priv_db;
	create database no_priv_db;
	use priv_db;
	eval create table t1 (f1 char(20)) engine= $engine_type;
	use no_priv_db;
        eval create table t1 (f1 char(20)) engine= $engine_type;

	create User test_yesprivs@localhost;
	set password for test_yesprivs@localhost = 'PWD';
	revoke ALL PRIVILEGES, GRANT OPTION FROM test_yesprivs@localhost;
	grant ALL  on *.* to test_yesprivs@localhost;
	--let $user = test_yesprivs@localhost
	--source include/show_grants.inc

        create User test_noprivs@localhost;
        set password for test_noprivs@localhost = 'PWD';
        revoke ALL PRIVILEGES, GRANT OPTION FROM test_noprivs@localhost;
        grant SELECT,INSERT  on *.* to test_noprivs@localhost;
        show grants for test_noprivs@localhost;

	connect (yes_privs,localhost,test_yesprivs,PWD,test,$MASTER_MYPORT,$MASTER_MYSOCK);

	connect (no_privs,localhost,test_noprivs,PWD,test,$MASTER_MYPORT,$MASTER_MYSOCK);

	connection yes_privs;
	select current_user;
let $message= trigger privilege on user level for create:;
--source include/show_msg.inc
	use priv_db;

	create trigger trg1_1 before INSERT  on t1 for each row
		set new.f1 = 'trig 1_1-yes';
        insert into t1 (f1) values ('insert-no');
        select f1 from t1 order by f1;

	use no_priv_db;
        create trigger priv_db.trg1_5 before UPDATE  on priv_db.t1
		for each row
                set new.f1 = 'trig 1_5-yes';
        insert into priv_db.t1 (f1) values ('insert-no');
        select f1 from priv_db.t1 order by f1;
	drop trigger priv_db.trg1_5;

        connection no_privs;
        select current_user;
        use priv_db;
        insert into t1 (f1) values ('insert-no');
        select f1 from t1 order by f1;

	connection default;
        select current_user;
	use priv_db;
        insert into t1 (f1) values ('insert-no');
        select f1 from t1 order by f1;

        revoke TRIGGER on *.* from test_yesprivs@localhost;
	--let $user = test_yesprivs@localhost
	--source include/show_grants.inc

# change of privilege only active after reconnecting the session

	--disable_warnings
        disconnect yes_privs;
	--enable_warnings
        connect (yes_privs,localhost,test_yesprivs,PWD,test,$MASTER_MYPORT,$MASTER_MYSOCK);
	select current_user;
        use priv_db;
--replace_column 6 #
        show triggers;
--replace_column 17 #
	select * from information_schema.triggers;
	--error ER_TABLEACCESS_DENIED_ERROR
        drop trigger trg1_1;

        connection default;
	select current_user;
	--let $user = 0
	--source include/show_grants.inc
        drop trigger trg1_1;
	use priv_db;

################ Section 3.5.3 ############
# Check for the db level of Triggers   #
###########################################
let $message= no trigger privilege on db level for create:;
--source include/show_msg.inc

	connection yes_privs;
        select current_user;
	--error ER_TABLEACCESS_DENIED_ERROR
	create trigger trg1_1 before INSERT on t1 for each row
		set new.f1 = 'trig 1_1-no';

        connection no_privs;
        select current_user;
        use priv_db;
        insert into t1 (f1) values ('insert-yes');
        select f1 from t1 order by f1;

	connection default;
        select current_user;
        grant TRIGGER on priv_db.* to test_yesprivs@localhost;
	--let $user = test_yesprivs@localhost
	--source include/show_grants.inc

let $message= trigger privilege on db level for create:;
--source include/show_msg.inc
	connection yes_privs;
        select current_user;

# active after 'use db'

	use priv_db;
	create trigger trg1_2 before INSERT  on t1 for each row
		set new.f1 = 'trig 1_2-yes';
        --error ER_TABLEACCESS_DENIED_ERROR
	create trigger no_priv_db.trg1_9 before insert on no_priv_db.t1
		for each row
		set new.f1 = 'trig 1_9-yes';
        use no_priv_db;
	--error ER_TABLEACCESS_DENIED_ERROR
        create trigger trg1_2 before INSERT  on t1 for each row
                set new.f1 = 'trig 1_2-no';
        create trigger priv_db.trg1_9 before UPDATE on priv_db.t1
                for each row
                set new.f1 = 'trig 1_9-yes';

	connection no_privs;
        select current_user;
	use priv_db;
        insert into t1 (f1) values ('insert-yes');
        select f1 from t1 order by f1;
	use no_priv_db;
        insert into t1 (f1) values ('insert-yes');
        select f1 from t1 order by f1;
	--error ER_TABLEACCESS_DENIED_ERROR
	drop trigger priv_db.trg1_9;

        connection default;
        select current_user;
	drop trigger priv_db.trg1_9;
 	revoke TRIGGER on priv_db.* from test_yesprivs@localhost;
	use priv_db;
	--error ER_TABLEACCESS_DENIED_ERROR
	insert into t1 (f1) values ('insert-yes');
	select f1 from t1 order by f1;
        grant TRIGGER on *.* to test_yesprivs@localhost;
	--let $user = test_yesprivs@localhost
	--source include/show_grants.inc

	connection yes_privs;
        select current_user;
	use no_priv_db;
	--error ER_TABLEACCESS_DENIED_ERROR
        create trigger trg1_2 before INSERT  on t1 for each row
                set new.f1 = 'trig 1_2-no';

        connection no_privs;
        select current_user;
	use priv_db;
        insert into t1 (f1) values ('insert-no');
        select f1 from t1 order by f1;
	use no_priv_db;
        insert into t1 (f1) values ('insert-yes');
        select f1 from t1 order by f1;

	--disable_warnings
        disconnect yes_privs;
	--enable_warnings
        connect (yes_privs,localhost,test_yesprivs,PWD,test,$MASTER_MYPORT,$MASTER_MYSOCK);
        select current_user;
	use no_priv_db;
        create trigger trg1_2 before INSERT  on t1 for each row
                set new.f1 = 'trig 1_2-yes';
        --disable_warnings
        disconnect yes_privs;
	--enable_warnings

        connection no_privs;
        select current_user;
	use priv_db;
        insert into t1 (f1) values ('insert-no');
        select f1 from t1 order by f1;
	use no_priv_db;
        insert into t1 (f1) values ('insert-no');
        select f1 from t1 order by f1;
	--disable_warnings
	disconnect no_privs;

# Cleanup table level
	connection default;
        select current_user;

# general Cleanup
	drop database if exists priv_db;
	drop database if exists no_priv_db;
        drop database if exists h1;
	drop user test_yesprivs@localhost;
        drop user test_noprivs@localhost;
 	--enable_warnings

