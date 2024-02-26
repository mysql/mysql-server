#======================================================================
#
# Trigger Tests
# test cases for TRIGGER privilege on db, table and column level
#======================================================================

--disable_abort_on_error

#########################################################
################ Section 3.5.3 ##########################
# Check for column privileges of Triggers               #
#########################################################

# General setup to be used in all testcases
let $message= ####### Testcase for column privileges of triggers: #######;
--source include/show_msg.inc

        --disable_warnings
	drop database if exists priv_db;
	drop database if exists no_priv_db;
	--enable_warnings
	create database priv_db;
	use priv_db;
	eval create table t1 (f1 char(20)) engine= $engine_type;
        eval create table t2 (f1 char(20)) engine= $engine_type;

	create User test_yesprivs@localhost;
	set password for test_yesprivs@localhost = 'PWD';
	revoke ALL PRIVILEGES, GRANT OPTION FROM test_yesprivs@localhost;
	grant TRIGGER on priv_db.* to test_yesprivs@localhost;
	show grants for test_yesprivs@localhost;

        create User test_noprivs@localhost;
        set password for test_noprivs@localhost = 'PWD';
        revoke ALL PRIVILEGES, GRANT OPTION FROM test_noprivs@localhost;
        grant SELECT,UPDATE on priv_db.* to test_noprivs@localhost;
        show grants for test_noprivs@localhost;

	connect (yes_privs,localhost,test_yesprivs,PWD,test,$MASTER_MYPORT,$MASTER_MYSOCK);

	connect (no_privs,localhost,test_noprivs,PWD,test,$MASTER_MYPORT,$MASTER_MYSOCK);

# grant TRIGGER and UPDATE on column -> succeed

let $message= update only on column:;
--source include/show_msg.inc

	connection default;
	select current_user;
	grant SELECT(f1),INSERT,UPDATE(f1) on priv_db.t1
		 to test_yesprivs@localhost;
        grant SELECT(f1),INSERT,UPDATE(f1) on priv_db.t2
                 to test_yesprivs@localhost;

	connection yes_privs;
	select current_user;
	use priv_db;
	insert into t1 (f1) values ('insert1-yes');
        insert into t2 (f1) values ('insert1-yes');
 	create trigger trg1_1 before UPDATE on t1 for each row
		set new.f1 = 'trig 1_1-yes';
       create trigger trg2_1 before UPDATE on t2 for each row
                set new.f1 = 'trig 2_1-yes';

	connection no_privs;
	select current_user;
	use priv_db;
	select f1 from t1 order by f1;
	update t1 set f1 = 'update1_no'
		where f1 like '%insert%';
	select f1 from t1 order by f1;
        select f1 from t2 order by f1;
        update t2 set f1 = 'update1_no'
                where f1 like '%insert%';
        select f1 from t2 order by f1;

        connection default;
	select current_user;
	revoke UPDATE     on priv_db.*
		from test_yesprivs@localhost;
	revoke UPDATE(f1) on priv_db.t2
                 from test_yesprivs@localhost;
        show grants for test_yesprivs@localhost;

        connection yes_privs;
        select current_user;
        use priv_db;
        insert into t1 (f1) values ('insert2-yes');
        insert into t2 (f1) values ('insert2-yes');

	connection no_privs;
	select current_user;
	use priv_db;
        update t1 set f1 = 'update2_no'
                where f1 like '%insert%';
	--error ER_COLUMNACCESS_DENIED_ERROR
        update t2 set f1 = 'update2_no'
                where f1 like '%insert%';
        update t1 set f1 = 'update3_no'
                where f1 like '%insert%';
	--error ER_COLUMNACCESS_DENIED_ERROR
        update t2 set f1 = 'update3_no'
                where f1 like '%insert%';
        select f1 from t1 order by f1;
        select f1 from t2 order by f1;

