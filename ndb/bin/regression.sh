#!/bin/sh
# NAME
#   regression.sh
# 
# SYNOPSIS
#   regression.sh
#
# DESCRIPTION
#
# This script runs a number of regression tests to verify that nothing
# is broken. Currently it executes the same tests as in the autotest
# regression suite.
#
# OPTIONS
# 
# EXAMPLES
#   
#   
# ENVIRONMENT
#   verbose             verbose printouts
#
# FILES
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


# die prints the supplied message to stderr,
# prefixed with the program name, and exits 
# with the exit code given by "-e num" or 
# 1, if no -e option is present.
#
die ()
{
        die_code__=1
	[ "X$1" = X-e ]  &&  { die_code__=$2; shift 2; }
	[ "X$1" = X-- ]  &&  shift 
	errmsg "$@"
	exit $die_code__
}


# msg prints the supplied message to stderr,
# prefixed with the program name.
#
errmsg ()
{
	echo "${progname:-<no program name set>}:" "$@" >&2
}

# rawdie prints the supplied message to stderr.
# It then exits with the exit code given with "-e num"
# or 1, if no -e option is present.
#
rawdie ()
{
        rawdie_code__=1
	[ "X$1" = X-e ]  &&  { rawdie_code__=$2; shift 2; }
	[ "X$1" = X-- ]  &&  shift 
	rawerrmsg "$@"
	exit $rawdie_code__
}

# Syndie prints the supplied message (if present) to stderr,
# prefixed with the program name, on the first line.
# On the second line, it prints $synopsis.
# It then exits with the exit code given with "-e num"
# or 1, if no -e option is present.
#
syndie ()
{
        syndie_code__=1
	[ "X$1" = X-e ]  &&  { syndie_code__=$2; shift 2; }
	[ "X$1" = X-- ]  &&  shift 
	[ -n "$*" ] && msg "$*"
	rawdie -e $syndie_code__  "Synopsis: $synopsis"
}




# msg prints the supplied message to stdout,
# prefixed with the program name.
#
msg ()
{
	echo "${progname:-<no program name set>}:" "$@" 
}

rawmsg () { echo "$*"; }  	# print the supplied message to stdout
rawerrmsg () { echo "$*" >&2; } # print the supplied message to stderr

# trace prints the supplied message to stdout if verbose is non-null
#
trace ()
{
    [ -n "$verbose" ]  &&  msg "$@"
}


# errtrace prints the supplied message to stderr if verbose is non-null
#
errtrace ()
{
    [ -n "$verbose" ]  &&  msg "$@" >&2
}


synopsis="regression.sh"
progname=`basename $0`

numOfTestsOK=0
numOfTestsFailed=0

LOG=regression-$1.`date '+%Y-%m-%d'`

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
trace "NDB_TOP = $NDB_TOP"

#
# THE TESTS TO EXECUTE
#

# BASIC FUNCTIONALITY
if [ $1 = "basic" ]
then
executeTest 'testBasic -n PkRead'
executeTest 'drop_all_tabs'

executeTest 'testBasic -n PkUpdate'
executeTest 'drop_all_tabs'

executeTest 'testBasic -n PkDelete'
executeTest 'drop_all_tabs'

executeTest 'testBasic -n PkInsert'
executeTest 'drop_all_tabs'

executeTest 'testBasic -n UpdateAndRead'
executeTest 'drop_all_tabs'

executeTest 'testBasic -n PkReadAndLocker' T6
executeTest 'drop_tab' T6

executeTest 'testBasic -n PkReadAndLocker2' T6
executeTest 'drop_tab' T6

executeTest 'testBasic -n PkReadUpdateAndLocker' T6
executeTest 'drop_tab' T6

executeTest 'testBasic -n ReadWithLocksAndInserts' T6
executeTest 'drop_tab' T6

executeTest 'testBasic -n PkInsertTwice' T1 T6 T10
executeTest 'drop_tab' T1 T6 T10

executeTest 'testBasic -n PkDirtyRead'
executeTest 'drop_all_tabs'
 
executeTest 'testBasic -n Fill' T6
executeTest 'drop_tab' T6

executeTest 'testBasic -n Fill' T1
executeTest 'drop_tab' T1

