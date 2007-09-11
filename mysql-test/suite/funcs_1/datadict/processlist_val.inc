########### suite/funcs_1/datadict/processlist_val.inc #################
#                                                                      #
# Testing of values within INFORMATION_SCHEMA.PROCESSLIST              #
#                                                                      #
# Ensure that the values fit to the current state of the connection    #
# and especially that they change if a connection does nothing or      #
# runs some SQL.                                                       #
# Examples:                                                            #
# - change the default database                                        #
# - send some time no SQL command to the server                        #
# - send a long running query                                          #
#                                                                      #
# Note(mleich):                                                        #
# 1. Please inform me if this test fails because of timing problems.   #
#    I tried to avoid instabilities but the values within the column   #
#    TIME are very sensible to fluctuations of the machine load.       #
#    I had to unify some TIME values with "--replace_result" in cases  #
#    where they are too unstable.                                      #
# 2. Storage engine variants of this test do not make sense.           #
#    -  I_S tables use the MEMORY storage engine whenever possible.    #
#    -  There are some I_S table which need column data types which    #
#       are not supported by MEMORY. Example: LONGTEXT/BLOB            #
#       MyISAM will be used for such tables.                           #
#       The column PROCESSLIST.INFO is of data type LONGTEXT           #
#           ----> MyISAM                                               #
#    -  There is no impact of the GLOBAL(server) or SESSION default    #
#       storage engine setting on the engine used for I_S tables.      #
#       That means we cannot get NDB or InnoDB instead.                #
# 3. The SHOW (FULL) PROCESSLIST command are for comparison.           #
#    The main test target is INFORMATION_SCHEMA.PROCESSLIST !          #
#                                                                      #
# Creation:                                                            #
# 2007-08-09 mleich Implement this test as part of                     #
#                   WL#3982 Test information_schema.processlist        #
#                                                                      #
########################################################################

# Basic preparations
--disable_abort_on_error
DROP USER ddicttestuser1@'localhost';
--enable_abort_on_error
CREATE USER ddicttestuser1@'localhost';
GRANT ALL ON *.* TO ddicttestuser1@'localhost';
REVOKE PROCESS ON *.* FROM ddicttestuser1@'localhost';
SET PASSWORD FOR ddicttestuser1@'localhost' = PASSWORD('ddictpass');
--disable_warnings
DROP TABLE IF EXISTS test.t1;
--enable_warnings
CREATE TABLE test.t1 (f1 BIGINT);


# Show the definition of the PROCESSLIST table
#--------------------------------------------------------------------------
SHOW CREATE TABLE INFORMATION_SCHEMA.PROCESSLIST;


# Ensure that the values follow the changing default database and statement
#--------------------------------------------------------------------------
# - We have now exact one connection. -> One record
SELECT COUNT(*) FROM INFORMATION_SCHEMA.PROCESSLIST;
# - Other expected values
#   - USER = 'root'
#   - HOST = 'localhost'
#   - DB = 'test'
#   - Command = 'Query'
#   - TIME = 0, I hope the testing machines are all time fast enough
#   - State IS NULL
#   - INFO must contain the corresponding SHOW/SELECT PROCESSLIST
USE test;
--replace_column 1 <ID>
SELECT * FROM INFORMATION_SCHEMA.PROCESSLIST;
--replace_column 1 <ID>
SHOW FULL PROCESSLIST;
#
# Expect to see now DB = 'information_schema'
USE information_schema;
--replace_column 1 <ID> 6 <TIME>
SELECT * FROM INFORMATION_SCHEMA.PROCESSLIST;
--replace_column 1 <ID> 6 <TIME>
SHOW FULL PROCESSLIST;
#
# Expect to see now INFO = 'SELECT INFO FROM INFORMATION_SCHEMA.PROCESSLIST;'
SELECT INFO FROM INFORMATION_SCHEMA.PROCESSLIST;


