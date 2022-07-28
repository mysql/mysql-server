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
#    - SEARCH_FILE does not contain SEARCH_PATTERN and PRINT_SEARCH_RESULT
#    is not set the test will abort immediate.
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

#    - PRINT_SEARCH_RESULT is set - the result of the search will
#    get printed.
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
    die "SEARCH_FILE not set" unless $ENV{'SEARCH_FILE'};
    my $print_search_result=   $ENV{PRINT_SEARCH_RESULT} or 0;
    my $abort_on=              $ENV{ABORT_ON} or "NOT_FOUND";
    my @search_files=          glob($ENV{'SEARCH_FILE'});
    my $search_pattern=        $ENV{'SEARCH_PATTERN'}        or die "SEARCH_PATTERN not set";
    my $inverse_search=        $ENV{'INVERSE_SEARCH'} || 0;
    my $file_content=          "";
    my $found=                  0;
    my $file_content=          "";
    my $found=                  0;
    foreach my $search_file (@search_files) {
       open(FILE, "$search_file") or die("Unable to open '$search_file': $!\n");
       while ( read(FILE, $file_content, 50000, 0) ) {
          if ( $file_content =~ m{$search_pattern} ) {
             $found= 1;
             last;
          }
       }
       close(FILE);
    }

    if ( $print_search_result == 0 ) {
      if ( $abort_on eq "NOT_FOUND" and $found == 0 ) {
        print join(":", @search_files);
        die ("# ERROR: None of the files contain the expected pattern $search_pattern\n");
      }
      if ( $abort_on eq "FOUND" and $found ) {
        die ("# ERROR: Some of the files contain pattern that we expected not to find $search_pattern\n");
      }
    } else {
       my $res=$found ? "FOUND" : "NOT FOUND";
       $ENV{SEARCH_FILE} =~ s{^.*?([^/\\]+)$}{$1};
       print "$res /$search_pattern/ in $ENV{SEARCH_FILE}\n";
    }

EOF
