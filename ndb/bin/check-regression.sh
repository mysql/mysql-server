#!/bin/sh
# NAME
#   check-regression.sh
# 
# SYNOPSIS
#   check-regression.sh
#
# DESCRIPTION
#
# This scrip must be run before any major cvs checkins are done.
# It will perform a number of regression tests to check that 
# nothing is broken.
#
# OPTIONS
#
# EXAMPLES
#   
#   
# ENVIRONMENT
#   NDB_PROJ_HOME       Home dir for ndb
#   verbose             verbose printouts
#
# FILES
#   $NDB_PROJ_HOME/lib/funcs.sh  general shell script functions
#
#
# SEE ALSO
#   
# DIAGNOSTICTS
#   
#   
# VERSION   
#   1.0
#   
# AUTHOR
#   
#

. $NDB_PROJ_HOME/lib/funcs.sh    # Load some good stuff

synopsis="check-regression.sh"
progname=`basename $0`

numOfTestsOK=0
numOfTestsFailed=0

LOG=check-regression.`date '+%Y-%m-%d'`

executeTest()
{
    eval "$@" | tee -a $LOG

    if [ $? -eq 0 ] 
    then
	echo "SUCCESS: $@"
	numOfTestsOK=`expr $numOfTestsOK + 1`
    else
	echo "FAILED: $@"
	numOfTestsFailed=`expr $numOfTestsFailed + 1`
    fi
}

#
# INFO 
#
trace "Starting: `date`"
trace "NDB_PROJ_HOME = $NDB_PROJ_HOME"
trace "NDB_TOP = $NDB_TOP"

#
# THE TESTS TO EXECUTE
#

# Testsuite: testDataBuffers
# Number of tests: 1
executeTest 'drop_tab ' TB00 TB01 TB02 TB03 TB04 TB05 TB06 TB07 TB08 TB09 TB10 TB11 TB12 TB13 TB14 TB15
executeTest 'testDataBuffers'
executeTest 'drop_tab ' TB00 TB01 TB02 TB03 TB04 TB05 TB06 TB07 TB08 TB09 TB10 TB11 TB12 TB13 TB14 TB15

TABLES="T9 T13"

# Testsuite: testBasic
# Number of tests: 16
executeTest 'testBasic -n PkInsert' $TABLES
executeTest 'testBasic -n PkRead' $TABLES
executeTest 'testBasic -n PkUpdate' $TABLES
executeTest 'testBasic -n PkDelete' $TABLES
#executeTest 'testBasic -n UpdateAndRead' 
#executeTest 'testBasic -n PkReadAndLocker'
#executeTest 'testBasic -n PkReadAndLocker2'
#executeTest 'testBasic -n PkReadUpdateAndLocker'
#executeTest 'testBasic -n ReadWithLocksAndInserts'
#executeTest 'testBasic -n ReadConsistency'
#executeTest 'testBasic -n PkInsertTwice'
#executeTest 'testBasic -n Fill'
#executeTest 'testBasic -n FillTwice'
#executeTest 'testBasic -n NoCommitSleep'
#executeTest 'testBasic -n NoCommit626'
#executeTest 'testBasic -n NoCommitAndClose'

# Testsuite: testBasicAsynch
# Number of tests: 4
executeTest 'testBasicAsynch -n PkInsertAsynch' $TABLES
executeTest 'testBasicAsynch -n PkReadAsynch' $TABLES
executeTest 'testBasicAsynch -n PkUpdateAsynch' $TABLES
executeTest 'testBasicAsynch -n PkDeleteAsynch' $TABLES

# Testsuite: testDict
# Number of tests: 6
#executeTest 'testDict -n CreateAndDrop'
#executeTest 'testDict -n CreateAndDropWithData'
#executeTest 'testDict -n CreateAndDropDuring'
#executeTest 'testDict -n CreateInvalidTables'
#executeTest 'testDict -n CreateTableWhenDbIsFull'
#executeTest 'testDict -n CreateMaxTables'

# Testsuite: testScan
# Number of tests: 34
#executeTest 'testScan -n ScanRead'
#executeTest 'testScan -n ScanRead16'
executeTest 'testScan -n ScanRead240' $TABLES
executeTest 'testScan -n ScanUpdate' $TABLES
executeTest 'testScan -n ScanUpdate2' $TABLES
executeTest 'testScan -n ScanDelete' $TABLES
executeTest 'testScan -n ScanDelete2' $TABLES
#executeTest 'testScan -n ScanUpdateAndScanRead'
#executeTest 'testScan -n ScanReadAndLocker'
#executeTest 'testScan -n ScanReadAndPkRead'
#executeTest 'testScan -n ScanRead488'
#executeTest 'testScan -n ScanWithLocksAndInserts'
#executeTest 'testScan -n ScanReadAbort'
#executeTest 'testScan -n ScanReadAbort15'
#executeTest 'testScan -n ScanReadAbort16'
#executeTest 'testScan -n ScanUpdateAbort16'
#executeTest 'testScan -n ScanReadAbort240'
#executeTest 'testScan -n ScanReadRestart'
#executeTest 'testScan -n ScanReadRestart16'
#executeTest 'testScan -n ScanReadRestart32'
#executeTest 'testScan -n ScanUpdateRestart'
#executeTest 'testScan -n ScanUpdateRestart16'
#executeTest 'testScan -n CheckGetValue'
#executeTest 'testScan -n CloseWithoutStop'
#executeTest 'testScan -n NextScanWhenNoMore'
#executeTest 'testScan -n ExecuteScanWithoutOpenScan'
#executeTest 'testScan -n OnlyOpenScanOnce'
#executeTest 'testScan -n OnlyOneOpInScanTrans'
#executeTest 'testScan -n OnlyOneOpBeforeOpenScan'
#executeTest 'testScan -n OnlyOneScanPerTrans'
#executeTest 'testScan -n NoCloseTransaction'
#executeTest 'testScan -n CheckInactivityTimeOut'
#executeTest 'testScan -n CheckInactivityBeforeClose'
#executeTest 'testScan -n CheckAfterTerror'

# Testsuite: testScanInterpreter
# Number of tests: 1
#executeTest 'testScanInterpreter -n ScanLessThan'

TABLES="T6 T13"

# Testsuite: testSystemRestart
# Number of tests: 4
executeTest 'testSystemRestart -l 1 -n SR1' $TABLES
executeTest 'testSystemRestart -l 1 -n SR2' $TABLES
#executeTest 'testSystemRestart -n SR_UNDO'
#executeTest 'testSystemRestart -n SR_FULLDB'

# TESTS FINISHED
trace "Finished: `date`"

#
# TEST SUMMARY
#
if [ $numOfTestsFailed -eq 0 ] 
then
    echo "-- REGRESSION TEST SUCCESSFUL --"
else
    echo "-- REGRESSION TEST FAILED!! --"
fi
echo "Number of successful tests: $numOfTestsOK"
echo "Number of failed tests    : $numOfTestsFailed"
