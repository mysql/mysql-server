include .defs.mk


ifeq ($(NDB_OS), OSE)
DIRS = basic flexBench flexAsynch
else
DIRS = lmc-bench bank
BIN_DIRS = 			\
	flexAsynch      \
	flexBench       \
	flexHammer	\
	flexScan        \
	flexTT          \
        nodeRecovery    \
	restartgci  	\
	create_tab	\
	create_all_tabs \
	drop_all_tabs	\
	bulk_copy	\
	restarter2 restarter \
	restarts testScan testNdbApi	\
	testScanInterpreter testIndex \
	testInterpreter \
	testOIBasic \
	testBackup \
	testBasic		\
        basicAsynch     \
	testNodeRestart \
	testOperations testTransactions \
	testSystemRestart \
	testTimeout \
	testMgm  \
	testRestartGci	\
	testDataBuffers \
	testDict	\
        initronja 	\
	benchronja 	\
        acid 		\
	interpreterInTup \
	telco       \
        indexTest       \
	test_event      \
        indexTest2      \
	testGrep                
#	tables 		\

ifeq ($(NDB_OS), SOLARIS)
ifeq ($(NDB_COMPILER), FORTE6)
	DIRS += flexTimedAsynch
endif
endif
endif

include ${NDB_TOP}/Epilogue.mk