# check with three columns
let $message= check if access only on one of three columns;
--source include/show_msg.inc

        connection default;
        select current_user;
	alter table priv_db.t1 add f2 char(20), add f3 int;
	revoke TRIGGER on priv_db.* from test_yesprivs@localhost;
	grant TRIGGER,SELECT on priv_db.t1 to test_yesprivs@localhost;
	grant UPDATE on priv_db.t2 to test_yesprivs@localhost;
	
	connection yes_privs;
	select current_user;
	use priv_db;
	insert into t1 values ('insert2-yes','insert2-yes',1);
	insert into t1 values ('insert3-yes','insert3-yes',2);
	select * from t1 order by f1;

	connection no_privs;
	select current_user;
	use priv_db;
	update t1 set 	f1 = 'update4-no',
			f2 = 'update4-yes',
			f3 = f3*10
		where f2 like '%yes';
	select * from t1 order by f1,f2,f3;

	connection yes_privs;
	select current_user;
	create trigger trg1_2 after UPDATE on t1 for each row
		set @f2 = 'trig 1_2-yes';

	connection no_privs;
	select current_user;
	update t1 set 	f1 = 'update5-yes',
			f2 = 'update5-yes'
		where f2 like '%yes';
	select * from t1 order by f1,f2,f3;
	select @f2;

        update t1 set f1 = 'update6_no'
                where f1 like '%insert%';
        --error ER_TABLEACCESS_DENIED_ERROR
        update t2 set f1 = 'update6_no'
                where f1 like '%insert%';
        update t1 set f1 = 'update7_no'
                where f1 like '%insert%';
        --error ER_TABLEACCESS_DENIED_ERROR
        update t2 set f1 = 'update7_no'
                where f1 like '%insert%';
        select f1 from t1 order by f1;
        select f1 from t2 order by f1;

# check with three columns


# check if update is rejected without trigger privilege

let $message= check if rejected without trigger privilege:;
--source include/show_msg.inc

        connection default;
        select current_user;
        revoke TRIGGER on priv_db.t1 from test_yesprivs@localhost;

        connection no_privs;
        select current_user;
	--error ER_TABLEACCESS_DENIED_ERROR
        update t1 set   f1 = 'update8-no',
                        f2 = 'update8-no'
                where f2 like '%yes';
        select * from t1 order by f1,f2,f3;
        select @f2;

# check trigger, but not update privilege on column

let $message= check trigger, but not update privilege on column:;
--source include/show_msg.inc

        connection default;
        select current_user;
        revoke UPDATE(f1) on priv_db.t1 from test_yesprivs@localhost;
	grant TRIGGER,UPDATE(f2),UPDATE(f3) on priv_db.t1
		to test_yesprivs@localhost;
	show grants for test_yesprivs@localhost;

        connection yes_privs;
        select current_user;
	use priv_db;
	drop trigger trg1_1;
        create trigger trg1_3 before UPDATE on t1 for each row
                set new.f1 = 'trig 1_3-yes';

        connection no_privs;
        select current_user;
	use priv_db;
	--error ER_COLUMNACCESS_DENIED_ERROR
        update t1 set   f1 = 'update9-no',
                        f2 = 'update9-no'
                where f2 like '%yes';
        select * from t1 order by f1,f2,f3;

# trigger is involved (table privilege) ->fail
	--error ER_COLUMNACCESS_DENIED_ERROR
	update t1 set f3= f3+1;
	select f3 from t1 order by f3;

        connection default;
        select current_user;
	revoke TRIGGER on priv_db.t1 from test_yesprivs@localhost;
        grant UPDATE(f1),UPDATE(f2),UPDATE(f3) on priv_db.t1
                to test_yesprivs@localhost;
        show grants for test_yesprivs@localhost;

# trigger is involved (table privilege) ->fail
        connection no_privs;
        select current_user;
        use priv_db;
        --error ER_TABLEACCESS_DENIED_ERROR
        update t1 set f3= f3+1;
        select f3 from t1 order by f3;

let $message= ##### trigger privilege on column level? #######;
--source include/show_msg.inc
	--error ER_PARSE_ERROR
	grant TRIGGER(f1) on priv_db.t1 to test_yesprivs@localhost;

# Cleanup table level
	--disable_warnings
        disconnect yes_privs;
	disconnect no_privs;

	connection default;
        select current_user;


# general Cleanup
	drop database if exists priv_db;
	drop user test_yesprivs@localhost;
	drop user test_noprivs@localhost;
	--enable_warnings

