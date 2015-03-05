#### suite/funcs_1/triggers/triggers_03.inc
#======================================================================
#
# Trigger Tests
# (test case numbering refer to requirement document TP v1.1)
#======================================================================
# WL#4084: enable disabled parts. 2007-11-15, hhunger

# This test cannot be used for the embedded server because we check here
# privilgeges.
--source include/not_embedded.inc

set @orig_sql_mode_session= @@SESSION.sql_mode;
set @orig_sql_mode_global= @@GLOBAL.sql_mode;
set GLOBAL sql_mode= (select replace(@@GLOBAL.sql_mode,'NO_AUTO_CREATE_USER',''));
set SESSION sql_mode= (select replace(@@SESSION.sql_mode,'NO_AUTO_CREATE_USER',''));

USE test;
--source suite/funcs_1/include/tb3.inc



--disable_abort_on_error

###########################################
################ Section 3.5.3 ############
# Check for the global nature of Triggers #
###########################################

# General setup to be used in all testcases of 3.5.3
let $message= Testcase 3.5.3:;
--source include/show_msg.inc

        --disable_warnings
	drop database if exists priv_db;
	--enable_warnings
	create database priv_db;
	use priv_db;
        --replace_result $engine_type <engine_to_be_used>
	eval create table t1 (f1 char(20)) engine= $engine_type;

	create User test_noprivs@localhost;
	set password for test_noprivs@localhost = 'PWD';

	create User test_yesprivs@localhost;
	set password for test_yesprivs@localhost = 'PWD';

#Section 3.5.3.1 / 3.5.3.2
# Test case: Ensure TRIGGER privilege is required to create a trigger
#Section 3.5.3.3 / 3.5.3.4
# Test case: Ensure that root always has the TRIGGER privilege.
# OMR - No need to test this since SUPER priv is an existing one and not related
#       or added for triggers (TP 2005-06-06)
#Section 3.5.3.5 / 3.5.3.6
# Test case: Ensure that the TRIGGER privilege is required to drop a trigger.
let $message= Testcase 3.5.3.2/6:;
--source include/show_msg.inc

	revoke ALL PRIVILEGES, GRANT OPTION FROM test_noprivs@localhost;
	grant ALL  on *.* to test_noprivs@localhost;
	revoke TRIGGER on *.* from test_noprivs@localhost;
	show grants for test_noprivs@localhost;

	revoke ALL PRIVILEGES, GRANT OPTION FROM test_yesprivs@localhost;
	grant TRIGGER on *.* to test_yesprivs@localhost;
# Adding the minimal priv to be able to set to the db
	grant SELECT on priv_db.t1 to test_yesprivs@localhost;
	show grants for test_yesprivs@localhost;

	connect (no_privs,localhost,test_noprivs,PWD,test,$MASTER_MYPORT,$MASTER_MYSOCK);
	connect (yes_privs,localhost,test_yesprivs,PWD,test,$MASTER_MYPORT,$MASTER_MYSOCK);
	connection default;

let $message= Testcase 3.5.3.2:;
--source include/show_msg.inc

	connection no_privs;
        select current_user;
	use priv_db;

	--error ER_TABLEACCESS_DENIED_ERROR
	create trigger trg1_1 before INSERT on t1 for each row
		set new.f1 = 'trig 3.5.3.2_1-no';

	connection default;
	use priv_db;
	insert into t1 (f1) values ('insert 3.5.3.2-no');
	select f1 from t1 order by f1;

	connection yes_privs;
        select current_user;
	use priv_db;

	create trigger trg1_2 before INSERT  on t1 for each row
		set new.f1 = 'trig 3.5.3.2_2-yes';

	connection default;
        select current_user;
	use priv_db;

        --error ER_COLUMNACCESS_DENIED_ERROR
	insert into t1 (f1) values ('insert 3.5.3.2-yes');
	select f1 from t1 order by f1;

        grant UPDATE on priv_db.t1 to test_yesprivs@localhost;
        insert into t1 (f1) values ('insert 3.5.3.2-yes');
        select f1 from t1 order by f1;

