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
#    the test will abort immediate.

perl;
    use strict;
    my $search_file=           $ENV{'SEARCH_FILE'}           or die "SEARCH_FILE not set";
    my $search_pattern=        $ENV{'SEARCH_PATTERN'}        or die "SEARCH_PATTERN not set";
    open(FILE, "$search_file") or die("Unable to open '$search_file': $!\n");
    while (<FILE>) {
	if (m{$search_pattern}) {
	    print "Pattern \"$search_pattern\" found\n";
	    close(FILE);
	    exit;
	}
    }

    close(FILE);
    print "Pattern \"$search_pattern\" not found\n";
EOF
