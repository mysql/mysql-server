#
# Check if we have a working DNS.
# Some 'wildcard dns' return some address even for non-existing hosts. This
# makes it hard to test connections to such host names.
# The actual check for working DNS is done in Perl, and the result available
# in an environment variable.
#
--require r/have_working_dns.require
disable_query_log;
eval SELECT LENGTH('$HAVE_BROKEN_DNS') = 0 AS 'have_working_dns';
enable_query_log;
