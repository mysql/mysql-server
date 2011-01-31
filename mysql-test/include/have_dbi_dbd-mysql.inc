#
# Originally created by John Embretsen, 2011-01-26.
#
# Checks for the existence of Perl modules DBI and DBD::mysql as seen from the
# perl installation used by "external" executable perl scripts, i.e. scripts
# that are executed as standalone scripts interpreted by the perl installation
# specified by the "shebang" line in the top of these scripts.
#
# If either module is not found, the test will be skipped.
#
# For use in tests that call perl scripts that require these modules.
#
# This file is intended to work on Unix. Windows may need different treatment.
# Reasoning:
#   - "shebangs" are not relevant on Windows, but need to be handled here.
#   - Perl scripts cannot be made executable on Windows, interpreter must be 
#     specified.
#
# Note that if there are multiple perl installations and not all have the
# required modules, this check may fail even if the perl in path does have
# the modules available. This may happen if the perl specified by the script's 
# shebang (e.g. #!/usr/bin/perl) does not have these modules, and script is 
# called without specifying the perl interpreter. However, this will be 
# a correct result in cases where a test calls a script with a similar shebang.
#
################################################################################

--source include/not_windows.inc

# We jump through some hoops since there is no direct way to check if an 
# external command went OK or not from a mysql-test file:
#
#   - In theory, we could do as simple as "exec perl -MDBI -MDBD::mysql -e 1",
#     however we cannot check the result (exit code) from within a test script. 
#     Also, this may not yield the same result as other uses of perl due to the 
#     shebang issue mentioned above.
#   - Instead we use a separate helper perl script that checks for the modules.
#   - If the modules are found, the perl script leaves a file which sets a
#     variable that can be read by this file.
#     If the modules are not found, the perl script does not set this variable,
#     but leaves an empty file instead.
#
# This is done because there is apparently no direct way to transfer
# information from perl to the test script itself.

--disable_query_log
--disable_result_log
--disable_warnings

# We do not use embedded perl in this script because that would not have yielded
# correct results for a situation where an external Perl script is called like
# "scriptname" instead of "perl scriptname" and the shebang in the script points
# to a specific perl that may be different than the perl in PATH.
#
# Instead, we call a separate helper script which checks for the modules in its
# own environment. We call it without "perl" in front.

--let $perlChecker= $MYSQLTEST_VARDIR/std_data/checkDBI_DBD-mysql.pl
--let $resultFile= $MYSQL_TMP_DIR/dbidbd-mysql.txt

# Make the script executable and execute it.
--chmod 0755 $perlChecker
--exec $perlChecker

# Source the resulting temporary file and look for a variable being set.
--source $resultFile

if (!$dbidbd) {
    --skip Test needs Perl modules DBI and DBD::mysql
} 

# Clean up
--remove_file $resultFile

--enable_query_log
--enable_result_log
--enable_warnings

