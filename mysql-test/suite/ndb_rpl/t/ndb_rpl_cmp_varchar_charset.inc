
##############################
# Include file used by test case forbug#34388068

--echo
--echo
--echo #####################################

show variables like 'ndb_log_update_%';

--echo #####################################
--echo ############## Varchar ##############

--connection master
create table t1(str varchar(32) primary key) engine = ndb;

--echo Insert space padded row, then update it to non-padded
insert into t1(str) values ("one        ");
update t1 set str="one";

--echo Verify the replica update, no trailing spaces anymore!
--sync_slave_with_master
--connection slave
select concat("<<<",str,">>>") from t1;

--connection master
drop table t1;

--echo #########################################
--echo ############## Longvarchar ##############

--connection master
create table t1(str varchar(512) primary key) engine = ndb;

--echo Insert space padded row, then update it to non-padded
insert into t1(str) values ("one        ");
update t1 set str="one";

--echo Verify the replica update, no trailing spaces anymore!
--sync_slave_with_master
--connection slave
select concat("<<<",str,">>>") from t1;

--connection master
drop table t1;

--echo #####################################
--echo ############ Varbinary ##############
# Varbinary not affected by bug as the binary representation
# compared as non equal from the beginning.
--connection master
create table t1(str varbinary(32) primary key) engine = ndb;

--echo Insert space padded row, then update it to non-padded
insert into t1(str) values ("one        ");
update t1 set str="one";

--echo Verify the replica update, no trailing spaces anymore!
--sync_slave_with_master
--connection slave
select concat("<<<",str,">>>") from t1;

--connection master
drop table t1;

# Note that fixed-char data type is not used in the above
# test cases as fixed-chars are always stored space padded.
# Thus, updating to a non-space padded has no effect.


--echo ##########################################################
--echo # Lower and upper case are compared as equal.
--echo # Binary representation are unequal though, and update
--echo # Need to be propogated to replica even when the 'after' images
--echo # are 'compared-equal'.
--echo # Tests inserts 'xyz', then update it to 'XYZ'.
--echo # Check that rows are updated on replica as well.
--echo
--echo #####################################
--echo ############## Varchar ##############
--connection master
create table t1(str varchar(32) primary key) engine = ndb;

--echo Insert 'xyz', then update varchar(32) to 'XYZ'
insert into t1(str) values ("xyz");
update t1 set str="XYZ";

--echo Verify the replica updated varchar(32) to 'XYZ' as well
--sync_slave_with_master
--connection slave
select str from t1;

--connection master
drop table t1;

--echo ##################################
--echo ############## Char ##############
--connection master
create table t1(str char(32) primary key) engine = ndb;

--echo Insert 'xyz', then update char(32) to 'XYZ'
insert into t1(str) values ("xyz");
update t1 set str="XYZ";

--echo Verify the replica updated char(32) to 'XYZ' as well
--sync_slave_with_master
--connection slave
select str from t1;

--connection master
drop table t1;

--echo ##################################
--echo ############ Varbinary ###########
--connection master
create table t1(str varbinary(32) primary key) engine = ndb;

--echo Insert 'xyz', then update varbinary(32) to 'XYZ'
insert into t1(str) values ("xyz");
update t1 set str="XYZ";

--echo Verify the replica updated varbinary(32) to 'XYZ' as well
--sync_slave_with_master
--connection slave
select str from t1;

--connection master
drop table t1;
