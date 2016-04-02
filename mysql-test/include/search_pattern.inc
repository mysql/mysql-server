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
    my $search_echo=           $ENV{'SEARCH_ECHO'}     || "NONE";
    my $ignore_pattern=        $ENV{'IGNORE_PATTERN'}  || '';
    my $search_echo_ctx=       $ENV{'SEARCH_ECHO_CTX'} || 0;

    open(FILE, "< $search_file") or die("Unable to open '$search_file': $!\n");
    my @lines= <FILE>;
    my $flag= 0;

    for (my $i= 0; $i<= $#lines; $i++)
    {
        my $line= $lines[$i];
	if ($line =~ /$search_pattern/)
        {
            if (!(length $ignore_pattern) or !($line =~ /$ignore_pattern/))
            {
                $flag= 1;
                if ($search_echo eq 'FIRST')
                {
                    print_previous_n_lines($i, $search_echo_ctx);
                    print_succeding_n_lines($i, $search_echo_ctx);
                    last;
                }
                elsif ($search_echo eq 'ALL')
                {
                    print_previous_n_lines($i, $search_echo_ctx);
                    print_succeding_n_lines($i, $search_echo_ctx);

                    # Separate each matching line
                    if ($search_echo_ctx > 0)
                    {
                        print "---\n";
                    }
                }
            }
	}
    }

    if ($flag == 1)
    {
        print "Pattern \"$search_pattern\" found\n";
    }
    else
    {
        print "Pattern \"$search_pattern\" not found\n";
    }
    close(FILE);

    sub print_previous_n_lines()
    {
        my ($i, $search_echo_ctx)= @_;

        for (my $j= 0; $j< $search_echo_ctx; $j++)
        {
            my $index= $i+$j-$search_echo_ctx;
            if ($index >= 0)
            {
                print "@{[$i+$j-$search_echo_ctx+1]}: $lines[$index]";
            }
        }
    }

    sub print_succeding_n_lines()
    {
        my ($i, $search_echo_ctx)= @_;

        for (my $j= 0; $j<= $search_echo_ctx; $j++)
        {
            if ($lines[$i+$j])
            {
                print "@{[$i+$j+1]}: $lines[$i+$j]";
             }
        }
    }

EOF

# Setting the env vars to default value
--let SEARCH_ECHO=     "NONE"
--let IGNORE_PATTERN=  ''
--let SEARCH_ECHO_CTX= 0
