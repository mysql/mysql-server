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
#    innodb.innodb_page_size                  [ fail ]
#            Test ended at 2011-11-11 18:15:58
#
#    CURRENT_TEST: innodb.innodb_page_size
#    # ERROR: The file '<name>' does not contain the expected pattern  <pattern>
#    mysqltest: In included file "./include/search_pattern.inc":
#    included from ./include/search_pattern_in_file.inc at line 36:
#    At line 25: command "perl" failed with error 255. my_errno=175
#

perl;
    use strict;
    my $search_file=           $ENV{'SEARCH_FILE'}           or die "SEARCH_FILE not set";
    my $search_pattern=        $ENV{'SEARCH_PATTERN'}        or die "SEARCH_PATTERN not set";
    open(FILE, "$search_file") or die("Unable to open '$search_file': $!\n");
    read(FILE, my $file_content, 50000, 0);
    close(FILE);
    if ( not $file_content =~ m{$search_pattern} ) {
       print "Pattern \"$search_pattern\" not found\n";
    } else {
       print "Pattern \"$search_pattern\" found\n";
    }
EOF
