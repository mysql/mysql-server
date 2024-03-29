#======================================================================
#
# Trigger Tests
# test cases for TRIGGER privilege on db, table and column level
# These tests ensure that at activation time (execute statement)
# the user must have trigger privilege.
#======================================================================

--disable_abort_on_error

###########################################################
################ Section 3.5.3 ############################
# Check for the trigger privilege in case of prepare/exec #
###########################################################

# General setup to be used in all testcases
let $message= #### Testcase for trigger privilege on execution time ########;
--source include/show_msg.inc

        --disable_warnings
	drop database if exists priv_db;
	--enable_warnings
	create database priv_db;
	use priv_db;
	eval create table t1 (f1 char(20)) engine= $engine_type;

	create User test_yesprivs@localhost;
	set password for test_yesprivs@localhost = 'PWD';
        create User test_useprivs@localhost;
        set password for test_useprivs@localhost = 'PWD';

	revoke ALL PRIVILEGES, GRANT OPTION FROM test_yesprivs@localhost;
        revoke ALL PRIVILEGES, GRANT OPTION FROM test_useprivs@localhost;

	connect (yes_privs,localhost,test_yesprivs,PWD,test,$MASTER_MYPORT,$MASTER_MYSOCK);

	connection default;
        select current_user;
--replace_column 6 #
	show triggers;
	grant  select, insert, update ,trigger
	        on priv_db.t1 to test_yesprivs@localhost
		with grant option;
	grant  select
                on priv_db.t1 to test_useprivs@localhost;
 	show grants for test_yesprivs@localhost;

	connection yes_privs;
        select current_user;
	use priv_db;
	create trigger trg1_1 before INSERT on t1 for each row
		set new.f1 = 'trig 1_1-yes';
	grant insert on t1 to test_useprivs@localhost;
	prepare ins1 from 'insert into t1 (f1) values (''insert1-no'')';
	execute ins1;
	select f1 from t1 order by f1;
        prepare ins1 from 'insert into t1 (f1) values (''insert2-no'')';

        connect (use_privs,localhost,test_useprivs,PWD,test,$MASTER_MYPORT,$MASTER_MYSOCK);
	select current_user;
	use priv_db;
        prepare ins1 from 'insert into t1 (f1) values (''insert3-no'')';
	execute ins1;
	select f1 from t1 order by f1;

        connection default;
        select current_user;
        revoke TRIGGER on priv_db.t1 from test_yesprivs@localhost;
        show grants for test_yesprivs@localhost;

        connection yes_privs;
        select current_user;
	--error ER_TABLEACCESS_DENIED_ERROR
        execute ins1;
        select f1 from t1 order by f1;
        prepare ins1 from 'insert into t1 (f1) values (''insert4-no'')';

        connection use_privs;
        select current_user;
        prepare ins1 from 'insert into t1 (f1) values (''insert5-no'')';
	--error ER_TABLEACCESS_DENIED_ERROR
        execute ins1;
        select f1 from t1 order by f1;

        connection default;
        select current_user;
        grant TRIGGER on priv_db.t1 to test_yesprivs@localhost;
        show grants for test_yesprivs@localhost;

        connection yes_privs;
        select current_user;
        execute ins1;
        select f1 from t1 order by f1;
        prepare ins1 from 'insert into t1 (f1) values (''insert6-no'')';

        connection use_privs;
        select current_user;
        execute ins1;
        select f1 from t1 order by f1;
        prepare ins1 from 'insert into t1 (f1) values (''insert7-no'')';

        connection default;
        select current_user;
        revoke TRIGGER on priv_db.t1 from test_yesprivs@localhost;
        show grants for test_yesprivs@localhost;

        connection yes_privs;
        select current_user;
        --error ER_TABLEACCESS_DENIED_ERROR
        execute ins1;
        select f1 from t1 order by f1;

        connection use_privs;
        select current_user;
        --error ER_TABLEACCESS_DENIED_ERROR
        execute ins1;
        select f1 from t1 order by f1;

        connection default;
        select current_user;
        grant TRIGGER on priv_db.t1 to test_yesprivs@localhost;
        show grants for test_yesprivs@localhost;

        connection yes_privs;
        select current_user;
        execute ins1;
        select f1 from t1 order by f1;

        connection use_privs;
        select current_user;
        execute ins1;
        select f1 from t1 order by f1;

        connection default;
        select current_user;
        revoke TRIGGER on priv_db.t1 from test_yesprivs@localhost;
        show grants for test_yesprivs@localhost;

        connection yes_privs;
        select current_user;
        execute ins1;
        select f1 from t1 order by f1;
        deallocate prepare ins1;

        connection use_privs;
        select current_user;
        execute ins1;
        select f1 from t1 order by f1;
        deallocate prepare ins1;

        connection default;
        select current_user;
        grant TRIGGER on priv_db.t1 to test_yesprivs@localhost;
        show grants for test_yesprivs@localhost;

        connection yes_privs;
        select current_user;
	drop trigger trg1_1;

        connection default;
        select current_user;

# Cleanup prepare
	--disable_warnings
        disconnect yes_privs;

	connection default;
        select current_user;
	--enable_warnings


# general Cleanup
	--disable_warnings
	drop database if exists priv_db;
	drop user test_yesprivs@localhost;
	drop user test_useprivs@localhost;
	--enable_warnings

