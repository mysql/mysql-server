############ suite/funcs_1/datadict/processlist_priv.inc ###############
#                                                                      #
# Testing of privileges around                                         #
#     SELECT ... PROCESSLIST/SHOW PROCESSLIST                          #
#                                                                      #
# Note(mleich):                                                        #
#     There is a significant risk to get an unstable test because of   #
#     timing issues.                                                   #
#     Example1:                                                        #
#     1. Disconnect connection X                                       #
#     2. Switch to connection  Y                                       #
#     3. SHOW PROCESSLIST might present a record like                  #
#           <ID> <user> <host> <db> Quit 0 cleaning up NULL            #
#        or even a row where connection X is without                   #
#        "Quit" or "cleaning up".                                      #
#        That means our SHOW PROCESSLIST can come too early.           #
#     Solution:                                                        #
#        Close the connections at the end of the test.                 #
#     Example2 (2008-08-14 again observed):                            #
#     1. connection X: SHOW PROCESSLIST/GRANT ... etc.                 #
#     2. Switch to connection  Y                                       #
#     3. SHOW PROCESSLIST might present a record like                  #
#        <ID> <user> <host> <db> Query TIME cleaning up <command>      #
#        <ID> <user> <host> <db> Query TIME writing to net <command>   #
#        Problems happens more often in case of slow filesystem!       #
#     First Solution:                                                  #
#        Insert a dummy SQL command where the cleanup is most probably #
#        fast before switching to another connection and running       #
#        SHOW/SELECT PROCESSLIST.                                      #
#        Suppress writing to protocol by assignment to $variable.      #
#        let $my_var= `SELECT 1`;                                      #
#        Even the 'SELECT 1' was in some cases in state                #
#        "writing to net".                                             #
#     Final Solution:                                                  #
#        --sleep 0.3                                              #
#        This value was at least on my box sufficient.                 #
#        Please inform us if this test fails so that we can adjust     #
#        the sleep time better or switch to poll routines.             #
#                                                                      #
#     Storage engine variants of this test do not make sense.          #
#     -  I_S tables use the MEMORY storage engine whenever possible.   #
#     -  There are some I_S table which need column data types which   #
#        are not supported by MEMORY. Example: LONGTEXT/BLOB           #
#        MyISAM will be used for such tables.                          #
#        The column PROCESSLIST.INFO is of data type LONGTEXT          #
#           ----> MyISAM                                               #
#     -  There is no impact of the GLOBAL(server) or SESSION default   #
#        storage engine setting on the engine used for I_S tables.     #
#        That means we cannot get NDB or InnoDB instead.               #
#                                                                      #
# Creation:                                                            #
# 2007-08    hhunger Implement this test as part of                    #
#                    WL#3982 Test information_schema.processlist       #
#                                                                      #
# Last update:                                                         #
# 2008-08-14 mleich  Bug#38270 Test "processlist_priv_ps" fails on     #
#                              varying "processlist" output            #
#                    - Replace one sleep by a poll routines            #
#                    - Remove or disable superfluous sleeps            #
#                                                                      #
########################################################################

# The following variables are used in "datadict_priv.inc" and here.
#
# information_schema table to be tested
let $table= processlist;
#
# columns of the information_schema table e.g. to use in a select.
let $columns= ID, USER, HOST, DB, COMMAND, TIME, STATE, INFO;
#
# Where clause for an update.
let $update_where= WHERE id=1 ;
#
# Column to be used in the SET of an update.
let $set_column= user='any_user' ;
#
# Where clause of a delete.
let $delete_where= WHERE id=1 ;
#
# Column to be dropped.
let $drop_column= user;
#
# Column to be indexed
let $index_col= user;

USE information_schema;

--echo ####################################################################################
--echo 1 Prepare test.
--echo   connection default (user=root)
--echo ####################################################################################
if (`SELECT COUNT(*) <> 2 FROM processlist`)
{
   --echo This test expects one connection to the server.
   --echo Expectation: USER  HOST       DB                  COMMAND  STATE      INFO
   --echo Expectation: root  localhost  information_schema  Query    executing  SELECT USER,HOST,DB,COMMAND,STATE,INFO FROM processlist ORDER BY ID
   --echo But we found in the moment:
   SELECT USER,HOST,DB,COMMAND,STATE,INFO FROM processlist ORDER BY ID;
   --echo Maybe
   --echo - the base configuration (no of parallel auxiliary sessions) of the server has changed
   --echo - a parallel test intended for another server accidently connected to our current one
   --echo We cannot proceed in this situation. Abort
   exit;
}

