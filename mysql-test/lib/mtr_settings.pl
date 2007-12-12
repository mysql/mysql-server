#
# Defines used to control how mysql-test-run.pl
# should work in the current version of MySQL
#

# Control if the suite name should be output before testname
sub SHOW_SUITE_NAME { return 1; };


# Control which suites are run by default
sub DEFAULT_SUITES { return "main,binlog,federated,rpl,rpl_ndb,ndb"; };



1;

