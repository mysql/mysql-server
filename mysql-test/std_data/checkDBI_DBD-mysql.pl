#!/usr/bin/perl

# Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; version 2
# of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA


################################################################################
#
# This perl script checks for availability of the Perl modules DBI and
# DBD::mysql using the "current" perl interpreter.
#
# Useful for test environment checking before testing executable perl scripts
# in the MySQL Server distribution.
#
# NOTE: The "shebang" on the first line of this script should always point to
#       /usr/bin/perl, so that we can use this script to check whether or not we
#       support running perl scripts with such a shebang without specifying the
#       perl interpreter on the command line. Such a script is mysqlhotcopy.
#
#       When run as "checkDBI_DBD-mysql.pl" the shebang line will be evaluated
#       and used. When run as "perl checkDBI_DBD-mysql.pl" the shebang line is
#       not used.
#
# NOTE: This script will create a temporary file in MTR's tmp dir.
#       If modules are found, a mysql-test statement which sets a special
#       variable is written to this file. If one of the modules is not found
#       (or cannot be loaded), the file will remain empty.
#       A test (or include file) which sources that file can then easily do
#       an if-check on the special variable to determine success or failure.
#
#       Example:
#
#         --let $perlChecker= $MYSQLTEST_VARDIR/std_data/checkDBI_DBD-mysql.pl
#         --let $resultFile= $MYSQL_TMP_DIR/dbidbd-mysql.txt
#         --chmod 0755 $perlChecker
#         --exec $perlChecker
#         --source $resultFile
#         if (!$dbidbd) {
#             --skip Test needs Perl modules DBI and DBD::mysql
#         } 
#
#       The calling script is also responsible for cleaning up after use:
#
#         --remove_file $resultFile
#
# Windows notes: 
#   - shebangs may work differently - call this script with "perl " in front.
#
# See mysql-test/include/have_dbi_dbd-mysql.inc for example use of this script.
# This script should be executable for the user running MTR.
#
################################################################################

BEGIN {
    # By using eval inside BEGIN we can suppress warnings and continue after.
    # We need to catch "Can't locate" as well as "Can't load" errors.
    eval{
        $FOUND_DBI=0;
        $FOUND_DBD_MYSQL=0;

        # Check for DBI module:
        $FOUND_DBI=1 if require DBI;

        # Check for DBD::mysql module
        $FOUND_DBD_MYSQL=1 if require DBD::mysql;
    };
};

# Open a file to be used for transfer of result back to mysql-test.
# The file must be created whether we write to it or not, otherwise mysql-test 
# will complain if trying to source it. 
# An empty file indicates failure to load modules.
open(FILE, ">", $ENV{'MYSQL_TMP_DIR'}.'/dbidbd-mysql.txt');

if ($FOUND_DBI && $FOUND_DBD_MYSQL) {
    # write a mysql-test command setting a variable to indicate success
    print(FILE 'let $dbidbd= FOUND_DBI_DBD-MYSQL;'."\n");
}

# close the file.
close(FILE);

1;