# Ensure that the values for an additional connection are correct
#--------------------------------------------------------------------------
SELECT ID INTO @my_proclist_id FROM INFORMATION_SCHEMA.PROCESSLIST;
--echo
--echo ----- establish connection ddicttestuser1 (user = ddicttestuser1) -----
connect (ddicttestuser1,localhost,ddicttestuser1,ddictpass,information_schema);
#
--echo
--echo ----- switch to connection default (user = root) -----
connection default;
# - We have now a second connection.
# - important values in second connection
#   - USER = ddicttestuser1
#   - HOST = 'localhost'
#   - DB = 'information_schema'
#   - Command = 'Sleep'
#   - TIME = 0, I hope the testing machines are all time fast enough
#   - State IS NULL
#   - INFO must be empty
--replace_column 1 <ID>
SELECT * FROM INFORMATION_SCHEMA.PROCESSLIST;
#    Change the SQL command (affects content of INFO)
--replace_column 1 <ID>
SHOW FULL PROCESSLIST;
#
--echo # Sleep some time
# The value of TIME must increase after some sleeps.
# So poll till TIME changes.
let $run= 10;
while ($run)
{
   dec $run;
   if (`SELECT COUNT(*) FROM INFORMATION_SCHEMA.PROCESSLIST
        WHERE ID = @my_proclist_id + 1 AND TIME > 0`)
   {
      let $run= 0;
   }
   --real_sleep 0.2
}
# Either we have now reached TIME = 1 or we fail with unexpected result.
# Expect to see now TIME = 1
--replace_column 1 <ID>
SELECT * FROM INFORMATION_SCHEMA.PROCESSLIST;
--replace_column 1 <ID>
SHOW FULL PROCESSLIST;
# The second connection must have an ID = my ID + 1;
SELECT ID = @my_proclist_id + 1 FROM INFORMATION_SCHEMA.PROCESSLIST
WHERE USER = 'ddicttestuser1';


# Ensure that the user ddicttestuser1 sees only connections with his username
# because he has not the PROCESS privilege.
#----------------------------------------------------------------------------
--echo
--echo ----- switch to connection ddicttestuser1 (user = ddicttestuser1) -----
connection ddicttestuser1;
--replace_column 1 <ID> 6 <TIME>
SELECT * FROM INFORMATION_SCHEMA.PROCESSLIST;
--replace_column 1 <ID> 6 <TIME>
SHOW FULL PROCESSLIST;


# Ensure that the user ddicttestuser1 sees all connections with his username.
#----------------------------------------------------------------------------
--echo
--echo ----- establish connection con2 (user = ddicttestuser1) ------
connect (con2,localhost,ddicttestuser1,ddictpass,information_schema);
--replace_column 1 <ID> 6 <TIME>
SELECT * FROM INFORMATION_SCHEMA.PROCESSLIST;
--replace_column 1 <ID> 6 <TIME>
SHOW FULL PROCESSLIST;
#
--echo
--echo ----- switch to connection default (user = root) -----
connection default;
--echo ----- close connection con2 -----
disconnect con2;


# Ensure we see correct values if a connection is during work
#----------------------------------------------------------------------------
--echo
--echo ----- switch to connection ddicttestuser1 (user = ddicttestuser1) -----
connection ddicttestuser1;
# "Organise" a long running command to be observed by the root user
--echo
--echo
--echo # Send a long enough running statement to the server, but do not
--echo # wait till the result comes back. We will pull this later.
send SELECT sleep(2.5),'Command time';
#
--echo
--echo ----- switch to connection default (user = root) -----
connection default;
# Sleep a bit so that we can be nearly sure that we see the SELECT of ddicttestuser1.
# Expect to see within the processlist the other connection just during statement
# execution.
#   - USER = ddicttestuser1
#   - HOST = 'localhost'
#   - DB = 'information_schema'
#   - Command = 'Query'
#   - TIME = 1, Attention: check with TIME = 0 is not stable
#   - State IS NULL
#   - INFO = "SELECT sleep(2.5),'Command time'"
--echo # Sleep some time
# The command must be at some time in work by the server.
# So poll till INFO is no more NULL and TIME > 0.
let $run= 10;
while ($run)
{
   dec $run;
   if (`SELECT COUNT(*) FROM INFORMATION_SCHEMA.PROCESSLIST
        WHERE INFO IS NOT NULL AND TIME > 0`)
   {
      let $run= 0;
   }
   --real_sleep 0.2
}
# Expect to see TIME = 1
--replace_column 1 <ID>
SELECT * FROM INFORMATION_SCHEMA.PROCESSLIST;
--replace_column 1 <ID>
SHOW FULL PROCESSLIST;
#
--echo # Sleep some time
# The value of TIME must increase after some sleeps therefore
# poll till TIME changes
let $run= 10;
while ($run)
{
   dec $run;
   if (`SELECT COUNT(*) FROM INFORMATION_SCHEMA.PROCESSLIST
        WHERE ID = @my_proclist_id + 1 AND TIME > 1`)
   {
      let $run= 0;
   }
   --real_sleep 0.2
}
# Either we have now reached TIME = 2 or we fail with unexpected result.
--replace_column 1 <ID>
SELECT * FROM INFORMATION_SCHEMA.PROCESSLIST;
--replace_column 1 <ID>
SHOW FULL PROCESSLIST;
#
--echo
--echo ----- switch to connection ddicttestuser1 (user = ddicttestuser1) -----
connection ddicttestuser1;
--echo # Pull("reap") the result set from the statement executed with "send".
reap;


