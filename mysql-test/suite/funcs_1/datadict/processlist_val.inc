########### suite/funcs_1/datadict/processlist_val.inc #########################
#                                                                              #
# Testing of values within INFORMATION_SCHEMA.PROCESSLIST                      #
#                                                                              #
# Ensure that the values fit to the current state of the connection and        #
# especially that they change if a connection does nothing or runs some SQL.   #
# runs some SQL.                                                               #
# Examples:                                                                    #
# - change the default database                                                #
# - send some period of time no SQL command to the server                      #
# - send a long running query                                                  #
#                                                                              #
# Note(mleich):                                                                #
# 1. Please inform me if this test fails because of timing problems.           #
# 2. Storage engine variants of this test do not make sense.                   #
#    -  I_S tables use the MEMORY storage engine whenever possible.            #
#    -  There are some I_S tables which need column data types which are not   #
#       supported by MEMORY. Example: LONGTEXT/BLOB                            #
#       MyISAM will be used for such tables.                                   #
#       The column PROCESSLIST.INFO is of data type LONGTEXT ----> MyISAM      #
#    -  There is no impact of the GLOBAL(server) or SESSION default storage    #
#       engine setting on the engine used for I_S tables.                      #
#       That means we cannot get NDB or InnoDB instead.                        #
# 3. The SHOW (FULL) PROCESSLIST command are for comparison.                   #
#    The main test target is INFORMATION_SCHEMA.PROCESSLIST !                  #
# 4. Attention:                                                                #
#    The values of the PROCESSLIST columns HOST and TIME tend to cause         #
#    problems and therefore their printing has to be suppressed.               #
#    Examples of the exact values:                                             #
#    HOST: 'localhost' (UNIX derivates)                                        #
#          'localhost:<varying_port>' (WINDOWS)                                #
#    TIME: In many cases within this test 0 seconds but if the testing box is  #
#          overloaded we might get up to 2 seconds.                            #
#    Solution:                                                                 #
#          --replace_column 1 <ID> 3 <HOST_NAME> 6 <TIME>                      #
# 5. How to debug the script?                                                  #
#    <graphical diff tool>                          \                          #
#        suite/funcs_1/datadict/processlist_val.inc \                          #
#        <Result|Reject|Log file>                                              #
#    I tweaked a lot of the script lines around "echo" so that you will get a  #
#    lot of useful synchronisation.                                            #
#                                                                              #
# Creation:                                                                    #
# 2007-08-09 mleich Implement this test as part of                             #
#                   WL#3982 Test information_schema.processlist                #
#                                                                              #
# Last Modification:                                                           #
# 2008-07-04 mleich Fix for                                                    #
#                   Bug#37853 Test "funcs_1.processlist_val_ps" fails in       #
#                             various ways                                     #
#                       - issues with printing of port (Win only)              #
#                       - too optimistic assumptions about timing              #
#                   + corrections of logic in poll routines                    #
#                   + minor improvements                                       #
################################################################################

# Basic preparations
--error 0, ER_CANNOT_USER
DROP USER test_user@'localhost';
CREATE USER test_user@'localhost';
GRANT ALL ON *.* TO test_user@'localhost';
REVOKE PROCESS ON *.* FROM test_user@'localhost';
SET PASSWORD FOR test_user@'localhost' = 'ddictpass';
--disable_warnings
DROP TABLE IF EXISTS test.t1;
--enable_warnings
CREATE TABLE test.t1 (f1 BIGINT);
USE test;


echo
# Show the definition of the PROCESSLIST table
#--------------------------------------------------------------------------
;
--replace_result InnoDB TMP_TABLE_ENGINE MyISAM TMP_TABLE_ENGINE 
SHOW CREATE TABLE INFORMATION_SCHEMA.PROCESSLIST;