let $message= Testcase 3.5.3.6:;
--source include/show_msg.inc

	connection no_privs;
	use priv_db;

	--error ER_TABLEACCESS_DENIED_ERROR
	drop trigger trg1_2;

	connection default;
	use priv_db;
	insert into t1 (f1) values ('insert 3.5.3.6-yes');
	select f1 from t1 order by f1;

	connection yes_privs;
	use priv_db;

	drop trigger trg1_2;

	connection default;
	use priv_db;
	insert into t1 (f1) values ('insert 3.5.3.6-no');
	select f1 from t1 order by f1;

# Cleanup
	--disable_warnings
	connection default;
	--error 0, ER_TRG_DOES_NOT_EXIST
	drop trigger trg1_2;
	disconnect no_privs;
	disconnect yes_privs;
	--enable_warnings


#Section 3.5.3.7
# Test case: Ensure that use of the construct "SET NEW. <column name> = <value>"
#            fails at CREATE TRIGGER time, if the current user does not have the
#            UPDATE privilege on the column specified

# --- 3.5.3.7a - Privs set on a global level
let $message=Testcase 3.5.3.7a:;
--source include/show_msg.inc

	revoke ALL PRIVILEGES, GRANT OPTION FROM test_noprivs@localhost;
	grant ALL  on *.* to test_noprivs@localhost;
	revoke UPDATE  on *.* from test_noprivs@localhost;
	show grants for test_noprivs@localhost;

	revoke ALL PRIVILEGES, GRANT OPTION FROM test_yesprivs@localhost;
	grant TRIGGER, UPDATE on *.* to test_yesprivs@localhost;
	show grants for test_yesprivs@localhost;

	connect (no_privs_424a,localhost,test_noprivs,PWD,test,$MASTER_MYPORT,$MASTER_MYSOCK);
	connect (yes_privs_424a,localhost,test_yesprivs,PWD,test,$MASTER_MYPORT,$MASTER_MYSOCK);

	connection no_privs_424a;
	select current_user;
	use priv_db;
	show grants;
	select f1 from t1 order by f1;

	create trigger trg4a_1 before INSERT on t1 for each row
		set new.f1 = 'trig 3.5.3.7-1a';

	connection default;
	--error ER_COLUMNACCESS_DENIED_ERROR
	insert into t1 (f1) values ('insert 3.5.3.7-1a');
	select f1 from t1 order by f1;
	drop trigger trg4a_1;

	connection yes_privs_424a;
	use priv_db;
	select current_user;
	show grants;
	create trigger trg4a_2 before INSERT  on t1 for each row
		set new.f1 = 'trig 3.5.3.7-2a';

	connection default;

	insert into t1 (f1) values ('insert 3.5.3.7-2b');
	select f1 from t1 order by f1;

# Cleanup
	--disable_warnings
	drop trigger trg4a_2;
	disconnect no_privs_424a;
	disconnect yes_privs_424a;
	--enable_warnings

# --- 3.5.3.7b - Privs set on a database level
let $message= Testcase 3.5.3.7b:;
--source include/show_msg.inc

	revoke ALL PRIVILEGES, GRANT OPTION FROM test_noprivs@localhost;
	grant TRIGGER on *.* to test_noprivs;
	grant ALL  on priv_db.* to test_noprivs@localhost;
	revoke UPDATE  on priv_db.* from test_noprivs@localhost;
	show grants for test_noprivs;

	revoke ALL PRIVILEGES, GRANT OPTION FROM test_yesprivs@localhost;
	grant TRIGGER on *.* to test_yesprivs@localhost;
	grant UPDATE on priv_db.* to test_yesprivs@localhost;
	show grants for test_yesprivs@localhost;

	connect (no_privs_424b,localhost,test_noprivs,PWD,test,$MASTER_MYPORT,$MASTER_MYSOCK);
	connect (yes_privs_424b,localhost,test_yesprivs,PWD,test,$MASTER_MYPORT,$MASTER_MYSOCK);
	connection default;

	connection no_privs_424b;
	show grants;
	use priv_db;

	create trigger trg4b_1 before UPDATE on t1 for each row
		set new.f1 = 'trig 3.5.3.7-1b';

	connection default;
	insert into t1 (f1) values ('insert 3.5.3.7-1b');
	select f1 from t1 order by f1;
	update t1 set  f1 = 'update 3.5.3.7-1b' where f1 = 'insert 3.5.3.7-1b';
	select f1 from t1 order by f1;
	drop trigger trg4b_1;

	connection yes_privs_424b;
	show grants;
	use priv_db;
	create trigger trg4b_2 before UPDATE  on t1 for each row
		set new.f1 = 'trig 3.5.3.7-2b';

	connection default;

	insert into t1 (f1) values ('insert 3.5.3.7-2b');
	select f1 from t1 order by f1;
	update t1 set  f1 = 'update 3.5.3.7-2b' where f1 = 'insert 3.5.3.7-2b';
	select f1 from t1 order by f1;
