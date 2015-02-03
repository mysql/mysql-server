#======================================================================
#
# Trigger Tests
# test cases for TRIGGER privilege on db, table and column level
#======================================================================

--disable_abort_on_error

###########################################
################ Section 3.5.3 ############
# Check for Triggers in transactions      #
###########################################

# General setup to be used in all testcases
let $message= #########      Testcase for transactions:   ########;
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

	connect (yes_privs,localhost,test_yesprivs,PWD,test,$MASTER_MYPORT,$MASTER_MYSOCK);

	connection default;
        select current_user;
        grant  select, insert, update ,trigger
	        on priv_db.t1 to test_yesprivs@localhost;
	show grants for test_yesprivs@localhost;

	connection yes_privs;
        select current_user;
	use priv_db;
	set autocommit=0;
	create definer=current_user trigger trg1_1
		before INSERT on t1 for each row
		set new.f1 = 'trig 1_1-yes';
	rollback work;
	insert into t1 (f1) values ('insert-no');
	select f1 from t1 order by f1;

        create definer=test_yesprivs@localhost trigger trg1_2
                before UPDATE on t1 for each row
                set new.f1 = 'trig 1_2-yes';
	commit work;
        update t1 set f1 = 'update-yes' where f1 like '%trig%';
        select f1 from t1 order by f1;
	commit work;
	drop trigger trg1_1;
	rollback work;
	--error ER_TRG_DOES_NOT_EXIST
	drop trigger trg1_1;
	drop trigger trg1_2;
	commit work;
	set autocommit=1;

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
	--enable_warnings