echo
# Ensure that the information about the own connection is correct.
#--------------------------------------------------------------------------
;
# Expected values
#   - USER = 'root'
#   - HOST (printed value is unified), the exact values are often like
#        UNIX: 'localhost'
#        WIN: 'localhost:<some port>'
#   - DB = 'test'
#   - Command IN (no protocol -> 'Query', ps-protocol -> 'Execute')
#   - TIME (printed value will be unified), the exact values are like
#          "normal" load: 0 (seconds)
#          "heavy" load: 1 or more (seconds)
#   - State 'executing'
#   - INFO must contain the corresponding SHOW/SELECT PROCESSLIST
#
# 1. Just dump what we get
--replace_column 1 <ID> 3 <HOST_NAME> 6 <TIME>
--replace_result "init" STATE "starting" STATE "cleaning up" STATE
--sorted_result
SELECT * FROM INFORMATION_SCHEMA.PROCESSLIST;
--replace_column 1 <ID> 3 <HOST_NAME> 6 <TIME>
--replace_result "init" STATE "starting" STATE "cleaning up" STATE
--sorted_result
SHOW FULL PROCESSLIST;
#
# Determine the connection id of the current connection (default)
SET @default_id = CONNECTION_ID();
#
# 2. There must be exact one connection with @default_id;
SELECT COUNT(*)  = 1 AS "Expect exact one connection with this id"
FROM INFORMATION_SCHEMA.PROCESSLIST WHERE ID = @default_id;
#
# 3. Check the remaining stuff
SELECT COUNT(*)  = 1 AS "Expect 1"
FROM INFORMATION_SCHEMA.PROCESSLIST WHERE ID = @default_id
     AND USER = 'root' AND DB = 'test' AND Command IN('Query','Execute')
     AND State = 'executing';
#
# 4. Change the DB
USE information_schema;
SELECT COUNT(*)  = 1 AS "Is the DB correct?"
FROM INFORMATION_SCHEMA.PROCESSLIST
WHERE ID = @default_id AND DB = 'information_schema';
#
# 5. Change the statement
let $my_statement =
SELECT @my_info := INFO FROM INFORMATION_SCHEMA.PROCESSLIST
WHERE ID = @default_id;
eval $my_statement;
eval
SELECT @my_info = '$my_statement'
    AS 'Is the content of PROCESSLIST.INFO correct?';
#
# 6. TIME must have a reasonable value
SELECT COUNT(*) = 1 AS "Has TIME a reasonable value?"
FROM INFORMATION_SCHEMA.PROCESSLIST
WHERE ID = @default_id AND 0 <= TIME < 10;


echo
# Ensure that the information about an inactive connection is correct.
#--------------------------------------------------------------------------
;
echo
# ----- establish connection con1 (user = test_user) -----
;
connect (con1,localhost,test_user,ddictpass,information_schema);
#
echo
# ----- switch to connection default (user = root) -----
;
connection default;
# We have now a second connection.
# First working phase for the new connection is with Command = 'Connect'.
# This is a very short phase and the likelihood to meet it is
# - nearly zero on average boxes with low parallel load
# - around some percent on weak or overloaded boxes
#   (Bug#32153 Status output differs - scheduling ?)
# Therefore we do not try to catch this state.
# We poll till we reach the long lasting phase with Command = 'Sleep'.
#   - USER = 'test_user'
#   - DB = 'information_schema'
#   - Command = 'Sleep'
#   - State is empty
#   - INFO IS NULL
echo
# Poll till the connection con1 is in state COMMAND = 'Sleep'.
;
let $wait_timeout= 10;
let $wait_condition= SELECT COUNT(*) = 1 FROM INFORMATION_SCHEMA.PROCESSLIST
                     WHERE COMMAND = 'Sleep' AND USER = 'test_user';