# Cleanup
	--disable_warnings
	drop trigger trg4b_2;
	disconnect no_privs_424b;
	disconnect yes_privs_424b;
	--enable_warnings

# --- 3.5.3.7c - Privs set on a table level
let $message= Testcase 3.5.3.7c;
--source include/show_msg.inc

	revoke ALL PRIVILEGES, GRANT OPTION FROM test_noprivs@localhost;
	grant TRIGGER on *.* to test_noprivs@localhost;
	grant ALL  on priv_db.t1 to test_noprivs@localhost;
	revoke UPDATE  on priv_db.t1 from test_noprivs@localhost;
	show grants for test_noprivs;

	revoke ALL PRIVILEGES, GRANT OPTION FROM test_yesprivs@localhost;
	grant TRIGGER on *.* to test_yesprivs@localhost;
	grant UPDATE on priv_db.t1 to test_yesprivs@localhost;
	show grants for test_yesprivs@localhost;

	connect (no_privs_424c,localhost,test_noprivs,PWD,test,$MASTER_MYPORT,$MASTER_MYSOCK);
	connect (yes_privs_424c,localhost,test_yesprivs,PWD,test,$MASTER_MYPORT,$MASTER_MYSOCK);
	connection default;

	connection no_privs_424c;
	show grants;
	use priv_db;

	create trigger trg4c_1 before INSERT on t1 for each row
		set new.f1 = 'trig 3.5.3.7-1c';

	connection default;
	insert into t1 (f1) values ('insert 3.5.3.7-1c');
	select f1 from t1 order by f1;
	drop trigger trg4c_1;

	connection yes_privs_424c;
	show grants;
	use priv_db;
	create trigger trg4c_2 before INSERT  on t1 for each row
		set new.f1 = 'trig 3.5.3.7-2c';

	connection default;

	insert into t1 (f1) values ('insert 3.5.3.7-2c');
	select f1 from t1 order by f1;

# Cleanup
	--disable_warnings
	drop trigger trg4c_2;
	disconnect no_privs_424c;
	disconnect yes_privs_424c;
	--enable_warnings

# --- 3.5.3.7d - Privs set on a column level
--disable_query_log
let $message= Testcase 3.5.3.7d:;
--enable_query_log
--source include/show_msg.inc

	revoke ALL PRIVILEGES, GRANT OPTION FROM test_noprivs@localhost;
	grant TRIGGER on *.* to test_noprivs@localhost;
# There is no ALL privs on the column level
	grant SELECT (f1), INSERT (f1) on priv_db.t1 to test_noprivs@localhost;
	show grants for test_noprivs;

	revoke ALL PRIVILEGES, GRANT OPTION FROM test_yesprivs@localhost;
	grant TRIGGER on *.* to test_yesprivs@localhost;
	grant UPDATE (f1) on priv_db.t1 to test_yesprivs@localhost;
	show grants for test_noprivs;

	connect (no_privs_424d,localhost,test_noprivs,PWD,test,$MASTER_MYPORT,$MASTER_MYSOCK);
	connect (yes_privs_424d,localhost,test_yesprivs,PWD,test,$MASTER_MYPORT,$MASTER_MYSOCK);
	connection default;

	connection no_privs_424d;
	show grants;
	use priv_db;
	create trigger trg4d_1 before INSERT on t1 for each row
		set new.f1 = 'trig 3.5.3.7-1d';

	connection default;
	insert into t1 (f1) values ('insert 3.5.3.7-1d');
	select f1 from t1 order by f1;
	drop trigger trg4d_1;

	connection yes_privs_424d;
	show grants;
	use priv_db;
	create trigger trg4d_2 before INSERT  on t1 for each row
		set new.f1 = 'trig 3.5.3.7-2d';

	connection default;

	insert into t1 (f1) values ('insert 3.5.3.7-2d');
	select f1 from t1 order by f1;