--echo ####################################################################################
--echo 1.1 Create two user
--echo ####################################################################################
# access to info tables as normal user
--error 0, ER_CANNOT_USER
DROP USER ddicttestuser1@'localhost';
--error 0, ER_CANNOT_USER
DROP USER ddicttestuser2@'localhost';
CREATE USER ddicttestuser1@'localhost';
CREATE USER ddicttestuser2@'localhost';
SET PASSWORD FOR ddicttestuser1@'localhost' = 'ddictpass';
SET PASSWORD FOR ddicttestuser2@'localhost' = 'ddictpass';

--echo ####################################################################################
--echo 1.2 Establish connection con100 (user=ddicttestuser1 with no PROCESS privilege):
connect (con100,localhost,ddicttestuser1,ddictpass,information_schema);
--echo ####################################################################################


--echo ####################################################################################
--echo 2 connection default(user=root with default privileges):
--echo   SHOW/SELECT shows all processes/threads.
--echo ####################################################################################
connection default;
# Avoid Bug#38270 Test "processlist_priv_ps" fails on varying "processlist" output
#    This subtest expects that the connection con100 is in state 'Sleep'.
#    Poll till the connection con100 is in state COMMAND = 'Sleep'.
let $wait_timeout= 10;
let $wait_condition=
SELECT COUNT(*) = 3 FROM INFORMATION_SCHEMA.PROCESSLIST
WHERE DB = 'information_schema' AND COMMAND = 'Sleep' AND USER = 'ddicttestuser1';
--source include/wait_condition.inc
--replace_result InnoDB TMP_TABLE_ENGINE MyISAM TMP_TABLE_ENGINE 
eval SHOW CREATE TABLE $table;
--replace_column 1 ID 3 HOST_NAME 6 TIME
--replace_result "init" STATE "starting" STATE "cleaning up" STATE
--sorted_result
eval SHOW $table;
--replace_column 1 ID 3 HOST_NAME 6 TIME
--replace_result "init" STATE "starting" STATE "cleaning up" STATE
--sorted_result
eval SELECT * FROM $table $select_where ORDER BY id;
--replace_result "init" STATE "starting" STATE "cleaning up" STATE
--sorted_result
--replace_column 1 ID 3 HOST_NAME 6 TIME
eval SELECT $columns FROM $table $select_where ORDER BY id;
--source suite/funcs_1/datadict/datadict_priv.inc


--echo ####################################################################################
--echo 3 Switch to connection con100 (user=ddicttestuser1 with no PROCESS privilege):
connection con100;
--echo   SHOW/SELECT shows only the processes (1) of the user.
--echo ####################################################################################
# No need for poll routine here.
# The current state of the default session might depend on load of testing box
# but "ddicttestuser1" must not see anything of the root session.
--replace_result InnoDB TMP_TABLE_ENGINE MyISAM TMP_TABLE_ENGINE 
eval SHOW CREATE TABLE $table;
--replace_column 1 ID 3 HOST_NAME 6 TIME
--replace_result "init" STATE "starting" STATE "cleaning up" STATE
--sorted_result
eval SHOW $table;
--replace_column 1 ID 3 HOST_NAME 6 TIME
--replace_result "init" STATE "starting" STATE "cleaning up" STATE
--sorted_result
eval SELECT * FROM $table $select_where ORDER BY id;
--replace_column 1 ID 3 HOST_NAME 6 TIME
--replace_result "init" STATE "starting" STATE "cleaning up" STATE
--sorted_result
eval SELECT $columns FROM $table $select_where ORDER BY id;
--source suite/funcs_1/datadict/datadict_priv.inc
--sleep 0.3


--echo ####################################################################################
--echo 4 Grant PROCESS privilege to ddicttestuser1
--echo   connection default (user=root)
--echo ####################################################################################
connection default;
GRANT PROCESS ON *.* TO ddicttestuser1@'localhost';
--sleep 0.3

--echo ####################################################################################
--echo 4.1 Existing connection con100 (ddicttestuser1)
--echo     The user ddicttestuser1 has the PROCESS privilege, but the connection was
--echo     established before PROCESS was granted.
--echo     SHOW/SELECT shows only the processes (1) of the user.
--echo ####################################################################################
connection con100;
SHOW GRANTS;
--replace_column 1 ID 3 HOST_NAME 6 TIME
--replace_result "init" STATE "starting" STATE "cleaning up" STATE
--sorted_result
SHOW processlist;
--replace_column 1 ID 3 HOST_NAME 6 TIME
--replace_result "init" STATE "starting" STATE "cleaning up" STATE
--sorted_result
SELECT * FROM information_schema.processlist;
--sleep 0.3