executeTest 'testBasic -n NoCommitSleep' T6
executeTest 'drop_tab' T6

executeTest 'testBasic -n NoCommit626' T6
executeTest 'drop_tab' T6

executeTest 'testBasic -n NoCommitAndClose' T6
executeTest 'drop_tab' T6

executeTest 'testBasic -n Commit626' T6
executeTest 'drop_tab' T6

executeTest 'testBasic -n CommitTry626' T6
executeTest 'drop_tab' T6

executeTest 'testBasic -n CommitAsMuch626' T6
executeTest 'drop_tab' T6

executeTest 'testBasic -n NoCommit626' T6
executeTest 'drop_tab' T6

executeTest 'testBasic -n NoCommitRollback626' T1 T6
executeTest 'drop_tab' T1 T6

executeTest 'testBasic -n Commit630' T1 T6
executeTest 'drop_tab' T6

executeTest 'testBasic -n CommitTry630' T1 T6
executeTest 'drop_tab' T1 T6

executeTest 'testBasic -n CommitAsMuch630' T1 T6
executeTest 'drop_tab' T1 T6

executeTest 'testBasic -n NoCommit630' T1 T6
executeTest 'drop_tab' T1 T6

executeTest 'testBasic -n NoCommitRollback630' T1 T6
executeTest 'drop_tab' T1 T6

executeTest 'testBasic -n NoCommitAndClose' T1 T6
executeTest 'drop_tab' T1 T6

executeTest 'testBasic -n RollbackUpdate' T1 T6
executeTest 'drop_tab' T1 T6

executeTest 'testBasic -n RollbackDeleteMultiple' T1 T6
executeTest 'drop_tab' T1 T6

executeTest 'testBasic -n ImplicitRollbackDelete' T1 T6
executeTest 'drop_tab' T1 T6

executeTest 'testBasic -n CommitDelete' T1 T6
executeTest 'drop_tab' T1 T6

executeTest 'testBasic -n RollbackNothing' T1 T6
executeTest 'drop_tab' T1 T6

executeTest 'testBasic -n ReadConsistency' T6
executeTest 'drop_tab' T6

executeTest 'testBasic -n PkRead' TPK_33 TPK_34 TPK_1003 TPK_2003 TPK_4092
executeTest 'drop_tab' TPK_33 TPK_34 TPK_1003 TPK_2003 TPK_4092

executeTest 'testBasic -n PkUpdate' TPK_33 TPK_34 TPK_1003 TPK_2003 TPK_4092
executeTest 'drop_tab' TPK_33 TPK_34 TPK_1003 TPK_2003 TPK_4092

executeTest 'testBasic -n PkDelete' TPK_33 TPK_34 TPK_1003 TPK_2003 TPK_4092
executeTest 'drop_tab' TPK_33 TPK_34 TPK_1003 TPK_2003 TPK_4092

executeTest 'testBasic -n PkInsert' TPK_33 TPK_34 TPK_1003 TPK_2003 TPK_409
executeTest 'drop_tab' TPK_33 TPK_34 TPK_1003 TPK_2003 TPK_4092

executeTest 'testBasic -n UpdateAndRead' TPK_33 TPK_34 TPK_1003 TPK_2003 TPK_4092
#executeTest 'drop_tab' TPK_33 TPK_34 TPK_1003 TPK_2003 TPK_4092

executeTest 'testBasicAsynch -n PkInsertAsynch'
executeTest 'drop_all_tabs' 

executeTest 'testBasicAsynch -n PkReadAsynch'
executeTest 'drop_all_tabs' 

executeTest 'testBasicAsynch -n PkUpdateAsynch'
executeTest 'drop_all_tabs' 

executeTest 'testBasicAsynch -n PkDeleteAsynch'
executeTest 'drop_all_tabs' 
fi

# SCAN TESTS
if [ $1 = "scan" ]
then
executeTest 'testScan -n ScanRead16'
executeTest 'drop_all_tabs'

executeTest 'testScan -n ScanRead240'
executeTest 'drop_all_tabs'

executeTest 'testScan -n ScanUpdate'
executeTest 'drop_all_tabs'