# Cleanup
	--disable_warnings
	drop trigger trg4d_2;
	disconnect no_privs_424d;
	disconnect yes_privs_424d;
	--enable_warnings

#Section 3.5.3.8
# Test case: Ensure that use of the construct "SET <target> = NEW. <Column name>" fails
#            at CREATE TRIGGER time, if the current user does not have the SELECT privilege
#            on the column specified.

# --- 3.5.3.8a - Privs set on a global level
let $message= Testcase 3.5.3.8a:;
--source include/show_msg.inc

	revoke ALL PRIVILEGES, GRANT OPTION FROM test_noprivs@localhost;
	grant ALL  on *.* to test_noprivs@localhost;
	revoke SELECT  on *.* from test_noprivs@localhost;
	show grants for test_noprivs@localhost;

	revoke ALL PRIVILEGES, GRANT OPTION FROM test_yesprivs@localhost;
	grant TRIGGER, SELECT on *.* to test_yesprivs@localhost;
	show grants for test_yesprivs@localhost;

	connect (no_privs_425a,localhost,test_noprivs,PWD,test,$MASTER_MYPORT,$MASTER_MYSOCK);
	connect (yes_privs_425a,localhost,test_yesprivs,PWD,test,$MASTER_MYPORT,$MASTER_MYSOCK);
	connection default;

	connection no_privs_425a;
	select current_user;
	use priv_db;
	show grants;

	create trigger trg5a_1 before INSERT on t1 for each row
		set @test_var = new.f1;

	connection default;
        set @test_var = 'before trig 3.5.3.8-1a';
	select @test_var;
	insert into t1 (f1) values ('insert 3.5.3.8-1a');
	select @test_var;
	drop trigger trg5a_1;

	connection yes_privs_425a;
	use priv_db;
	select current_user;
	show grants;
	create trigger trg5a_2 before INSERT  on t1 for each row
		set @test_var= new.f1;

	connection default;
        set @test_var= 'before trig 3.5.3.8-2a';
	select @test_var;

	insert into t1 (f1) values ('insert 3.5.3.8-2a');
	select @test_var;

# Cleanup
	--disable_warnings
	drop trigger trg5a_2;
	disconnect no_privs_425a;
	disconnect yes_privs_425a;
	--enable_warnings

# --- 3.5.3.8b - Privs set on a database level
let $message= Testcase: 3.5.3.8b;
--source include/show_msg.inc

	revoke ALL PRIVILEGES, GRANT OPTION FROM test_noprivs@localhost;
	grant TRIGGER on *.* to test_noprivs@localhost;
	grant ALL  on priv_db.* to test_noprivs@localhost;
	revoke SELECT  on priv_db.* from test_noprivs@localhost;
	show grants for test_noprivs@localhost;

	revoke ALL PRIVILEGES, GRANT OPTION FROM test_yesprivs@localhost;
	grant TRIGGER on *.* to test_yesprivs@localhost;
	grant SELECT on priv_db.* to test_yesprivs@localhost;
	show grants for test_yesprivs@localhost;

	connect (no_privs_425b,localhost,test_noprivs,PWD,test,$MASTER_MYPORT,$MASTER_MYSOCK);
	connect (yes_privs_425b,localhost,test_yesprivs,PWD,test,$MASTER_MYPORT,$MASTER_MYSOCK);
	connection default;

	connection no_privs_425b;
	show grants;
	use priv_db;

	create trigger trg5b_1 before UPDATE on t1 for each row
		set @test_var= new.f1;

	connection default;
        set @test_var= 'before trig 3.5.3.8-1b';
	insert into t1 (f1) values ('insert 3.5.3.8-1b');
	select @test_var;
	update t1 set  f1= 'update 3.5.3.8-1b' where f1 = 'insert 3.5.3.8-1b';
	select @test_var;
	drop trigger trg5b_1;

	connection yes_privs_425b;
	show grants;
	use priv_db;
	create trigger trg5b_2 before UPDATE  on t1 for each row
		set @test_var= new.f1;

	connection default;
        set @test_var= 'before trig 3.5.3.8-2b';
	insert into t1 (f1) values ('insert 3.5.3.8-2b');
	select @test_var;

	update t1 set  f1= 'update 3.5.3.8-2b' where f1 = 'insert 3.5.3.8-2b';
	select @test_var;
