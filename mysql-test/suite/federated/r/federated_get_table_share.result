CREATE DATABASE federated;
CREATE DATABASE federated;
###########################################################
#
# Test cases for wl#7593: Don't hold LOCK_open...
#
# Most of the tests related to this wl are in a different
# test file main.get_table_share.test. One scenario is relevant
# for the federated storage engine, which is why it is put into
# this file.
#
# This test case is based on two threads: One thread master
# opening table tb, being paused while opening the share,
# while another thread issues a ALTER and DROP SERVER commands
# to make sure the tables related to the dropped server are
# being closed.
#
# Please note that the include files federated.inc and
# federated_cleanup.inc creates an cleans up various resources
# related to the federated engine (connections, schemas, etc).
#
#
connection slave;
# Create a database and the tables on the slave:
CREATE DATABASE new_federated;
CREATE TABLE federated.ta (pk integer primary key);
CREATE TABLE federated.tb (pk integer primary key);
CREATE TABLE new_federated.ta (pk integer primary key);
CREATE TABLE new_federated.tb (pk integer primary key);
#
connection master;
# Create one server and two local, federated tables on the master:
CREATE SERVER s FOREIGN DATA WRAPPER 'mysql' OPTIONS
(USER 'root', HOST '127.0.0.1', PORT SLAVE_PORT, DATABASE 'federated');
CREATE TABLE federated.ta (pk integer primary key)
ENGINE= FEDERATED CONNECTION= 's';
CREATE TABLE federated.tb (pk integer primary key)
ENGINE= FEDERATED CONNECTION= 's';
#
# Check that the server is created, and do an insert into ta to make
# sure it is open and in the cache, then show open federated tables:
SELECT * from mysql.servers;
Server_name	Host	Db	Username	Password	Port	Socket	Wrapper	Owner
s	127.0.0.1	federated	root		SLAVE_PORT		mysql	
INSERT INTO federated.ta VALUES(0);
SELECT * from federated.ta;
pk
0
SHOW OPEN TABLES IN federated;
Database	Table	In_use	Name_locked
federated	ta	0	0
#
# Wait after releasing LOCK_open for tb, and make sure we never
# end up at the 'found_share' sync point:
SET DEBUG_SYNC= 'get_share_before_open SIGNAL open_master WAIT_FOR cont_master';
SET DEBUG_SYNC= 'get_share_found_share HIT_LIMIT 1';
INSERT INTO federated.tb VALUES(1);
#
connection default;
# Wait for open_master, then issue an ALTER SERVER command, which will
# close the tables for the server being altered. The table ta is open,
# and will be closed, while tb is in the process of being opened, and
# will therefore be skipped (i.e. not closed):
SET DEBUG_SYNC= 'now WAIT_FOR open_master';
ALTER SERVER s OPTIONS (DATABASE 'new_federated');
#
# Then, we show open federated tables to verify that neither tb nor
# ta are open, then we let the master open the tb share and insert,
# then we reap the master to be sure it is done with the insert:
SHOW OPEN TABLES IN federated;
Database	Table	In_use	Name_locked
SET DEBUG_SYNC= 'now SIGNAL cont_master';
connection master;
#
connection default;
# Now, tb should be open:
SHOW OPEN TABLES IN federated;
Database	Table	In_use	Name_locked
federated	tb	0	0
#
# A select from tb will show the inserted data since the
# actual server information from after the ALTER SERVER is used
# to open the table:
SELECT * from federated.tb;
pk
1
#
# A select from ta will now be empty since we switched
# remote table:
SELECT * from federated.ta;
pk
#
# Now, both ta and tb should be open:
SHOW OPEN TABLES IN federated;
Database	Table	In_use	Name_locked
federated	ta	0	0
federated	tb	0	0
#
connection master;
# Back to the master, we flush table tb and do a new insert,
# pausing before opening the share, just like we did above:
FLUSH TABLE federated.tb;
SHOW OPEN TABLES IN federated;
Database	Table	In_use	Name_locked
federated	ta	0	0
SET DEBUG_SYNC= 'get_share_before_open SIGNAL open_master WAIT_FOR cont_master';
SET DEBUG_SYNC= 'get_share_found_share HIT_LIMIT 1';
INSERT INTO federated.tb VALUES(2);
#
connection default;
# Wait for open_master, then issue a DROP SERVER command:
SET DEBUG_SYNC= 'now WAIT_FOR open_master';
DROP SERVER s;
#
# Let the master finish opening the tb share, and then reap it, which
# will fail since the server has been deleted:
SET DEBUG_SYNC= 'now SIGNAL cont_master';
connection master;
ERROR HY000: server name: 's' doesn't exist!
SET DEBUG_SYNC= 'RESET';
#
connection default;
# Then, we show open tables to verify that tb is left opened,
# while ta is not open anymore:
SHOW OPEN TABLES IN federated;
Database	Table	In_use	Name_locked
federated	tb	0	0
#
connection slave;
# Drop remote tables and database:
DROP TABLE federated.ta, federated.tb;
DROP TABLE new_federated.ta, new_federated.tb;
DROP DATABASE new_federated;
#
connection default;
# Reset DEBUG_SYNC and drop local tables:
SET DEBUG_SYNC= 'RESET';
DROP TABLE federated.ta, federated.tb;
#
# Disconnecting slave and master is handled in federated_cleanup.inc
DROP TABLE IF EXISTS federated.t1;
DROP DATABASE federated;
DROP TABLE IF EXISTS federated.t1;
DROP DATABASE federated;