--echo ####################################################################################
--echo 4.2 New connection con101 (ddicttestuser1 with PROCESS privilege)
--echo     SHOW/SELECT shows all processes/threads.
--echo ####################################################################################
connect (con101,localhost,ddicttestuser1,ddictpass,information_schema);
SHOW GRANTS;
--replace_column 1 ID 3 HOST_NAME 6 TIME
--replace_result "init" STATE "starting" STATE "cleaning up" STATE
--sorted_result
SHOW processlist;
--replace_column 1 ID 3 HOST_NAME 6 TIME
--replace_result "init" STATE "starting" STATE "cleaning up" STATE
--sorted_result
SELECT * FROM information_schema.processlist;
--sleep 0.3


--echo ####################################################################################
--echo 5 Grant PROCESS privilege to anonymous user.
--echo   connection default (user=root)
--echo ####################################################################################
connection default;
CREATE USER ''@'localhost';
GRANT PROCESS ON *.* TO ''@'localhost';
--sleep 0.3

--echo ####################################################################################
--echo 5.1 Establish connection (anonymous1,localhost,'',,information_schema)
--echo     anonymous user with PROCESS privilege
--echo     SHOW/SELECT shows all processes/threads.
--echo ####################################################################################
connect (anonymous1,localhost,"''",,information_schema);
SHOW GRANTS;
--replace_column 1 ID 3 HOST_NAME 6 TIME
--replace_result "init" STATE "starting" STATE "cleaning up" STATE
--sorted_result
SHOW processlist;
--replace_column 1 ID 3 HOST_NAME 6 TIME
--replace_result "init" STATE "starting" STATE "cleaning up" STATE
--sorted_result
SELECT * FROM information_schema.processlist;
--sleep 0.3


--echo ####################################################################################
--echo 6 Revoke PROCESS privilege from ddicttestuser1
--echo   connection default (user=root)
--echo ####################################################################################
connection default;
REVOKE PROCESS ON *.* FROM ddicttestuser1@'localhost';
--sleep 0.3

--echo ####################################################################################
--echo 6.1 New connection con102 (ddicttestuser1 has no more PROCESS privilege)
connect (con102,localhost,ddicttestuser1,ddictpass,information_schema);
--echo     Again (compared to state before GRANT PROCESS) only the processes of
--echo     ddicttestuser1 are visible.
--echo ####################################################################################
SHOW GRANTS;
--replace_column 1 ID 3 HOST_NAME 6 TIME
--replace_result "init" STATE "starting" STATE "cleaning up" STATE
--sorted_result
SHOW processlist;
--replace_column 1 ID 3 HOST_NAME 6 TIME
--replace_result "init" STATE "starting" STATE "cleaning up" STATE
--sorted_result
SELECT * FROM information_schema.processlist;
--sleep 0.3


--echo ####################################################################################
--echo 7 Revoke PROCESS privilege from anonymous user
--echo   connection default (user=root)
--echo ####################################################################################
connection default;
REVOKE PROCESS ON *.* FROM ''@'localhost';
--sleep 0.3

--echo ####################################################################################
--echo 7.1 New connection (anonymous2,localhost,'',,information_schema)
connect (anonymous2,localhost,"''",,information_schema);
--echo     The anonymous user has no more the PROCESS privilege
--echo     Again only the processes of the anonymous user are visible.
--echo ####################################################################################
SHOW GRANTS FOR ''@'localhost';
if ($fixed_bug_30395)
{
# Bug#30395 strange results after REVOKE PROCESS ON *.* FROM ...
--replace_column 1 ID 3 HOST_NAME 6 TIME
--replace_result "init" STATE "starting" STATE "cleaning up" STATE
--sorted_result
SHOW processlist;
}
--replace_column 1 ID 3 HOST_NAME 6 TIME
--replace_result "init" STATE "starting" STATE "cleaning up" STATE
--sorted_result
SELECT * FROM information_schema.processlist;
--sleep 0.3


--echo ####################################################################################
--echo 8 Grant SUPER (does not imply PROCESS) privilege to ddicttestuser1
--echo   connection default (user=root)
--echo ####################################################################################
connection default;
GRANT SUPER ON *.* TO 'ddicttestuser1'@'localhost';
--sleep 0.3

