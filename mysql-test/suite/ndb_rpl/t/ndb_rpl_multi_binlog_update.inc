###############################################################################
# Description: Performs binloggable and non binloggable DDL and DML at each
#              MySQLD in pair of clusters with (at least) 3 MySQLDs each.
#              Then connects to each MySQLD and examines the contents of
#              its Binlog.
#              Expected contents depends on the configuration of Binlogging
#              in the cluster used.
#              Default from rpl_ndb_multi_binlog_update.test is as shown
#              below. 
# 
# Testing scenario: Cluster 1 replicates to Cluster 2
#
#  Key : BL= Binlogging, LSU = LogSlaveUpdates = On
#
#                       BL                        BL
#  cluster 1 [  srv_master  srv_master1  srv_master2  ]
#                   |
#                   |-----------------------
#                   v          v            v
#  cluster 2 [  srv_slave  srv_slave1  srv_slave2 ]
#                                  BL      BL LSU
#
#  Cluster 1:
#  - Schema change originates on all severs in Cluster 1
#  - MySQLD1 which is binlogging master. 
#    Identification: connection (srv_master), config (mysqld.1.1).
#  - MySQLD2 which is not binlogging.
#    Identification: connection (srv_master1), config (mysqld.1.2).
#  - MySQLD3 which is binlogging but not currently master.
#    Identification: connection (srv_master2), config (mysqld.1.3).
#
#  Cluster 2:
#  - MySQLD1: 
#    Can act as slave, not binlogging
#    Identification: connection (srv_slave), config (mysqld.1.slave).
#  - MySQLD2: 
#    Can act as slave, binlogging, log-replica-updates = off
#    Identification: connection (srv_slave1), config (mysqld.2.slave).
#  - MySQLD3 
#    Can act as slave, binlogging, log-replica-updates = on
#    Identification: connection (srv_slave2), config (mysqld.3.slave).
#
# See rpl_ndb_multi_binlog_update.test for default configuration.
# Originally motivated by bug#45756
# See rpl_ndb_slave_lsu.test for full testcase
#
# Preconditions :
#   - Connections srv_master, srv_master1, srv_master2, srv_slave, 
#     srv_slave1, srv_slave2 exist.
#   - $which_slave contains the name of the slave server performing
#     the replication slave role.
#
###############################################################################

###############################################################################
#                            Checking Replication
###############################################################################
--echo *** Generating slave cluster originated binloggable changes ***
connection srv_slave;
CREATE TABLE bug_45756_slave_logged_1 (a int) engine = NDB;
INSERT INTO bug_45756_slave_logged_1 VALUES (1);
DROP TABLE bug_45756_slave_logged_1;

connection srv_slave1;
CREATE TABLE bug_45756_slave_logged_2 (a int) engine = NDB;
INSERT INTO bug_45756_slave_logged_2 VALUES (1);
DROP TABLE bug_45756_slave_logged_2;

connection srv_slave2;
CREATE TABLE bug_45756_slave_logged_3 (a int) engine = NDB;
INSERT INTO bug_45756_slave_logged_3 VALUES (1);
DROP TABLE bug_45756_slave_logged_3;

--echo ***Generating slave cluster non-binloggable changes***
connection srv_slave;
SET SQL_LOG_BIN= 0;
CREATE TABLE bug_45756_slave_not_logged_1 (a int) engine = NDB;
INSERT INTO bug_45756_slave_not_logged_1 VALUES (1);
DROP TABLE bug_45756_slave_not_logged_1;
SET SQL_LOG_BIN= 1;

connection srv_slave1;
SET SQL_LOG_BIN= 0;
CREATE TABLE bug_45756_slave_not_logged_2 (a int) engine = NDB;
INSERT INTO bug_45756_slave_not_logged_2 VALUES (1);
DROP TABLE bug_45756_slave_not_logged_2;
SET SQL_LOG_BIN= 1;