# Cleanup
	--disable_warnings
	drop trigger trg5b_2;
	disconnect no_privs_425b;
	disconnect yes_privs_425b;
	--enable_warnings

# --- 3.5.3.8c - Privs set on a table level
let $message= Testcase 3.5.3.8c:;
--source include/show_msg.inc

	revoke ALL PRIVILEGES, GRANT OPTION FROM test_noprivs@localhost;
	grant TRIGGER on *.* to test_noprivs@localhost;
	grant ALL  on priv_db.t1 to test_noprivs@localhost;
	revoke SELECT  on priv_db.t1 from test_noprivs@localhost;
	show grants for test_noprivs@localhost;

	revoke ALL PRIVILEGES, GRANT OPTION FROM test_yesprivs@localhost;
	grant TRIGGER on *.* to test_yesprivs@localhost;
	grant SELECT on priv_db.t1 to test_yesprivs@localhost;
	show grants for test_yesprivs@localhost;

	connect (no_privs_425c,localhost,test_noprivs,PWD,test,$MASTER_MYPORT,$MASTER_MYSOCK);
	connect (yes_privs_425c,localhost,test_yesprivs,PWD,test,$MASTER_MYPORT,$MASTER_MYSOCK);
	connection default;

	connection no_privs_425c;
	show grants;
	use priv_db;

	create trigger trg5c_1 before INSERT on t1 for each row
		set @test_var= new.f1;

	connection default;
        set @test_var= 'before trig 3.5.3.8-1c';
	insert into t1 (f1) values ('insert 3.5.3.8-1c');
	select @test_var;
	drop trigger trg5c_1;

	connection yes_privs_425c;
	show grants;
	use priv_db;
	create trigger trg5c_2 before INSERT  on t1 for each row
		set @test_var= new.f1;

	connection default;
        set @test_var='before trig 3.5.3.8-2c';

	insert into t1 (f1) values ('insert 3.5.3.8-2c');
	select @test_var;
# Cleanup
	--disable_warnings
	drop trigger trg5c_2;
	disconnect no_privs_425c;
	disconnect yes_privs_425c;
	--enable_warnings

# --- 3.5.3.8d - Privs set on a column level
let $message=Testcase: 3.5.3.8d:;
--source include/show_msg.inc

	revoke ALL PRIVILEGES, GRANT OPTION FROM test_noprivs@localhost;
	grant TRIGGER on *.* to test_noprivs@localhost;
# There is no ALL prov on the column level
	grant UPDATE (f1), INSERT (f1) on priv_db.t1 to test_noprivs@localhost;
	show grants for test_noprivs@localhost;

	revoke ALL PRIVILEGES, GRANT OPTION FROM test_yesprivs@localhost;
	grant TRIGGER on *.* to test_yesprivs@localhost;
	grant SELECT (f1) on priv_db.t1 to test_yesprivs@localhost;
	show grants for test_noprivs@localhost;

	connect (no_privs_425d,localhost,test_noprivs,PWD,test,$MASTER_MYPORT,$MASTER_MYSOCK);
	connect (yes_privs_425d,localhost,test_yesprivs,PWD,test,$MASTER_MYPORT,$MASTER_MYSOCK);
	connection default;

	connection no_privs_425d;
	show grants;
	use priv_db;
	create trigger trg5d_1 before INSERT on t1 for each row
		set @test_var= new.f1;

	connection default;
        set @test_var='before trig 3.5.3.8-1d';
	insert into t1 (f1) values ('insert 3.5.3.8-1d');
	select @test_var;
	drop trigger trg5d_1;

	connection yes_privs_425d;
	show grants;
	use priv_db;
	create trigger trg5d_2 before INSERT  on t1 for each row
		set @test_var= new.f1;

	connection default;
        set @test_var='before trig 3.5.3.8-2d';

	insert into t1 (f1) values ('insert 3.5.3.8-2d');
	select @test_var;