--echo ####################################################################################
--echo 8.1 New connection con103 (ddicttestuser1 with SUPER privilege)
connect (con103,localhost,ddicttestuser1,ddictpass,information_schema);
--echo     Only the processes of ddicttestuser1 user are visible.
--echo ####################################################################################
SHOW GRANTS FOR 'ddicttestuser1'@'localhost';
--replace_column 1 ID 3 HOST_NAME 6 TIME
--replace_result "init" STATE "starting" STATE "cleaning up" STATE
--sorted_result
SHOW processlist;
--replace_column 1 ID 3 HOST_NAME 6 TIME
--replace_result "init" STATE "starting" STATE "cleaning up" STATE
--sorted_result
SELECT * FROM information_schema.processlist;
--sleep 0.3


--echo ####################################################################################
--echo 9 Revoke SUPER privilege from user ddicttestuser1
--echo   connection default (user=root)
--echo ####################################################################################
connection default;
REVOKE SUPER ON *.* FROM 'ddicttestuser1'@'localhost';
--sleep 0.3

--echo ####################################################################################
--echo 9.1 New connection con104 (ddicttestuser1 without SUPER privilege)
connect (con104,localhost,ddicttestuser1,ddictpass,information_schema);
--echo     ddicttestuser1 has no more the SUPER privilege.
--echo     Only the processes of ddicttestuser1 are visible.
--echo ####################################################################################
SHOW GRANTS FOR 'ddicttestuser1'@'localhost';
--replace_column 1 ID 3 HOST_NAME 6 TIME
--replace_result "init" STATE "starting" STATE "cleaning up" STATE
--sorted_result
SHOW processlist;
--replace_column 1 ID 3 HOST_NAME 6 TIME
--replace_result "init" STATE "starting" STATE "cleaning up" STATE
--sorted_result
SELECT * FROM information_schema.processlist;
--sleep 0.3


--echo ####################################################################################
--echo 10 Grant SUPER privilege with grant option to user ddicttestuser1.
--echo   connection default (user=root)
--echo ####################################################################################
connection default;
GRANT SUPER ON *.* TO 'ddicttestuser1'@'localhost' WITH GRANT OPTION;
--sleep 0.3

--echo ####################################################################################
--echo 10.1 New connection con105 (ddicttestuser1 with SUPER privilege and GRANT OPTION)
connect (con105,localhost,ddicttestuser1,ddictpass,information_schema);
--echo      Try to grant PROCESS privilege to user ddicttestuser2 without having it.
--echo ####################################################################################
SHOW GRANTS FOR 'ddicttestuser1'@'localhost';
--error ER_ACCESS_DENIED_ERROR
GRANT PROCESS ON *.* TO 'ddicttestuser2'@'localhost';

--echo ####################################################################################
--echo 10.2 Grant SUPER and PROCESS privilege with grant option to user ddicttestuser1
--echo   connection default (user=root)
--echo ####################################################################################
connection default;
GRANT SUPER,PROCESS ON *.* TO 'ddicttestuser1'@'localhost' WITH GRANT OPTION;
--sleep 0.3

--echo ####################################################################################
--echo 10.3 New connection con106 (ddicttestuser1 with SUPER,PROCESS WITH GRANT OPTION)
connect (con106,localhost,ddicttestuser1,ddictpass,information_schema);
--echo      Grant PROCESS privilege to user ddicttestuser2
--echo ####################################################################################
SHOW GRANTS FOR 'ddicttestuser1'@'localhost';
GRANT PROCESS ON *.* TO 'ddicttestuser2'@'localhost';
--sleep 0.3

--echo ####################################################################################
--echo 10.4 New connection con200 (ddicttestuser2 with PROCESS privilege)
connect (con200,localhost,ddicttestuser2,ddictpass,information_schema);
--echo      ddicttestuser2 has now the PROCESS privilege and sees all connections
--echo ####################################################################################
SHOW GRANTS FOR 'ddicttestuser2'@'localhost';
--replace_column 1 ID 3 HOST_NAME 6 TIME
--replace_result "init" STATE "starting" STATE "cleaning up" STATE
--sorted_result
SHOW processlist;
--replace_column 1 ID 3 HOST_NAME 6 TIME
--replace_result "init" STATE "starting" STATE "cleaning up" STATE
--sorted_result
SELECT * FROM information_schema.processlist;
--sleep 0.3


