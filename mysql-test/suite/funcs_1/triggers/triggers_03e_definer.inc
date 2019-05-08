#======================================================================
#
# Trigger Tests
# test cases for TRIGGER privilege on db, table and column level
#======================================================================

--disable_abort_on_error

###########################################
################ Section 3.5.3 ############
# Check for the definer of Triggers       #
###########################################

# General setup to be used in all testcases
let $message= #########      Testcase for definer:   ########;
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

# create trigger with not existing definer shall deliver a warning:
	connection default;
        select current_user;
#	--warning 1449
        create definer=not_ex_user@localhost trigger trg1_0
                before INSERT on t1 for each row
                set new.f1 = 'trig 1_0-yes';
	drop trigger trg1_0;
# create trigger with definer test_yesprivs@localhost->succeed:
        create definer=test_yesprivs@localhost trigger trg1_0
                before INSERT on t1 for each row
                set new.f1 = 'trig 1_0-yes';
        grant  select, insert, update
                on priv_db.t1 to test_yesprivs@localhost;

	connection yes_privs;
	select current_user;
	use priv_db;
# user hasn't trigger privilege->fail:
	--error ER_TABLEACCESS_DENIED_ERROR
        insert into t1 (f1) values ('insert-no');
        select f1 from t1 order by f1;
	--error ER_TABLEACCESS_DENIED_ERROR
        drop trigger trg1_0;

        connection default;
        select current_user;
        grant  select, insert, update ,trigger
	        on priv_db.t1 to test_yesprivs@localhost;
	show grants for test_yesprivs@localhost;

	connection yes_privs;
        select current_user;
# user now has trigger privilege->succeed:
        insert into t1 (f1) values ('insert-no');
        select f1 from t1 order by f1;
        drop trigger trg1_0;
# user has not super privilege->fail:
	--error ER_SPECIFIC_ACCESS_DENIED_ERROR
        create definer=not_ex_user@localhost trigger trg1_0
                before INSERT on t1 for each row
                set new.f1 = 'trig 1_0-yes';
  connection default;
    create user definer_user@localhost identified by 'boo';
    grant set_user_id,system_user, trigger on *.* to definer_user@localhost;
  connect (con1,localhost,definer_user,boo,test,$MASTER_MYPORT,$MASTER_MYSOCK);
    create table test.t1 (f1 varchar(20));
    create definer=root@localhost trigger test.trg1_0
      before INSERT on test.t1 for each row
      set new.f1 = 'yes';
  connection default;
    drop trigger test.trg1_0;
    drop user definer_user@localhost;
    drop table test.t1;
  connection yes_privs;

# shall always succeed:
	create definer=current_user trigger trg1_1
		before INSERT on t1 for each row
		set new.f1 = 'trig 1_1-yes';
	insert into t1 (f1) values ('insert-no');
	select f1 from t1 order by f1;
# shall always succeed:
        create definer=test_yesprivs@localhost trigger trg1_2
                before UPDATE on t1 for each row
                set new.f1 = 'trig 1_2-yes';
        update t1 set f1 = 'update-yes' where f1 like '%trig%';
        select f1 from t1 order by f1;

        connection default;
        select current_user;
	grant trigger on priv_db.* to test_yesprivs@localhost
		with grant option;

# user has not super privilege->fail:
        connection yes_privs;
	select current_user;
	show grants;
	--error ER_SPECIFIC_ACCESS_DENIED_ERROR
        create definer=not_ex_user@localhost trigger trg1_3
                after UPDATE on t1 for each row
                set @var1 = 'trig 1_3-yes';

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

