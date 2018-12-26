ATRT_USAGE_README.txt
Author: Serge Kozlov, MySQL
Date: 03/23/2006

Contents

1. How to run
2. Results
3. ATRT Command Line Options

Note: how to setup ATRT please look ATRT_SETUP_README.txt
  
  1.How to run.
  =============

Simple way to start atrt:

	atrt --testcase-file=atrt_test_case_file

Command line above doesn't produce any log files. Better use following :

	atrt --log-file=log.txt --testcase-file=atrt_test_case_file

Now we can look log.txt for investigation any issues. If it is insufficiently
then add one or more -v arguments:

	atrt -v -v --log-file=log.txt --testcase-file=atrt_test_case_file

If the test case file contains two or more test we can add -r options for 
preventing stopping testing if one test fails (like --force for mysql-test-run)

	atrt -v -v -r --log-file=log.txt --testcase-file=atrt_test_case_file

The line below is optimal solution for testing:

	atrt -v -v -r -R --log-file=log.txt --testcase-file=atrt_test_case_file

All additional command line arguments and description of used in exampes above 
available in this document in section atrt command line options

  2.Results.
  ==========

  Unlike mysql-test-run frame work atrt doesn't inform to console passed/failed 
status of tests. You need to use --log-file option and look into log file for 
getting information about status of tests. When atrt finished you can look into 
log file defined --log-file option. It's main source of information about how 
were performed atrt tests. Below added the examples of content of log-file for 
different failures (except example 1 for passed test). Examples include 
probable cases with reasons and recommended solutions and cover not test issues 
only but mistakes of atrt configuration or cluster settings. As ATRT testcase 
file used simple test included in MySQL installation:

	max-time: 600
	cmd: testBasic
	args: -n PkRead? T1


Of course these examples don't cover all possible failures but at least most 
probable and often appearing.
Note: Before start atrt I recommend try to run selected cluster configuration 
by manual and make sure that it can work: enough to run mgmd, ndbd, mysqld and 
look via mgm to status of these nodes

Example 1. Test passed

2006-03-02 15:36:51 [ndb_atrt] INFO     -- Setup path not specified, using /home/ndbdev/skozlov/asetup
2006-03-02 15:36:51 [ndb_atrt] INFO     -- Starting...
2006-03-02 15:36:51 [ndb_atrt] INFO     -- Connecting to hosts
2006-03-02 15:36:51 [ndb_atrt] DEBUG    -- Connected to node1:1234
2006-03-02 15:36:51 [ndb_atrt] DEBUG    -- Connected to node2:1234
2006-03-02 15:36:51 [ndb_atrt] DEBUG    -- Connected to node3:1234
2006-03-02 15:36:55 [ndb_atrt] INFO     -- (Re)starting ndb processes
2006-03-02 15:37:11 [ndb_atrt] INFO     -- Ndb start completed
2006-03-02 15:37:11 [ndb_atrt] INFO     -- #1 - testBasic -n PkRead? T1
2006-03-02 16:37:16 [ndb_atrt] INFO     -- #1 OK (0)

Example 2. Test failed.
Reason: ATRT not started properly. d.txt not found. Probably necessary folders 
and d.txt file don't exist.
Solution: run make-config.sh d.tmp and try again.

2006-03-02 18:32:08 [ndb_atrt] INFO     -- Setup path not specified, using /home/ndbdev/skozlov/asetup
2006-03-02 18:32:08 [ndb_atrt] INFO     -- Starting...
2006-03-02 18:32:08 [ndb_atrt] CRITICAL -- Failed to open process config file: d.txt

Example 3. Test failed.
Reason: ATRT not started properly. Necessary folders were removed but d.txt file 
exists.
Solution: run make-config.sh d.tmp and try again.

2006-03-02 18:30:54 [ndb_atrt] INFO     -- Setup path not specified, using /home/ndbdev/skozlov/asetup
2006-03-02 18:30:54 [ndb_atrt] INFO     -- Starting...
2006-03-02 18:30:54 [ndb_atrt] INFO     -- Connecting to hosts
2006-03-02 18:30:54 [ndb_atrt] DEBUG    -- Connected to node1:1234
2006-03-02 18:30:54 [ndb_atrt] DEBUG    -- Connected to node2:1234
2006-03-02 18:30:54 [ndb_atrt] DEBUG    -- Connected to node3:1234
2006-03-02 18:30:58 [ndb_atrt] INFO     -- (Re)starting ndb processes
2006-03-02 18:31:00 [ndb_atrt] CRITICAL -- Failed to setup process

Example 4. Test failed.
Reason: ATRT not started properly. node2 hasn't running ndb_cpcd process.
Solution: log into node2 and starts ndb_cpcd process.

2006-03-02 18:15:05 [ndb_atrt] INFO     -- Setup path not specified, using /home/ndbdev/skozlov/asetup
2006-03-02 18:15:05 [ndb_atrt] INFO     -- Starting...
2006-03-02 18:15:05 [ndb_atrt] INFO     -- Connecting to hosts
2006-03-02 18:15:05 [ndb_atrt] DEBUG    -- Connected to node1:1234
2006-03-02 18:15:05 [ndb_atrt] ERROR    -- Unable to connect to cpc node2:1234

Example 5. Test failed.
Reason: ATRT not started properly. baseport option isn't defined in d.tmp or 
baseport and PortNumber are different.
Solution: correct d.tmp, run make-config.sh d.tmp and try again.