--source include/wait_condition.inc
# 1. Just dump what we get
--replace_column 1 <ID> 3 <HOST_NAME> 6 <TIME>
--replace_result "init" STATE "starting" STATE "cleaning up" STATE
--sorted_result
SELECT * FROM INFORMATION_SCHEMA.PROCESSLIST;
--replace_column 1 <ID> 3 <HOST_NAME> 6 <TIME>
--replace_result "init" STATE "starting" STATE "cleaning up" STATE
--sorted_result
SHOW FULL PROCESSLIST;
#
# Pull ID and TIME of the second connection
SELECT ID,TIME INTO @test_user_con1_id,@time FROM INFORMATION_SCHEMA.PROCESSLIST
WHERE COMMAND = 'Sleep' AND USER = 'test_user';
#
# 2. The second connection must (behaviour at least since 2007) have an
#    ID = ID_of_previous_connection + 1
SELECT @test_user_con1_id = @default_id + 1
    AS "Did we got the next higher PROCESSLIST ID?";
#
# 3. TIME must have a reasonable value
SELECT 0 <=  @time < 10 AS "Has TIME a reasonable value?";
#
# 4. HOST must be for both connections similar (varying port on Win)
SELECT COUNT(*) = 3 AS "Is HOST LIKE 'localhost%'?"
FROM INFORMATION_SCHEMA.PROCESSLIST
WHERE HOST LIKE 'localhost%';
#
# 5. Check the remaining stuff
SELECT COUNT(*)  = 1 AS "Expect 1"
FROM INFORMATION_SCHEMA.PROCESSLIST WHERE ID = @test_user_con1_id
     AND USER = 'test_user' AND DB = 'information_schema'
     AND Command = 'Sleep' AND State = '' AND INFO IS NULL;
#
# 6. Check that TIME increases
let $wait_timeout= 10;
let $wait_condition= SELECT COUNT(*) = 1 FROM INFORMATION_SCHEMA.PROCESSLIST
                     WHERE COMMAND = 'Sleep' AND USER = 'test_user'
                       AND TIME > @time;
--source include/wait_condition.inc


echo
# Ensure that the user test_user sees only connections with his username
# because he has not the PROCESS privilege.
#----------------------------------------------------------------------------
;
echo
# ----- switch to connection con1 (user = test_user) -----
;
connection con1;
--replace_column 1 <ID> 3 <HOST_NAME> 6 <TIME>
--replace_result "init" STATE "starting" STATE "cleaning up" STATE
--sorted_result
SELECT * FROM INFORMATION_SCHEMA.PROCESSLIST;
--replace_column 1 <ID> 3 <HOST_NAME> 6 <TIME>
--replace_result "init" STATE "starting" STATE "cleaning up" STATE
--sorted_result
SHOW FULL PROCESSLIST;


echo
# Ensure that the user test_user sees all connections with his username.
#----------------------------------------------------------------------------
;
echo
----- establish connection con2 (user = test_user) ------
;
connect (con2,localhost,test_user,ddictpass,information_schema);
echo
# ----- switch to connection default (user = root) -----
;
connection default;
# If the testing box is under heavy load we might see within some of the
# next queries connection
# con2 with Command = 'Connect'
# con1 with INFO = 'SHOW FULL PROCESSLIST' and STATE = 'Writing to net'
# Both phases are too short to be checked.
echo
# Poll till all connections of 'test_user' are in a state with COMMAND = 'Sleep'
;
let $wait_condition= SELECT COUNT(*) = 2 FROM INFORMATION_SCHEMA.PROCESSLIST
        WHERE USER = 'test_user' AND COMMAND = 'Sleep';
--source include/wait_condition.inc
echo
# ----- switch to connection con2 (user = test_user) -----
;
connection con2;
# Just dump what we get
--replace_column 1 <ID> 3 <HOST_NAME> 6 <TIME>
--replace_result "init" STATE "starting" STATE "cleaning up" STATE
--sorted_result
SELECT * FROM INFORMATION_SCHEMA.PROCESSLIST;
--replace_column 1 <ID> 3 <HOST_NAME> 6 <TIME>
--replace_result "init" STATE "starting" STATE "cleaning up" STATE
--sorted_result
SHOW FULL PROCESSLIST;
#
echo
# ----- switch to connection default (user = root) -----
;
connection default;
# Pull the ID of con2, we will need it later
SELECT ID INTO @test_user_con2_id FROM INFORMATION_SCHEMA.PROCESSLIST
WHERE ID <> @test_user_con1_id
  AND USER = 'test_user' AND DB = 'information_schema';


