--source include/have_plugin_auth.inc
--source include/rpl/init_source_replica.inc

#
# Check that replication slave can connect to master using an account
# which authenticates with an external authentication plugin (bug#12897501).

#
# First stop the slave to guarantee that nothing is replicated.
#
--connection slave
--echo [connection slave]
--source include/rpl/stop_replica.inc
#
# Create an replication account on the master.
#
--connection master
--echo [connection master]
CREATE USER 'plug_user' IDENTIFIED WITH 'test_plugin_server' AS 'plug_user';
GRANT REPLICATION SLAVE ON *.* TO plug_user;
FLUSH PRIVILEGES;

#
# Now go to slave and change the replication user.
#
--connection slave
--echo [connection slave]
--let $source_user= query_get_value(SHOW REPLICA STATUS, Source_User, 1)
--replace_column 2 ####
CHANGE REPLICATION SOURCE TO 
  SOURCE_USER=     'plug_user',
  SOURCE_PASSWORD= 'plug_user',
  SOURCE_RETRY_COUNT= 0;

#
# Start replica with new replication account - this should trigger connection
# to the master server.
#
--source include/rpl/start_replica.inc

# Replicate all statements executed on master, in this case,
# (creation of the plug_user account).
#
--connection master
--sync_slave_with_master
--echo # Slave in-sync with master now.

SELECT user, plugin, authentication_string FROM mysql.user WHERE user LIKE 'plug_user';

#
# Now we can stop the slave and clean up.
#
# Note: it is important that slave is stopped at this
# moment - otherwise master's cleanup statements
# would be replicated on slave!
#
--echo # Cleanup (on slave).
--source include/rpl/stop_replica.inc
--replace_column 2 ####
eval CHANGE REPLICATION SOURCE TO SOURCE_USER='$source_user';
DROP USER 'plug_user';

--echo # Cleanup (on master).
--connection master
DROP USER 'plug_user';

--let $rpl_only_running_threads= 1
--source include/rpl/deinit.inc