connection srv_slave2;
SET SQL_LOG_BIN= 0;
CREATE TABLE bug_45756_slave_not_logged_3 (a int) engine = NDB;
INSERT INTO bug_45756_slave_not_logged_3 VALUES (1);
DROP TABLE bug_45756_slave_not_logged_3;
SET SQL_LOG_BIN= 1;

--echo *** Generating data to be replicated ***
connection srv_master;
CREATE TABLE bug45756_master_logged_1 (a int) engine = NDB;
INSERT INTO bug45756_master_logged_1 VALUES (1);
DROP TABLE bug45756_master_logged_1;

connection srv_master1;
CREATE TABLE bug45756_master_logged_2 (a int) engine = NDB;
INSERT INTO bug45756_master_logged_2 VALUES (1);
DROP TABLE bug45756_master_logged_2;

connection srv_master2;
CREATE TABLE bug45756_master_logged_3 (a int) engine = NDB;
INSERT INTO bug45756_master_logged_3 VALUES (1);
DROP TABLE bug45756_master_logged_3;

--echo *** Generating changes not to be replicated ***
connection srv_master;
SET SQL_LOG_BIN= 0;
CREATE TABLE bug45756_master_not_logged_1 (a int) engine = NDB;
INSERT INTO bug45756_master_not_logged_1 VALUES (1);
DROP TABLE bug45756_master_not_logged_1;
SET SQL_LOG_BIN= 1;

connection srv_master1;
SET SQL_LOG_BIN= 0;
CREATE TABLE bug45756_master_not_logged_2 (a int) engine = NDB;
INSERT INTO bug45756_master_not_logged_2 VALUES (1);
DROP TABLE bug45756_master_not_logged_2;
SET SQL_LOG_BIN= 1;

connection srv_master2;
SET SQL_LOG_BIN= 0;
CREATE TABLE bug45756_master_not_logged_3 (a int) engine = NDB;
INSERT INTO bug45756_master_not_logged_3 VALUES (1);
DROP TABLE bug45756_master_not_logged_3;
SET SQL_LOG_BIN= 1;

connection srv_master;
sync_slave_with_master $which_slave;

--echo *** Checking binlog contents on every server in both clusters ***
connection srv_master;
--echo 
--echo 
--echo 
--echo connection srv_master;
show variables like 'server_id';
show variables like 'log_bin';
show variables like 'log_replica_updates';
--source include/show_binlog_events.inc
let $BINLOG_FILENAME=master-bin;
--source suite/ndb_rpl/t/ndb_rpl_get_binlog_events.inc


connection srv_master1;
--echo 
--echo 
--echo 
--echo connection srv_master1;
show variables like 'server_id';
show variables like 'log_bin';
show variables like 'log_replica_updates';
--source include/show_binlog_events.inc

connection srv_master2;
--echo 
--echo 
--echo 
--echo connection srv_master2;
show variables like 'server_id';
show variables like 'log_bin';
show variables like 'log_replica_updates';
--source include/show_binlog_events.inc
let $BINLOG_FILENAME=master-bin;
--source suite/ndb_rpl/t/ndb_rpl_get_binlog_events.inc

connection srv_slave;
--echo 
--echo 
--echo 
--echo connection srv_slave;
show variables like 'server_id';
show variables like 'log_bin';
show variables like 'log_replica_updates';
--source include/show_binlog_events.inc

connection srv_slave1;
--echo 
--echo 
--echo 
--echo connection srv_slave1;
show variables like 'server_id';
show variables like 'log_bin';
show variables like 'log_replica_updates';
--source include/show_binlog_events.inc
let $BINLOG_FILENAME=slave-master-bin;
--source suite/ndb_rpl/t/ndb_rpl_get_binlog_events.inc

connection srv_slave2;
--echo 
--echo 
--echo 
--echo connection srv_slave2;
show variables like 'server_id';
show variables like 'log_bin';
show variables like 'log_replica_updates';
--source include/show_binlog_events.inc
let $BINLOG_FILENAME=slave-master-bin;
--source suite/ndb_rpl/t/ndb_rpl_get_binlog_events.inc

connection $which_slave;