echo
# Ensure we get correct information about a connection during work
#----------------------------------------------------------------------------
;
echo
# ----- switch to connection con2 (user = test_user) -----
;
connection con2;
# "Organise" a long running command to be observed by the root user.
echo
# Send a long enough running statement to the server, but do not
# wait till the result comes back.
;
# Worst case scenario (=high I/O load on testing box):
# - My experience:
#   Up to 2 seconds runtime per SELECT ... FROM INFORMATION_SCHEMA.PROCESSLIST
#   in rare cases.
# - The following sequence contains ~ 4 of such SELECTs
# Therefore we sleep 10 seconds.
let $sleep_command =
SELECT sleep(10), 17;
send;
eval $sleep_command;
#
echo
# ----- switch to connection default (user = root) -----
;
connection default;
echo
# Poll till connection con2 is in state 'User sleep'.
;
# Expect to see within the processlist the other connection just during
# statement execution.
#   - USER = 'test_user'
#   - DB = 'information_schema'
#   - Command = 'Query'(run without --ps-protocol)/'Execute' (run --ps-protocol)
#   - TIME >= 0
#   - State = 'User sleep'
#   - INFO = $sleep_command
let $wait_condition=
SELECT COUNT(*) FROM INFORMATION_SCHEMA.PROCESSLIST
WHERE ID = @test_user_con2_id AND Command IN('Query','Execute')
  AND State = 'User sleep' AND INFO IS NOT NULL ;
--source include/wait_condition.inc
# 1. Just dump what we get
--replace_column 1 <ID> 3 <HOST_NAME> 6 <TIME>
--replace_result "init" STATE "starting" STATE "cleaning up" STATE
--sorted_result
SELECT * FROM INFORMATION_SCHEMA.PROCESSLIST;
--replace_column 1 <ID> 3 <HOST_NAME> 6 <TIME>
--replace_result "init" STATE "starting" STATE "cleaning up" STATE
--sorted_result
SHOW FULL PROCESSLIST;
#
# Pull some information about the connection con2
SELECT STATE, TIME, INFO INTO @state, @time, @info
FROM INFORMATION_SCHEMA.PROCESSLIST
WHERE ID = @test_user_con2_id;
# 2. TIME must have a reasonable value
SELECT 0 <= @time < 10 AS "Has TIME a reasonable value?";
# 3. STATE must be 'User sleep'
SELECT @state = 'User sleep' AS "Has STATE the expected value?";
# 4. INFO must fit
eval SELECT @info = '$sleep_command' AS "Has INFO the expected value?";
# 5. Check that TIME increases
let $wait_timeout= 10;
let $wait_condition= SELECT COUNT(*) = 1 FROM INFORMATION_SCHEMA.PROCESSLIST
    WHERE ID = @test_user_con2_id AND INFO IS NOT NULL AND TIME > @time;
--source include/wait_condition.inc
echo
# ----- switch to connection con2 (user = testuser) -----
;
connection con2;
echo
# Pull("reap") the result set from the statement executed with "send".
;
reap;
echo
# ----- switch to connection default (user = root) -----
;
connection default;
echo
# Poll till all connections of 'test_user' are in a state with COMMAND = 'Sleep'
;
let $wait_timeout= 10;
let $wait_condition= SELECT COUNT(*) = 2 FROM INFORMATION_SCHEMA.PROCESSLIST
                     WHERE COMMAND = 'Sleep' AND USER = 'test_user';
--source include/wait_condition.inc