executeTest 'testScan -n ScanUpdate2' T6
executeTest 'drop_tab' T6

executeTest 'testScan -n ScanDelete'
executeTest 'drop_all_tab'

executeTest 'testScan -n ScanDelete2' T10
executeTest 'drop_tab' T10

executeTest 'testScan -n ScanUpdateAndScanRead' T6
executeTest 'drop_tab' T6

executeTest 'testScan -n ScanReadAndLocker' T6
executeTest 'drop_tab' T6

executeTest 'testScan -n ScanReadAndPkRead' T6
executeTest 'drop_tab' T6

executeTest 'testScan -n ScanRead488' T6
executeTest 'drop_tab' T6

executeTest 'testScan -n ScanWithLocksAndInserts' T6
executeTest 'drop_tab' T6

executeTest 'testScan -n ScanReadAbort' T6
executeTest 'drop_tab' T6

executeTest 'testScan -n ScanReadAbort15' T6
executeTest 'drop_tab' T6

executeTest 'testScan -n ScanReadAbort240' T6
executeTest 'drop_tab' T6

executeTest 'testScan -n ScanUpdateAbort16' T6
executeTest 'drop_tab' T6

executeTest 'testScan -n ScanReadRestart' T6
executeTest 'drop_tab' T6

executeTest 'testScan -n ScanUpdateRestart' T6
executeTest 'drop_tab' T6

executeTest 'testScan -n CheckGetValue' T6
executeTest 'drop_tab' T6

executeTest 'testScan -n CloseWithoutStop' T6
executeTest 'drop_tab' T6

executeTest 'testScan -n NextScanWhenNoMore' T6
executeTest 'drop_tab' T6

executeTest 'testScan -n ExecuteScanWithoutOpenScan' T6
executeTest 'drop_tab' T6

executeTest 'testScan -n OnlyOpenScanOnce' T6
executeTest 'drop_tab' T6

executeTest 'testScan -n OnlyOneOpInScanTrans' T6
executeTest 'drop_tab' T6

executeTest 'testScan -n OnlyOneOpBeforeOpenScan' T6
executeTest 'drop_tab' T6

executeTest 'testScan -n OnlyOneScanPerTrans' T6
executeTest 'drop_tab' T6

executeTest 'testScan -n NoCloseTransaction' T6
executeTest 'drop_tab' T6

executeTest 'testScan -n CheckInactivityTimeOut' T6
executeTest 'drop_tab' T6

executeTest 'testScan -n CheckInactivityBeforeClose' T6
executeTest 'drop_tab' T6

executeTest 'testScan -n CheckAfterTerror' T6
executeTest 'drop_tab' T6
fi


# DICT TESTS
if [ $1 = "dict" ]
then
executeTest 'testDict -n CreateAndDrop'
executeTest 'drop_all_tabs' 

executeTest 'testDict -n CreateAndDropWithData'
executeTest 'drop_all_tabs' 

executeTest 'testDict -n CreateAndDropDuring' T6
executeTest 'drop_tab' T6 

executeTest 'testDict -n CreateInvalidTables'  
executeTest 'drop_all_tabs' 

executeTest 'testDict -n CreateTableWhenDbIsFull' T6
executeTest 'drop_tab' T6
 
executeTest 'testDict -n CreateMaxTables' T6
executeTest 'drop_tab' T6

executeTest 'testDict -n FragmentTypeAll' T1 T6 T7 T8
executeTest 'drop_tab' T1 T6 T7 T8
 
executeTest 'testDict -n FragmentTypeAllLarge' T1 T6 T7 T8
executeTest 'drop_tab' T1 T6 T7 T8

executeTest 'testDict -n TemporaryTables' T1 T6 T7 T8
executeTest 'drop_tab' T1 T6 T7 T8
fi

# TEST NDBAPI
if [ $1 = "api" ]
then
executeTest 'testNdbApi -n MaxNdb' T6
executeTest 'drop_tab' T6

executeTest 'testNdbApi -n MaxTransactions' T1 T6 T7 T8 T13
executeTest 'drop_tab' T1 T6 T7 T8 T13

executeTest 'testNdbApi -n MaxOperations' T1 T6 T7 T8 T1
executeTest 'drop_tab' T1 T6 T7 T8 T13

