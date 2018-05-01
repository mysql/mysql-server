# Purpose:
#    Simple search with Perl for a pattern in some file.
#
#    The advantages compared to thinkable auxiliary constructs using the
#    mysqltest language and SQL are:
#    1. We do not need a running MySQL server.
#    2. SQL causes "noise" during debugging and increases the size of logs.
#       Perl code does not disturb at all.
#
#    The environment variables SEARCH_FILE and SEARCH_PATTERN must be set
#    before sourcing this routine.
#
#    In case of
#    - SEARCH_FILE and/or SEARCH_PATTERN is not set
#    - SEARCH_FILE cannot be opened
#    - SEARCH_FILE does not contain SEARCH_PATTERN
#    the test will abort immediate.
#    MTR will report something like
#    ....
#    worker[1] Using MTR_BUILD_THREAD 300, with reserved ports 13000..13009
#    main.1st                                 [ pass ]      3
#    innodb.innodb_page_size_func             [ fail ]
#            Test ended at 2011-11-11 18:15:58
#
#    CURRENT_TEST: innodb.innodb_page_size_func
#    # ERROR: The file '<name>' does not contain the expected pattern  <pattern>
#    mysqltest: At line 55: command "perl" failed with error 255. my_errno=175
#    In included file ./include/search_pattern_multiline.inc: 55
#    included from ./mysql-test/suite/innodb/t/innodb_page_size_func.test: 202
#
#    The result from queries just before the failure was:
#    ...
#    - saving '<some path>' to '<some path>'
#    main.1st                                 [ pass ]      2
#
# Typical use case (check invalid server startup options):
#    let $error_log= $MYSQLTEST_VARDIR/log/my_restart.err;
#    --error 0,1
#    --remove_file $error_log
#    let SEARCH_FILE= $error_log;
#    # Stop the server
#    let $restart_file= $MYSQLTEST_VARDIR/tmp/mysqld.1.expect;
#    --exec echo "wait" > $restart_file
#    --shutdown_server 
#    --source include/wait_until_disconnected.inc
#
#    --error 1
#    --exec $MYSQLD_CMD <whatever wrong setting> > $error_log 2>&1
#    # The server restart aborts
#    let SEARCH_PATTERN= \[ERROR\] Aborting;
#    --source include/search_pattern_multiline.inc
#
# Created:  2011-11-11 mleich

perl;
    use strict;
    my $search_file=           $ENV{'SEARCH_FILE'}           or die "SEARCH_FILE not set";
    my $search_pattern=        $ENV{'SEARCH_PATTERN'}        or die "SEARCH_PATTERN not set";
    my $inverse_search=        $ENV{'INVERSE_SEARCH'} || 0;
    my $file_content=          "";
    my $found=                  0;
    open(FILE, "$search_file") or die("Unable to open '$search_file': $!\n");

    my $scalar_content= join("", <FILE>);
    if ( ($inverse_search == 0) && (not $scalar_content =~ m{$search_pattern}) ) {
           die("# ERROR: The file '$search_file' does not contain the expected pattern  $search_pattern\n->$scalar_content<-\n");
    }

    if ( ($inverse_search == 1) && ($scalar_content =~ m{$search_pattern}) ) {
           die("# ERROR: The file '$search_file' contains the prohibited pattern $search_pattern\n->$file_content<-\n");
    }
    close(FILE);
EOF
