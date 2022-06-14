# ==== Purpose ====
#
# Grep an expression for a pattern, produce a single string out
# depending whether the match was found or not.
#
# The environment variables SEARCH_EXPRESSION and SEARCH_PATTERN
# must be set before sourcing this routine.
#
# ==== Usage ====
#
# --let SEARCH_EXPRESSION= TEXT
# --let SEARCH_PATTERN= TEXT
# --source include/search_pattern_on_variable.inc

perl;
    use strict;
    my $search_expression= $ENV{'SEARCH_EXPRESSION'}    or die "SEARCH_EXPRESSION not set";
    my $search_pattern=    $ENV{'SEARCH_PATTERN'}       or die "SEARCH_PATTERN not set";
    if ($search_expression =~ /$search_pattern/)
    {
        print "Pattern \"$search_pattern\" found\n";
    }
    else
    {
        print "Pattern \"$search_pattern\" not found\n";
    }
EOF