# Ensure that SHOW/SELECT processlist can handle extreme long commands
#----------------------------------------------------------------------------
--echo
--echo
--echo # Send a long (21 KB code and runtime = 2 seconds) statement to the server,
--echo # but do not wait till the result comes back. We will pull this later.
# Please do not change the next statement.
# The annoying long line is intended. Many short lines would be a different test.
send SELECT sleep(2),'BEGIN this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.this is the representative of a very long statement.END' AS "my_monster_statement";
#
--echo
--echo ----- switch to connection default (user = root) -----
connection default;
--echo # Sleep some time
# The command must be at some time in work by the server.
# So poll till INFO is no more NULL.
let $run= 10;
while ($run)
{
   dec $run;
   if (`SELECT COUNT(*) FROM INFORMATION_SCHEMA.PROCESSLIST
        WHERE INFO IS NOT NULL`)
   {
      let $run= 0;
   }
   --real_sleep 0.2
}
# Expect to see that SELECT/SHOW PROCESSLIST can handle my statement monster.
--replace_column 1 <ID> 6 <TIME>
SELECT * FROM INFORMATION_SCHEMA.PROCESSLIST;
--replace_column 1 <ID> 6 <TIME>
SHOW FULL PROCESSLIST;
# SHOW PROCESSLIST truncates INFO after 100 characters.
--replace_column 1 <ID> 6 <TIME>
SHOW PROCESSLIST;
--echo
--echo ----- switch to connection ddicttestuser1 (user = ddicttestuser1) -----
connection ddicttestuser1;
--echo # Pull("reap") the result set from the monster statement executed with "send".
reap;


# Ensure that we see that a connection "hangs" when colliding with a
# WRITE TABLE LOCK
#----------------------------------------------------------------------------
--echo
--echo ----- switch to connection default (user = root) -----
connection default;
LOCK TABLE test.t1 WRITE;
#
--echo
--echo ----- switch to connection ddicttestuser1 (user = ddicttestuser1) -----
connection ddicttestuser1;
--echo # Send a statement to the server, but do not wait till the result
--echo # comes back. We will pull this later.
send SELECT COUNT(*) FROM test.t1;
#
--echo
--echo ----- switch to connection default (user = root) -----
connection default;
--echo # Sleep some time
# The command must be at some time in work by the server.
# So poll till INFO is no more NULL.
let $run= 10;
while ($run)
{
   dec $run;
   if (`SELECT COUNT(*) FROM INFORMATION_SCHEMA.PROCESSLIST
        WHERE INFO IS NOT NULL`)
   {
      let $run= 0;
   }
   --real_sleep 0.2
}
# Expect to see the state 'Locked' for the second connection because the SELECT
# collides with the WRITE TABLE LOCK.
--replace_column 1 <ID> 6 <TIME>
SELECT * FROM INFORMATION_SCHEMA.PROCESSLIST;
--replace_column 1 <ID> 6 <TIME>
SHOW FULL PROCESSLIST;
UNLOCK TABLES;
#
--echo
--echo ----- switch to connection ddicttestuser1 (user = ddicttestuser1) -----
connection ddicttestuser1;
--echo # Pull("reap") the result set from the statement executed with "send".
reap;


# Cleanup
--echo
--echo ----- switch to connection default (user = root) -----
connection default;
--echo
--echo ----- close connection ddicttestuser1 -----
disconnect ddicttestuser1;
DROP USER ddicttestuser1@'localhost';
