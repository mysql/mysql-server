# Include file for testing that concurrent alter changes are/are not
# detected.
# Inputs :
#   $alter_statement
#   $expected_alter_error
#   $other_statement
#   $expected_other_error
#
--echo Checking copying alter $alter_statement
--echo   concurrent with $other_statement 
--echo   has alter error $expected_alter_error
--echo   and statement error $expected_other_error

--connection server1

# See ha_ndbcluster.cc for definition of this sync point
# Get sync point to signal 'alter_waiting' and wait for signal 'alter_go'
set DEBUG_SYNC= 'ndb.before_commit_count_check SIGNAL alter_waiting WAIT_FOR alter_go';

--send_eval $alter_statement

--connection server1con2

#--echo Waiting for ALTER to stall
# wait for alter_waiting
set DEBUG_SYNC= 'now WAIT_FOR alter_waiting';
#--echo Alter stalled

--connection server2
#--echo Executing concurrent query
--error $expected_other_error
--eval $other_statement

--connection server1con2
#--echo Resuming alter
# signal 'alter_go' to resume the alter
set DEBUG_SYNC= 'now SIGNAL alter_go';

--connection server1
#--echo Checking result

--error $expected_alter_error
--reap

set DEBUG_SYNC='RESET';