2006-03-02 18:25:31 [ndb_atrt] INFO     -- Setup path not specified, using /home/ndbdev/skozlov/asetup
2006-03-02 18:25:31 [ndb_atrt] INFO     -- Starting...
2006-03-02 18:25:31 [ndb_atrt] INFO     -- Connecting to hosts
2006-03-02 18:25:31 [ndb_atrt] DEBUG    -- Connected to node1:1234
2006-03-02 18:25:31 [ndb_atrt] DEBUG    -- Connected to node2:1234
2006-03-02 18:25:31 [ndb_atrt] DEBUG    -- Connected to node3:1234
2006-03-02 18:25:36 [ndb_atrt] INFO     -- (Re)starting ndb processes
2006-03-02 18:26:08 [ndb_atrt] CRITICAL -- Unable to connect to ndb mgm node1:0

Example 6. Test failed.
Reason: ATRT not started properly. basedir option points to wrong path.
Solution: correct basedir, run make-config.sh d.tmp and try again.

2006-03-02 18:40:10 [ndb_atrt] INFO     -- Setup path not specified, using /home/ndbdev/skozlov/asetup
2006-03-02 18:40:10 [ndb_atrt] INFO     -- Starting...
2006-03-02 18:40:10 [ndb_atrt] INFO     -- Connecting to hosts
2006-03-02 18:40:10 [ndb_atrt] DEBUG    -- Connected to node1:1234
2006-03-02 18:40:10 [ndb_atrt] DEBUG    -- Connected to node2:1234
2006-03-02 18:40:10 [ndb_atrt] DEBUG    -- Connected to node3:1234
2006-03-02 18:40:14 [ndb_atrt] INFO     -- (Re)starting ndb processes
2006-03-02 18:40:16 [ndb_atrt] ERROR    -- Unable to start process: Failed to start

Example 7. Test failed.
Reason: ndb nodes have problems on starting.
Solution: Check configuration of ndb nodes.

2006-03-02 18:46:44 [ndb_atrt] INFO     -- Setup path not specified, using /home/ndbdev/skozlov/asetup
2006-03-02 18:46:44 [ndb_atrt] INFO     -- Starting...
2006-03-02 18:46:44 [ndb_atrt] INFO     -- Connecting to hosts
2006-03-02 18:46:44 [ndb_atrt] DEBUG    -- Connected to node1:1234
2006-03-02 18:46:44 [ndb_atrt] DEBUG    -- Connected to node2:1234
2006-03-02 18:46:44 [ndb_atrt] DEBUG    -- Connected to node3:1234
2006-03-02 18:46:50 [ndb_atrt] INFO     -- (Re)starting ndb processes
2006-03-02 18:52:59 [ndb_atrt] CRITICAL -- wait ndb timed out 3 3 4
2006-03-02 18:58:59 [ndb_atrt] CRITICAL -- wait ndb timed out 3 3 4

Example 8. Test failed.
Reason: test application not found.
Solution: Correct file name in ATRT test case file and make sure that the file 
exists in $MYSQL_DIR/bin directory.

2006-03-02 20:21:54 [ndb_atrt] INFO     -- Starting...
2006-03-02 20:21:54 [ndb_atrt] INFO     -- Connecting to hosts
2006-03-02 20:21:54 [ndb_atrt] DEBUG    -- Connected to node1:1234
2006-03-02 20:21:54 [ndb_atrt] DEBUG    -- Connected to node2:1234
2006-03-02 20:21:54 [ndb_atrt] DEBUG    -- Connected to node3:1234
2006-03-02 20:21:57 [ndb_atrt] INFO     -- (Re)starting ndb processes
2006-03-02 20:22:12 [ndb_atrt] INFO     -- Ndb start completed
2006-03-02 20:22:12 [ndb_atrt] INFO     -- #1 - testBasic123 -n PkRead? T2
2006-03-02 20:22:13 [ndb_atrt] ERROR    -- Unable to start process: Failed to start

Example 9. Test failed.
Probable reasons:
* wrong arguments for test application
* itself test failed
* timeout reached
Solution: Since mgmd/ndbd nodes started properly in such case then try to 
investigate log files in result/X.api/, result/X.mysqld, result/X.mysql 
directories.

2006-03-02 19:59:35 [ndb_atrt] INFO     -- Setup path not specified, using /home/ndbdev/skozlov/asetup
2006-03-02 19:59:35 [ndb_atrt] INFO     -- Starting...
2006-03-02 19:59:35 [ndb_atrt] INFO     -- Connecting to hosts
2006-03-02 19:59:35 [ndb_atrt] DEBUG    -- Connected to node1:1234
2006-03-02 19:59:35 [ndb_atrt] DEBUG    -- Connected to node2:1234
2006-03-02 19:59:35 [ndb_atrt] DEBUG    -- Connected to node3:1234
2006-03-02 20:00:49 [ndb_atrt] INFO     -- (Re)starting ndb processes
2006-03-02 20:01:05 [ndb_atrt] INFO     -- Ndb start completed
2006-03-02 20:01:05 [ndb_atrt] INFO     -- #1 - testBasic -n PkRead? T1
2006-03-02 20:01:38 [ndb_atrt] INFO     -- #1 FAILED(256)


  3.ATRT Command Line Options.
  ============================

--process-config=string
    Specify ATRT configuration file. If not specified, ATRT will look in local 
    directory for the d.txt file.

--setup-path=string
    This path points to place where necessary directories (created by 
    make-config) can be found. Note: d.txt should be in same directory where 
    you start atrt!

-v (verbose)

    * without the option: atrt prints only [CRITICAL] events
    * one -v: atrt prints [CRITICAL], [INFO] events
    * two -v: atrt prints [CRITICAL], [INFO], [DEBUG] events

--log-file=string
    Used to specify file to log ATRT's results on starting application and 
    running tests.

--testcase-file=string -f
    Used to feed ATRT test cases in a text file.

--report-file=string
    File to record test results

-i, --interactive
    ATRT terminates on first test failure

-r, --regression
    Continues even on test failures

-b, --bench
    Always produce report