executeTest 'testNdbApi -n MaxGetValue' T1 T6 T7 T8 T13
executeTest 'drop_tab' T1 T6 T7 T8 T13

executeTest 'testNdbApi -n MaxEqual' 
executeTest 'drop_all_tabs'

executeTest 'testNdbApi -n DeleteNdb' T1 T6
executeTest 'drop_tab' T1 T6

executeTest 'testNdbApi -n WaitUntilReady' T1 T6 T7 T8 T13
executeTest 'drop_tab' T1 T6 T7 T8 T13

executeTest 'testNdbApi -n GetOperationNoTab' T6
executeTest 'drop_tab' T6

executeTest 'testNdbApi -n NdbErrorOperation' T6
executeTest 'drop_tab' T6

executeTest 'testNdbApi -n MissingOperation' T6
executeTest 'drop_tab' T6

executeTest 'testNdbApi -n GetValueInUpdate' T6
executeTest 'drop_tab' T6

executeTest 'testNdbApi -n UpdateWithoutKeys' T6
executeTest 'drop_tab' T6

executeTest 'testNdbApi -n UpdateWithoutValues' T6
executeTest 'drop_tab' T6

executeTest 'testOperations -n ReadRead' T6
executeTest 'drop_tab' T6

executeTest 'testOperations -n ReadReadEx' T6
executeTest 'drop_tab' T6

executeTest 'testOperations -n ReadInsert' T6
executeTest 'drop_tab' T6

executeTest 'testOperations -n ReadUpdate' T6
executeTest 'drop_tab' T6

executeTest 'testOperations -n ReadDelete' T6
executeTest 'drop_tab' T6

executeTest 'testOperations -n ReadExRead' T6
executeTest 'drop_tab' T6

executeTest 'testOperations -n ReadExReadEx' T6
executeTest 'drop_tab' T6

executeTest 'testOperations -n ReadExInsert' T6
executeTest 'drop_tab' T6

executeTest 'testOperations -n ReadExUpdate' T6
executeTest 'drop_tab' T6

executeTest 'testOperations -n ReadExDelete' T6
executeTest 'drop_tab' T6

executeTest 'testOperations -n InsertRead' T6
executeTest 'drop_tab' T6

executeTest 'testOperations -n InsertReadEx' T6
executeTest 'drop_tab' T6

executeTest 'testOperations -n InsertInsert' T6
executeTest 'drop_tab' T6

executeTest 'testOperations -n InsertUpdate' T6
executeTest 'drop_tab' T6

executeTest 'testOperations -n InsertDelete' T6
executeTest 'drop_tab' T6

executeTest 'testOperations -n UpdateRead' T6
executeTest 'drop_tab' T6

executeTest 'testOperations -n UpdateReadEx' T6
executeTest 'drop_tab' T6

executeTest 'testOperations -n UpdateInsert' T6
executeTest 'drop_tab' T6

executeTest 'testOperations -n UpdateUpdate' T6
executeTest 'drop_tab' T6

executeTest 'testOperations -n UpdateDelete' T6
executeTest 'drop_tab' T6

executeTest 'testOperations -n DeleteRead' T6
executeTest 'drop_tab' T6

executeTest 'testOperations -n DeleteReadEx' T6
executeTest 'drop_tab' T6

executeTest 'testOperations -n DeleteInsert' T6
executeTest 'drop_tab' T6

executeTest 'testOperations -n DeleteUpdate' T6
executeTest 'drop_tab' T6

executeTest 'testOperations -n DeleteDelete' T6
executeTest 'drop_tab' T6

executeTest 'testRestartGci' T6
executeTest 'drop_tab' T6

executeTest 'testIndex -n CreateAll' 
executeTest 'drop_all_tabs' 

executeTest 'testIndex -n InsertDeleteGentle' T1 T6 T8 T10
executeTest 'drop_tab' T1 T6 T8 T10

executeTest 'testIndex -n InsertDelete' T1 T6 T8 T10
executeTest 'drop_tab' T1 T6 T8 T10

executeTest 'testIndex -n CreateLoadDropGentle' T1 T6 T8 T10
executeTest 'drop_tab' T1 T6 T8 T10