--echo ####################################################################################
--echo 11 User ddicttestuser1 revokes PROCESS privilege from user ddicttestuser2
--echo    connection ddicttestuser1;
--echo ####################################################################################
connection con106;
REVOKE PROCESS ON *.* FROM 'ddicttestuser2'@'localhost';
--sleep 0.3

--echo ####################################################################################
--echo 11.1 New connection con201 (ddicttestuser2)
connect (con201,localhost,ddicttestuser2,ddictpass,information_schema);
--echo      ddicttestuser2 has no more the PROCESS privilege and can only see own connects
--echo ####################################################################################
SHOW GRANTS;
--replace_column 1 ID 3 HOST_NAME 6 TIME
--replace_result "init" STATE "starting" STATE "cleaning up" STATE
--sorted_result
SHOW processlist;
--replace_column 1 ID 3 HOST_NAME 6 TIME
--replace_result "init" STATE "starting" STATE "cleaning up" STATE
--sorted_result
SELECT * FROM information_schema.processlist;
--sleep 0.3

--echo ####################################################################################
--echo 11.2 Revoke SUPER,PROCESS,GRANT OPTION privilege from user ddicttestuser1
--echo      connection default (user=root)
--echo ####################################################################################
connection default;
REVOKE SUPER,PROCESS,GRANT OPTION ON *.* FROM 'ddicttestuser1'@'localhost';
--sleep 0.3

--echo ####################################################################################
--echo 11.3 New connection con107 (ddicttestuser1)
connect (con107,localhost,ddicttestuser1,ddictpass,information_schema);
--echo      ddicttestuser1 has no more the PROCESS privilege and can only see own connects
--echo      He is also unable to GRANT the PROCESS privilege to ddicttestuser2
--echo ####################################################################################
SHOW GRANTS FOR 'ddicttestuser1'@'localhost';
--error ER_ACCESS_DENIED_ERROR
GRANT PROCESS ON *.* TO 'ddicttestuser2'@'localhost';
--replace_column 1 ID 3 HOST_NAME 6 TIME
--replace_result "init" STATE "starting" STATE "cleaning up" STATE
--sorted_result
SHOW processlist;
--replace_column 1 ID 3 HOST_NAME 6 TIME
--replace_result "init" STATE "starting" STATE "cleaning up" STATE
--sorted_result
SELECT * FROM information_schema.processlist;
--sleep 0.3


--echo ####################################################################################
--echo 12 Revoke the SELECT privilege from user ddicttestuser1
--echo    connection default (user=root)
--echo ####################################################################################
connection default;
REVOKE SELECT ON *.* FROM 'ddicttestuser1'@'localhost';
--sleep 0.3

--echo ####################################################################################
--echo 12.1 New connection con108 (ddicttestuser1)
connect (con108,localhost,ddicttestuser1,ddictpass,information_schema);
--echo      ddicttestuser1 has neither PROCESS nor SELECT privilege
--echo      Manual says: Each MySQL user has the right to access these tables, but can see
--echo                   only the rows ...
--echo      Therefore the missing SELECT privilege does not affect SELECTs on PROCESSLIST.
--echo ####################################################################################
SHOW GRANTS FOR 'ddicttestuser1'@'localhost';
--replace_column 1 ID 3 HOST_NAME 6 TIME
--replace_result "init" STATE "starting" STATE "cleaning up" STATE
--sorted_result
SHOW processlist;
--replace_column 1 ID 3 HOST_NAME 6 TIME
--replace_result "init" STATE "starting" STATE "cleaning up" STATE
--sorted_result
SELECT * FROM information_schema.processlist;
--sleep 0.3

--echo ####################################################################################
--echo 12.2 Revoke only the SELECT privilege on the information_schema from ddicttestuser1.
--echo      connection default (user=root)
--echo ####################################################################################
connection default;
--error ER_DBACCESS_DENIED_ERROR
REVOKE SELECT ON information_schema.* FROM 'ddicttestuser3'@'localhost';
--sleep 0.3


--echo ####################################################################################
--echo    connection default (user=root)
--echo    Cleanup: close connections, DROP USER etc.
--echo ####################################################################################
connection default;
disconnect con100;
disconnect con101;
disconnect con102;
disconnect con103;
disconnect con104;
disconnect con105;
disconnect con106;
disconnect con107;
disconnect con108;
disconnect con200;
disconnect con201;
disconnect anonymous1;
disconnect anonymous2;
DROP USER ddicttestuser1@'localhost';
DROP USER ddicttestuser2@'localhost';
REVOKE USAGE ON *.* FROM ''@'localhost';
DROP USER ''@'localhost';
