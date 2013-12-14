#======================================================================
#
# Trigger Tests
# test cases for TRIGGER privilege on db, table and column level
#======================================================================

call mtr.add_suppression("Did not write failed .* into binary log while "
                         "storing table level and column level grants "
                         "in the privilege tables.");
--disable_abort_on_error

#########################################################
################ Section 3.5.3 ##########################
# Check for mix of db and table level of Triggers       #
#########################################################

# General setup to be used in all testcases
let $message= ####### Testcase for mix of db and table level: #######;
--source include/show_msg.inc

        --disable_warnings
	drop database if exists priv1_db;
	drop database if exists priv2_db;
	--enable_warnings
	create database priv1_db;
	create database priv2_db;
	use priv1_db;
	eval create table t1 (f1 char(20)) engine= $engine_type;
        eval create table t2 (f1 char(20)) engine= $engine_type;
	use priv2_db;
	eval create table t1 (f1 char(20)) engine= $engine_type;

	create User test_yesprivs@localhost;
	set password for test_yesprivs@localhost = password('PWD');
	revoke ALL PRIVILEGES, GRANT OPTION FROM test_yesprivs@localhost;
	grant ALL  on priv1_db.* to test_yesprivs@localhost;
        grant SELECT,UPDATE on priv2_db.* to test_yesprivs@localhost;
	show grants for test_yesprivs@localhost;

        create User test_noprivs@localhost;
        set password for test_noprivs@localhost = password('PWD');
        revoke ALL PRIVILEGES, GRANT OPTION FROM test_noprivs@localhost;
        grant SELECT,INSERT,UPDATE on priv1_db.* to test_noprivs@localhost;
        grant SELECT,INSERT on priv2_db.* to test_noprivs@localhost;
        show grants for test_noprivs@localhost;

	connect (yes_privs,localhost,test_yesprivs,PWD,test,$MASTER_MYPORT,$MASTER_MYSOCK);
	use priv1_db;
	connect (no_privs,localhost,test_noprivs,PWD,test,$MASTER_MYPORT,$MASTER_MYSOCK);
	use priv1_db;

# trigger priv on db level->create trigger for all tables successful

let $message= trigger privilege on one db1 db level, not on db2;
--source include/show_msg.inc
	connection yes_privs;
	select current_user;
	use priv1_db;
	create trigger trg1_1 before INSERT  on t1 for each row
		set new.f1 = 'trig 1_1-yes';
        create trigger trg2_1 before INSERT  on t2 for each row
                set new.f1 = 'trig 2_1-yes';
	use priv2_db;
	--error ER_TABLEACCESS_DENIED_ERROR
	create trigger trg1_1 before INSERT  on t1 for each row
		set new.f1 = 'trig1_1-yes';

	connection no_privs;
	select current_user;
	insert into t1 (f1) values ('insert1_no');
	select f1 from t1 order by f1;
	insert into t2 (f1) values ('insert1_no');
	select f1 from t2 order by f1;
	insert into priv2_db.t1 (f1) values ('insert21-yes');
	select f1 from priv2_db.t1 order by f1;

	use priv2_db;
        insert into t1 (f1) values ('insert1_yes');
        select f1 from t1 order by f1;
        insert into priv1_db.t1 (f1) values ('insert11-no');
        select f1 from priv1_db.t1 order by f1;
        insert into priv1_db.t2 (f1) values ('insert22-no');
        select f1 from priv1_db.t2 order by f1;

# revoke trigger priv on table level, that doesn't exists->fail

let $message= revoke trigger privilege on table level (not existing);
--source include/show_msg.inc
       	connection default;
        select current_user;
	use priv1_db;
	--error ER_NONEXISTING_TABLE_GRANT
        revoke TRIGGER on priv1_db.t1 from test_yesprivs@localhost;
        show grants for test_yesprivs@localhost;

        connection yes_privs;
	select current_user;
	drop trigger trg1_1;
	drop trigger trg2_1;
	use priv1_db;
	drop trigger trg1_1;
	drop trigger trg2_1;

# revoke the db level->create/drop/use trigger fail

        connection default;
	select current_user;
	use priv1_db;
	revoke TRIGGER on priv1_db.* from test_yesprivs@localhost;

################ Section 3.5.3 ############
# Check for the table level of Triggers   #
###########################################
let $message= no trigger privilege on table level for create:;
--source include/show_msg.inc

	connection yes_privs;
        select current_user;
	use priv1_db;
	--error ER_TABLEACCESS_DENIED_ERROR
	create trigger trg1_1 before INSERT on t1 for each row
		set new.f1 = 'trig 1_1-no';

	connection default;
        select current_user;
	show triggers;
        grant TRIGGER on priv1_db.t1 to test_yesprivs@localhost;
        show grants for test_yesprivs@localhost;

let $message= trigger privilege on table level for create:;
--source include/show_msg.inc
	connection yes_privs;
        select current_user;
	show triggers;
	create trigger trg1_2 before INSERT  on t1 for each row
		set new.f1 = 'trig 1_2-yes';
        --error ER_TABLEACCESS_DENIED_ERROR
        create trigger trg2_1 before INSERT  on t2 for each row
                set new.f1 = 'trig 2_1-no';

	connection no_privs;
        select current_user;
# need 'use db' to get the newest privileges
	use priv1_db;
        insert into t1 (f1) values ('insert2-no');
        select f1 from t1 order by f1;
        insert into t2 (f1) values ('insert2-yes');
        select f1 from t2 order by f1;
        insert into priv2_db.t1 (f1) values ('insert22-yes');
        select f1 from priv2_db.t1 order by f1;

	connection default;
	select current_user;
        grant TRIGGER on priv1_db.* to test_yesprivs@localhost;
        show grants for test_yesprivs@localhost;

# though granted on db level->create trigger fails (no use db)

	connection yes_privs;
        select current_user;
	--error ER_TABLEACCESS_DENIED_ERROR
        create trigger trg2_1 before INSERT  on t2 for each row
                set new.f1 = 'trig 2_1-yes';

# grant trigger takes effect

	use priv1_db;
        create trigger trg2_1 before INSERT  on t2 for each row
                set new.f1 = 'trig 2_1-yes';

        connection no_privs;
        select current_user;
	use priv1_db;
        insert into t1 (f1) values ('insert3-no');
        select f1 from t1 order by f1;
        insert into t2 (f1) values ('insert3-no');
        select f1 from t2 order by f1;
	use priv2_db;
        insert into priv1_db.t1 (f1) values ('insert12-no');
        select f1 from priv1_db.t1 order by f1;
        insert into priv1_db.t2 (f1) values ('insert23-no');
        select f1 from priv1_db.t2 order by f1;

	disconnect no_privs;

        connection yes_privs;
        select current_user;

	drop trigger trg1_2;
	drop trigger trg2_1;

# Cleanup table level
	--disable_warnings
        disconnect yes_privs;

	connection default;
        select current_user;


# general Cleanup
	drop database if exists priv1_db;
	drop database if exists priv2_db;
	drop user test_yesprivs@localhost;
	drop user test_noprivs@localhost;
	--enable_warnings

