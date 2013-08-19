# Copyright (c) 2012, Oracle and/or its affiliates. All rights
# reserved.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; version 2 of
# the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
# 02110-1301  USA
# This include file contains perl code to execute testing of startup options 
# with bootstrap in my.cnf.
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
use strict;
use lib "include";
use server_option_test;
init_bootstrap() if not -d "$ENV{'MYSQLTEST_VARDIR'}/tmp/bootstrap";
my_cnf_bootstrap("$ENV{'TEST_OPTION'}","$ENV{'ADDITIONAL_OPTIONS'}");
my $found= check_bootstrap_log("$ENV{'LOG_PATTERN'}");
printf "Additional options: %-60s\n","$ENV{'ADDITIONAL_OPTIONS'}" if $ENV{'ADDITIONAL_OPTIONS'} ne "";
printf "%-60s %-20s". ($found==1 ? " passed\n" : " failed\n"), "'$ENV{'TEST_OPTION'}'", "in my.cnf:";
EOF