executeTest 'testIndex -n CreateLoadDrop' T1 T6 T8 T10
executeTest 'drop_tab' T1 T6 T8 T10

executeTest 'testBackup' -n BackupOne 

executeTest 'testBackup' -n BackupBank T6
executeTest 'drop_tab' T6
fi

# TEST SYSTEM RESTARTS
if [ $1 = "sr" ]
then
executeTest 'testSystemRestart -n SR1' T1 
executeTest 'testSystemRestart -n SR1' T6 
executeTest 'testSystemRestart -n SR1' T7 
executeTest 'testSystemRestart -n SR1' T8
executeTest 'testSystemRestart -n SR1' T10 
executeTest 'testSystemRestart -n SR2' T1
executeTest 'testSystemRestart -n SR2' T6
executeTest 'testSystemRestart -n SR2' T7
executeTest 'testSystemRestart -n SR2' T10
executeTest 'testSystemRestart -n SR2' T13
executeTest 'testSystemRestart -n SR3' T6
executeTest 'testSystemRestart -n SR3' T10
executeTest 'testSystemRestart -n SR4' T6
executeTest 'testSystemRestart -n SR_UNDO' T1
executeTest 'testSystemRestart -n SR_UNDO' T6
executeTest 'testSystemRestart -n SR_UNDO' T7
executeTest 'testSystemRestart -n SR_UNDO' T8
executeTest 'testSystemRestart -n SR_UNDO' T10
executeTest 'drop_tab' T1 T6 T7 T8 T10 
fi

# TEST NODE RESTARTS
if [ $1 = "nr" ]
then
executeTest 'testNodeRestart -n NoLoad' T6 T8 T13
executeTest 'drop_tab' T6 T8 T13

executeTest 'testNodeRestart -n PkRead' T6 T8 T13
executeTest 'drop_tab' T6 T8 T13

executeTest 'testNodeRestart -n PkReadPkUpdate' T6 T8 T13
executeTest 'drop_tab' T6 T8 T13

executeTest 'testNodeRestart -n ReadUpdateScan' T6 T8 T13
executeTest 'drop_tab' T6 T8 T13

executeTest 'testNodeRestart -n Terror' T6 T13
executeTest 'drop_tab' T6 T8 T13

executeTest 'testNodeRestart -n FullDb' T6 T13
executeTest 'drop_tab' T6 T8 T13

executeTest 'testNodeRestart -n RestartRandomNode' T6 T13
executeTest 'drop_tab' T6 T8 T13

executeTest 'testNodeRestart -n RestartRandomNodeError' T6 T13
executeTest 'drop_tab' T6 T8 T13

executeTest 'testNodeRestart -n RestartRandomNodeInitial' T6 T13 
executeTest 'drop_tab' T6 T8 T13

executeTest 'testNodeRestart -n RestartNFDuringNR' T6 T13 
executeTest 'drop_tab' T6 T8 T13

executeTest 'testNodeRestart -n RestartNodeDuringLCP' T6 T13 
executeTest 'drop_tab' T6 T8 T13

executeTest 'testNodeRestart -n RestartMasterNodeError' T6 T8 T13
executeTest 'drop_tab' T6 T8 T13

executeTest 'testNodeRestart -n TwoNodeFailure' T6 T8 T13
executeTest 'drop_tab' T6 T8 T13

executeTest 'testNodeRestart -n TwoMasterNodeFailure' T6 T8 T13
executeTest 'drop_tab' T6 T8 T13

executeTest 'testNodeRestart -n FiftyPercentFail' T6 T8 T13
executeTest 'drop_tab' T6 T8 T13

executeTest 'testNodeRestart -n RestartAllNodes' T6 T8 T13
executeTest 'drop_tab' T6 T8 T13

executeTest 'testNodeRestart -n RestartAllNodesAbort' T6 T8 T13
executeTest 'drop_tab' T6 T8 T13

executeTest 'testNodeRestart -n RestartAllNodesError9999' T6 T8 T13
executeTest 'drop_tab' T6 T8 T13

executeTest 'testNodeRestart -n FiftyPercentStopAndWait' T6 T8 T13
executeTest 'drop_tab' T6 T8 T13

fi

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