# Cleanup 3.5.3.8
	--disable_warnings
	drop trigger trg5d_2;
	--enable_warnings

# --- 3.5.3.x   to test for trigger definer privs in the case of trigger
#               actions (insert/update/delete/select) performed on other
#               tables.
let $message=Testcase: 3.5.3.x:;
--source include/show_msg.inc

	use priv_db;
        --disable_warnings
        drop table if exists t1;
        drop table if exists t2;
        --enable_warnings

        --replace_result $engine_type <engine_to_be_used>
        eval create table t1 (f1 int) engine= $engine_type;
        --replace_result $engine_type <engine_to_be_used>
        eval create table t2 (f2 int) engine= $engine_type;

	revoke ALL PRIVILEGES, GRANT OPTION FROM test_yesprivs@localhost;
	grant TRIGGER on *.* to test_yesprivs@localhost;
	grant SELECT, UPDATE on priv_db.t1 to test_yesprivs@localhost;
	grant SELECT on priv_db.t2 to test_yesprivs@localhost;
	show grants for test_yesprivs@localhost;

       connect (yes_353x,localhost,test_yesprivs,PWD,test,$MASTER_MYPORT,$MASTER_MYSOCK);

	connection yes_353x;
        select current_user;
	use priv_db;

	create trigger trg1 before insert  on t1 for each row
		insert into t2 values (new.f1);

	connection default;
	use priv_db;
	insert into t1 (f1) values (4);
	revoke SELECT on priv_db.t2 from test_yesprivs@localhost;
	grant INSERT on priv_db.t2 to test_yesprivs@localhost;
	insert into t1 (f1) values (4);
	select f1 from t1 order by f1;
	select f2 from t2 order by f2;

	connection yes_353x;
	use priv_db;
        drop trigger trg1;

	create trigger trg2 before insert  on t1 for each row
		update t2 set f2=new.f1-1;

	connection default;
	use priv_db;
	insert into t1 (f1) values (2);
	revoke INSERT on priv_db.t2 from test_yesprivs@localhost;
	grant UPDATE on priv_db.t2 to test_yesprivs@localhost;
	insert into t1 (f1) values (2);
	select f1 from t1 order by f1;
	select f2 from t2 order by f2;

	connection yes_353x;
	use priv_db;
        drop trigger trg2;

	create trigger trg3 before insert  on t1 for each row
		select f2 into @aaa from t2 where f2=new.f1;

	connection default;
	use priv_db;
	insert into t1 (f1) values (1);
	revoke UPDATE on priv_db.t2 from test_yesprivs@localhost;
	grant SELECT on priv_db.t2 to test_yesprivs@localhost;
	insert into t1 (f1) values (1);
	select f1 from t1 order by f1;
	select f2 from t2 order by f2;
        select @aaa;

	connection yes_353x;
	use priv_db;
        drop trigger trg3;

	create trigger trg4 before insert  on t1 for each row
		delete from t2;

	connection default;
	use priv_db;
	insert into t1 (f1) values (1);
	revoke SELECT on priv_db.t2 from test_yesprivs@localhost;
	grant DELETE on priv_db.t2 to test_yesprivs@localhost;
	insert into t1 (f1) values (1);
	select f1 from t1 order by f1;
	select f2 from t2 order by f2;



# Cleanup 3.5.3
	--disable_warnings
	drop database if exists priv_db;
	drop user test_yesprivs@localhost;
	drop user test_noprivs@localhost;
	drop user test_noprivs;
	--enable_warnings

use test;
drop table tb3;

set GLOBAL sql_mode= @orig_sql_mode_global;
set SESSION sql_mode= @orig_sql_mode_session;
