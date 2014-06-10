# This include file contains perl code to execute testing of startup options 
# with bootstrap on command line.
# Creator: Horst Hunger
# Date:    2012-09-17
#
# This is a dummy sql statement as mtr let the test fails without query.
--disable_query_log
--disable_result_log
SELECT 1 ;
--enable_query_log
--enable_result_log
perl;
use lib "include";
use server_option_test;
use strict;
init_bootstrap() if not -d "$ENV{'MYSQLTEST_VARDIR'}/tmp/bootstrap";
commandline_bootstrap("$ENV{'TEST_OPTION'}","$ENV{'ADDITIONAL_OPTIONS'}");
my $found= check_bootstrap_log("$ENV{'LOG_PATTERN'}");
printf "Additional options: %-60s\n","$ENV{'ADDITIONAL_OPTIONS'}" if $ENV{'ADDITIONAL_OPTIONS'} ne "";
printf "%-60s %-20s". ($found==1 ? " passed\n" : " failed\n"), "'$ENV{'TEST_OPTION'}'", "on commandline:";
EOF