echo
# Ensure that we see that a connection "hangs" when colliding with a
# WRITE TABLE LOCK
#----------------------------------------------------------------------------
;
LOCK TABLE test.t1 WRITE;
#
echo
# ----- switch to connection con2 (user = test_user) -----
;
connection con2;
echo
# Send a statement to the server, but do not wait till the result
# comes back. We will pull this later.
;
send
SELECT COUNT(*) FROM test.t1;
echo
# ----- switch to connection default (user = root) -----
;
connection default;
echo
# Poll till INFO is no more NULL and State = 'Waiting for table metadata lock'.
;
let $wait_condition= SELECT COUNT(*) FROM INFORMATION_SCHEMA.PROCESSLIST
        WHERE INFO IS NOT NULL AND STATE = 'Waiting for table metadata lock';
--source include/wait_condition.inc
#
# Expect to see the state 'Waiting for table metadata lock' for the third
# connection because the SELECT collides with the WRITE TABLE LOCK.
--replace_column 1 <ID> 3 <HOST_NAME> 6 <TIME>
--replace_result "init" STATE "starting" STATE "cleaning up" STATE
--sorted_result
SELECT * FROM INFORMATION_SCHEMA.PROCESSLIST;
UNLOCK TABLES;
#
echo
# ----- switch to connection con2 (user = test_user) -----
;
connection con2;
echo
# Pull("reap") the result set from the statement executed with "send".
;
reap;


echo
# Ensure that SHOW/SELECT processlist can handle extreme long commands
#----------------------------------------------------------------------------
;
# We do not want to waste runtime, therefore we run the following test based
# on "Lock collision" and not with some "sleep(10)".
echo
# ----- switch to connection default (user = root) -----
;
connection default;
LOCK TABLE test.t1 WRITE;
#
echo
# ----- switch to connection con2 (user = test_user) -----
;
connection con2;
echo
# Send a long (~20 KB code) statement to the server, but do not wait
# till the result comes back. We will pull this later.
;
let $string=
`SELECT CONCAT('BEGIN-',
        REPEAT('This is the representative of a very long statement.',400),
        '-END')`;
let $my_statement =
SELECT count(*),'$string' AS "Long string" FROM test.t1;
send;
eval $my_statement;
echo
# ----- switch to connection default (user = root) -----
;
connection default;
echo
# Poll till INFO is no more NULL and State = 'Waiting for table metadata lock'.
;
let $wait_condition= SELECT COUNT(*) FROM INFORMATION_SCHEMA.PROCESSLIST
        WHERE INFO IS NOT NULL AND STATE = 'Waiting for table metadata lock';
--source include/wait_condition.inc
echo
# Expect result:
# Statement                                      Content of INFO
# SELECT * FROM INFORMATION_SCHEMA.PROCESSLIST   Complete statement
# SHOW FULL PROCESSLIST                          Complete statement
# SHOW PROCESSLIST                               statement truncated after 100 char
;
--replace_column 1 <ID> 3 <HOST_NAME> 5 <COMMAND> 6 <TIME> 7 <STATE>
--replace_result "init" STATE "starting" STATE "cleaning up" STATE
--sorted_result
SELECT * FROM INFORMATION_SCHEMA.PROCESSLIST;
--replace_column 1 <ID> 3 <HOST_NAME> 5 <COMMAND> 6 <TIME> 7 <STATE>
--replace_result "init" STATE "starting" STATE "cleaning up" STATE
--sorted_result
SHOW FULL PROCESSLIST;
--replace_column 1 <ID> 3 <HOST_NAME> 5 <COMMAND> 6 <TIME> 7 <STATE>
--replace_result "init" STATE "starting" STATE "cleaning up" STATE
--sorted_result
SHOW PROCESSLIST;
UNLOCK TABLES;
echo
# ----- switch to connection con2 (user = test_user) -----
;
connection con2;
echo
# Pull("reap") the result set from the monster statement executed with "send".
;
reap;


# Cleanup
echo
# ----- switch to connection default (user = root) -----
;
connection default;
echo
----- disconnect con1 and con2 -----
;
disconnect con1;
disconnect con2;
DROP USER test_user@'localhost';
DROP TABLE test.t1;
